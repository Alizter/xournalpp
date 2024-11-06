#include "StrokeViewHelper.h"

#include <glib.h>

#include "model/LineStyle.h"
#include "model/Point.h"
#include "util/Assert.h"
#include "util/LoopUtil.h"
#include "util/PairView.h"
#include "util/Util.h"  // for cairo_set_dash_from_vector

void xoj::view::StrokeViewHelper::pathToCairo(cairo_t* cr, const std::vector<Point>& pts) {
    for_first_then_each(
            pts, [cr](auto const& first) { cairo_move_to(cr, first.x, first.y); },
            [cr](auto const& other) { cairo_line_to(cr, other.x, other.y); });
}

/**
 * No pressure sensitivity, one line is drawn
 */
void xoj::view::StrokeViewHelper::drawNoPressure(cairo_t* cr, const std::vector<Point>& pts, const double strokeWidth,
                                                 const LineStyle& lineStyle, double dashOffset) {
    cairo_set_line_width(cr, strokeWidth);

    const auto& dashes = lineStyle.getDashes();
    Util::cairo_set_dash_from_vector(cr, dashes, dashOffset);

    pathToCairo(cr, pts);
    cairo_stroke(cr);
}

void draw_path_calligraphic(cairo_t* cr, cairo_path_t* path, double angle, double thickness, bool fill) {
    double last_move_x = 0., last_move_y = 0.;
    double current_point_x = 0., current_point_y = 0.;
    double x_shift = cos(angle) * thickness;
    double y_shift = sin(angle) * thickness;

    // Go through the path.  For each path segment, we draw a small rectangle.
    for (int i = 0; i < path->num_data; i += path->data[i].header.length) {
        cairo_path_data_t* data = &path->data[i];
        double x, y;
        switch (data->header.type) {
            case CAIRO_PATH_MOVE_TO:
                last_move_x = data[1].point.x;
                last_move_y = data[1].point.y;
                current_point_x = data[1].point.x;
                current_point_y = data[1].point.y;
                break;
            case CAIRO_PATH_LINE_TO:
            case CAIRO_PATH_CLOSE_PATH:
                if (data->header.type == CAIRO_PATH_LINE_TO) {
                    x = data[1].point.x;
                    y = data[1].point.y;
                } else {
                    x = last_move_x;
                    y = last_move_y;
                }
                cairo_move_to(cr, current_point_x + x_shift, current_point_y + y_shift);
                cairo_line_to(cr, current_point_x - x_shift, current_point_y - y_shift);
                cairo_line_to(cr, x - x_shift, y - y_shift);
                cairo_line_to(cr, x + x_shift, y + y_shift);
                cairo_close_path(cr);
                if (fill)
                    cairo_fill(cr);
                else
                    cairo_stroke(cr);

                current_point_x = x;
                current_point_y = y;
                break;
            case CAIRO_PATH_CURVE_TO:
                xoj_assert(0 && "curve to should not be present since we use cairo_copy_path_flat");
                break;
            default:
                xoj_assert(0 && "Unknown path command");
        }
    }
}

void stroke_calligraphic(cairo_t* cr, double angle, double thickness) {
    cairo_pattern_t* mask;
    cairo_path_t* path;

    // Get the current path. This uses _flat so that cairo replaces
    // cairo_curve_to() calls with many line_to()s.
    path = cairo_copy_path_flat(cr);
    cairo_new_path(cr);

    cairo_save(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    // I get antialiasing artifacts with OVER where two of the rectangles meet. ADD seems to make that problem go away.
    cairo_set_operator(cr, CAIRO_OPERATOR_ADD);

    // Redirect drawing to a temporary surface that we use to prepare the path.
    // This surface only has an alpha channel. It starts all transparent. The code
    // below draws to it, making it opaque where needed.
    cairo_push_group_with_content(cr, CAIRO_CONTENT_ALPHA);

    draw_path_calligraphic(cr, path, angle, thickness, true);

    // Now draw through the mask.
    // We used cairo_save() above. The cairo_restore() here now restores the
    // source that was set by the caller (e.g. through cairo_set_source_rgb).

    mask = cairo_pop_group(cr);
    cairo_restore(cr);
    cairo_mask(cr, mask);

    cairo_pattern_destroy(mask);
    cairo_path_destroy(path);
}

const double angle = M_PI / 8;
const double max_nib_thickness = 1.0;
const double min_nib_thickness = 0.2;

/**
 * Draw a stroke with pressure, for this multiple lines with different widths needs to be drawn
 */
double xoj::view::StrokeViewHelper::drawWithPressure(cairo_t* cr, const std::vector<Point>& pts,
                                                     const LineStyle& lineStyle, double dashOffset) {
    const auto& dashes = lineStyle.getDashes();

    /*
     * Because the width varies, we need to call cairo_stroke() once per segment
     */
    auto drawSegment = [cr](const Point& p, const Point& q) {
        xoj_assert(p.z > 0.0);
        // cairo_set_line_width(cr, p.z);
        cairo_move_to(cr, p.x, p.y);
        cairo_line_to(cr, q.x, q.y);
        // cairo_stroke(cr);
        double thickness = min_nib_thickness + (max_nib_thickness - min_nib_thickness) * p.z;
        stroke_calligraphic(cr, angle, thickness);
    };

    if (!dashes.empty()) {
        for (const auto& [p, q]: PairView(pts)) {
            Util::cairo_set_dash_from_vector(cr, dashes, dashOffset);
            dashOffset += p.lineLengthTo(q);
            drawSegment(p, q);
        }
    } else {
        cairo_set_dash(cr, nullptr, 0, 0.0);
        for (const auto& [p, q]: PairView(pts)) {
            drawSegment(p, q);
        }
    }
    return dashOffset;
}
