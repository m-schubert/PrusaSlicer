#include "FillHorizontalHoneycomb.hpp"
#include "../ExPolygon.hpp"
#include "../ClipperUtils.hpp"

namespace Slic3r {


coord_t token_offset = scale_(1.0);

void FillHorizontalHoneycomb::_fill_surface_single(
    const FillParams                &params, 
    unsigned int                     thickness_layers,
    const std::pair<float, Point>   &direction, 
    ExPolygon                       &expolygon, 
    Polylines                       &polylines_out)
{
    // Calculate hexagon parameters.
    coordf_t hex_width = this->spacing / params.density * 1.5;
    coordf_t hex_side = hex_width / std::sqrt(3.0);

    // Calculate a bounding box and expand it by 1mm.
    BoundingBox bounding_box = expolygon.contour.bounding_box();
    bounding_box.offset(token_offset);
    
    // Determine whether we're on an odd or even hex, and how far through the hex we are.
    // Each hex is 2 * hex_side high, and there is 0.5 * hex_side shared between successive
    // rows of hexagon. We divide the z height into 1.5 * hex_side "rows".
    //
    // Thus, hex_index gives us the "row" index, and hex_fraction gives us the fraction of
    // the way through the row that we are, in a range of 0 to 1.5.
    coordf_t a = this->z / hex_side;
    size_t hex_index = static_cast<size_t>(a / 1.5);
    double hex_fraction = a - hex_index * 1.5;

    // Calculate an offset (between odd and even hexes).
    coordf_t offset = hex_width * (hex_index % 2) / 2.0;

    // Generate odd/even polygons for the hex sides
    std::pair<Polygon, Polygon> polygons;
    if (hex_fraction > 0.5) {
        // We're at the vertical sides.
        polygons = _generate_single_lines(bounding_box, scale_(hex_width), scale_(offset));
    } else {
        // We're at the angled sides.
        coordf_t padding = hex_width * (0.5 - hex_fraction);

        if (2 * padding < this->spacing) {
            // Padding is small enough that we fall back to single lines.
            polygons = _generate_single_lines(bounding_box, scale_(hex_width), scale_(offset));
        } else if (hex_width - 2 * padding < this->spacing) {
            // Padding is large enough that the split lines merge back into single lines at a different offset.
            offset = hex_width * ((hex_index + 1) % 2) / 2.0;
            polygons = _generate_single_lines(bounding_box, scale_(hex_width), scale_(offset));
        } else {
            polygons = _generate_split_lines(bounding_box, scale_(hex_width), scale_(padding), scale_(offset));
        }
    }
    
    // Clip lines to the expolygon.
    if (polygons.first.size() && polygons.second.size()) {
        //Polygons foo = (this->layer_id % 2) ? polygons.first : polygons.second;
        Polygons foo = intersection((this->layer_id % 2) ? polygons.first : polygons.second, expolygon);
        for (auto p = foo.begin(); p != foo.end(); ++p) {
            polylines_out.push_back(*p);
        }
    }
}

std::pair<Polygon, Polygon> FillHorizontalHoneycomb::_generate_split_lines(
    const BoundingBox& bounding_box,
    coord_t center_spacing,
    coord_t padding,
    coord_t offset)
{
    std::pair<Polygon, Polygon> polygons;

    Polygon* a = &polygons.first;
    Polygon* b = &polygons.second;
    
    BoundingBox aligned = bounding_box;
    aligned.merge(Point(_align_to_grid(bounding_box.min.x(), center_spacing * 2, offset), aligned.min.y()));

    coord_t x = aligned.min.x();
    coord_t y_top = aligned.max.y();
    coord_t y_bot = aligned.min.y();

    a->append(Point(x - padding, y_bot-token_offset));
    b->append(Point(x - padding, y_bot-token_offset));

    for (; x <= aligned.max.x(); x += center_spacing) {
        a->append({Point(x - padding, y_top), Point(x + padding, y_top), Point(x + padding, y_bot), Point(x + center_spacing - padding, y_bot)});
        b->append({Point(x - padding, y_bot), Point(x + padding, y_bot), Point(x + padding, y_top), Point(x + center_spacing - padding, y_top)});
    }

    a->append({Point(x - padding, y_top), Point(x + padding, y_top), Point(x + padding, y_bot-token_offset)});
    b->append({Point(x - padding, y_bot), Point(x + padding, y_bot), Point(x + padding, y_bot-token_offset)});

    return polygons;
}

std::pair<Polygon, Polygon> FillHorizontalHoneycomb::_generate_single_lines(
    const BoundingBox& bounding_box,
    coord_t center_spacing,
    coord_t offset)
{
    std::pair<Polygon, Polygon> polygons;

    Polygon* a = &polygons.first;
    Polygon* b = &polygons.second;
    
    BoundingBox aligned = bounding_box;
    aligned.merge(Point(_align_to_grid(bounding_box.min.x(), center_spacing * 2, offset), aligned.min.y()));

    coord_t x = aligned.min.x();
    coord_t y_top = aligned.max.y();
    coord_t y_bot = aligned.min.y();

    a->append(Point(x, y_bot-token_offset));
    b->append(Point(x, y_bot-token_offset));

    for (; x <= aligned.max.x(); x += center_spacing) {
        a->append({Point(x, y_top), Point(x + center_spacing, y_top)});
        b->append({Point(x, y_bot), Point(x + center_spacing, y_bot)});
        std::swap(a, b);
    }

    a->append(Point(x, y_bot-token_offset));
    b->append(Point(x, y_bot-token_offset));

    return polygons;
}

}
