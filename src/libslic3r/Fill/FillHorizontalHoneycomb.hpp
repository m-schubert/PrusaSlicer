#ifndef slic3r_FillHorizontalHoneycomb_hpp_
#define slic3r_FillHorizontalHoneycomb_hpp_

#include "../libslic3r.h"

#include "FillBase.hpp"

namespace Slic3r {

class FillHorizontalHoneycomb : public Fill
{
public:
    FillHorizontalHoneycomb() {}
    virtual Fill* clone() const { return new FillHorizontalHoneycomb(*this); }

protected:
    virtual float _layer_angle(size_t idx) const { return 0.f; }
    
    virtual void _fill_surface_single(
        const FillParams                &params, 
        unsigned int                     thickness_layers,
        const std::pair<float, Point>   &direction, 
        ExPolygon                       &expolygon, 
        Polylines                       &polylines_out);

private:
    std::pair<Polygon, Polygon> _generate_single_lines(const BoundingBox& bounding_box, coord_t center_spacing, coord_t offset);
    std::pair<Polygon, Polygon> _generate_split_lines(const BoundingBox& bounding_box, coord_t center_spacing, coord_t padding, coord_t offset);
};

} // namespace Slic3r

#endif // slic3r_FillHorizontalHoneycomb_hpp_