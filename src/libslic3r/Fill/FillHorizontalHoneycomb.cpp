#include "FillHorizontalHoneycomb.hpp"
#include "../ExPolygon.hpp"

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

    Polylines lines;
    if (hex_fraction > 0.5) {
        // We're at the vertical sides.

        // Calculate an offset (between odd and even hexes).
        coordf_t offset = hex_width * (hex_index % 2) / 2.0;

        lines = _generate_single_lines(bounding_box, scale_(hex_width), scale_(offset));
    } else {
        // We're at the angled sides.

        coordf_t padding = hex_width * hex_fraction;

        if (2 * padding < this->spacing) {
            coordf_t offset = ?;
            lines = _generate_single_lines(bounding_box, scale_(hex_width), scale_(offset));
        } else if (hex_width - 2 * padding < this->spacing) {
            coordf_t offset = ?;
            lines = _generate_single_lines(bounding_box, scale_(hex_width), scale_(offset));
        } else {
            lines = _generate_split_lines(bounding_box, scale_(hex_width), scale_(padding), scale_(offset));
        }
    }

    // Calculate an offset (between odd and even hexes).
    coordf_t offset = hex_width * (hex_index % 2) / 2.0;

    // Calculate padding from line centers (zero for vertical sides, some finite value for angled sides).
    coordf_t padding = (hex_fraction < 0.66666666667) ? 0 : hex_width * hex_fraction;
    
    Polylines lines = _generate_lines(bounding_box, scale_(hex_width), scale_(padding), scale_(offset));
}

Polylines FillHorizontalHoneycomb::_generate_lines(
    BoundingBox& bounding_box,
    coord_t center_spacing,
    coord_t padding,
    coord_t offset)
{
    coord_t scaled_spacing = scale_(this->spacing);

    if (2 * padding < scaled_spacing) {
        padding = 0;
    } else if (center_spacing - 2 * padding < scaled_spacing) {
        padding = 0;
        offset -= center_spacing / 2;
    }

    Polylines lines;

    return lines;
}

}
