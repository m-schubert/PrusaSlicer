#include "FillHorizontalHoneycomb.hpp"
#include "../ExPolygon.hpp"
#include "../ClipperUtils.hpp"

namespace Slic3r {


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

    // Rotate the polygon, and calculate a bounding box.
    expolygon.rotate(direction.first, direction.second);
    BoundingBox bounding_box = expolygon.contour.bounding_box();
    
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

    // Generate lines for the hex sides
    Polylines lines;
    if (hex_fraction > 0.5) {
        // We're at the vertical sides.
        lines = _generate_single_lines(bounding_box, scale_(hex_width), scale_(offset));
    } else {
        // We're at the angled sides.
        coordf_t padding = hex_width * (0.5 - hex_fraction);

        if (2 * padding < this->spacing) {
            // Padding is small enough that we fall back to single lines.
            lines = _generate_single_lines(bounding_box, scale_(hex_width), scale_(offset));
        } else if (hex_width - 2 * padding < this->spacing) {
            // Padding is large enough that the split lines merge back into single lines at a different offset.
            offset = hex_width * ((hex_index + 1) % 2) / 2.0;
            lines = _generate_single_lines(bounding_box, scale_(hex_width), scale_(offset));
        } else {
            lines = _generate_split_lines(bounding_box, scale_(hex_width), scale_(padding), scale_(offset));
        }
    }

    // Clip lines to the expolygon.
    lines = intersection_pl(lines, expolygon);

    polylines_out = lines;
}

Polylines FillHorizontalHoneycomb::_generate_split_lines(
    const BoundingBox& bounding_box,
    coord_t center_spacing,
    coord_t padding,
    coord_t offset)
{
    Polylines left = _generate_single_lines(bounding_box, center_spacing, offset - padding);
    Polylines right = _generate_single_lines(bounding_box, center_spacing, offset + padding);

    Polylines both;
    both.reserve(left.size() + right.size());
    both.insert(both.end(), left.begin(), left.end());
    both.insert(both.end(), right.begin(), right.end());

    return both;
}

Polylines FillHorizontalHoneycomb::_generate_single_lines(
    const BoundingBox& bounding_box,
    coord_t center_spacing,
    coord_t offset)
{
    Polylines lines;

    BoundingBox aligned = bounding_box;
    aligned.merge(Point(_align_to_grid(bounding_box.min.x(), center_spacing, offset), aligned.min.y()));
    for (coord_t x = aligned.min.x(); x <= aligned.max.x(); x += center_spacing) {
        lines.push_back(Polyline(Point(x, aligned.min.y()), Point(x, aligned.max.y())));
    }

    return lines;
}

}
