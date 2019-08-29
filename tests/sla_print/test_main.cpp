#include <gtest/gtest.h>

#include "libslic3r/libslic3r.h"
#include "libslic3r/SLAPrint.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/SLA/SLABasePool.hpp"
#include "libslic3r/SLA/SLASupportTree.hpp"
#include "libslic3r/SLA/SLAAutoSupports.hpp"
#include "libslic3r/MTUtils.hpp"

#if defined(WIN32) || defined(_WIN32) 
#define PATH_SEPARATOR "\\" 
#else 
#define PATH_SEPARATOR "/" 
#endif

class SLASupportGeneration: public ::testing::Test {
protected:

    Slic3r::TriangleMesh       m_inputmesh;
    Slic3r::TriangleMesh       m_output_mesh;
    Slic3r::ExPolygons         m_model_contours;
    Slic3r::ExPolygons         m_support_contours;
    Slic3r::sla::PadConfig     m_padcfg;
    Slic3r::sla::SupportConfig m_supportcfg;

    // To be able to reuse slicing results
    std::vector<Slic3r::ExPolygons> m_model_slices;
    std::vector<float>              m_slice_grid;

public:
    SLASupportGeneration() {}

    void load_model(const std::string &stl_filename)
    {
        auto fpath = std::string(TEST_DATA_DIR PATH_SEPARATOR) + stl_filename;
        m_inputmesh.ReadSTLFile(fpath.c_str());
        m_inputmesh.repair();
    }

    enum e_validity {
        ASSUME_NO_EMPTY = 1,
        ASSUME_MANIFOLD = 2,
        ASSUME_NO_REPAIR = 4
    };

    void check_validity(int flags = ASSUME_NO_EMPTY | ASSUME_MANIFOLD |
                                    ASSUME_NO_REPAIR)
    {
        if (flags & (1 << ASSUME_NO_EMPTY)) {
            ASSERT_TRUE(!m_output_mesh.empty());
        }

        ASSERT_TRUE(stl_validate(&m_output_mesh.stl));

        bool do_update_shared_vertices = false;
        m_output_mesh.repair(do_update_shared_vertices);


        if (flags & (1 << ASSUME_NO_REPAIR)) {
            ASSERT_FALSE(m_output_mesh.needed_repair());
        }

        if (flags & (1 << ASSUME_MANIFOLD)) {
            // Pad should be stricly manifold
            m_output_mesh.require_shared_vertices();
            ASSERT_TRUE(m_output_mesh.is_manifold());
        }
    }

    void test_pad(const std::string &stl_filename)
    {
        load_model(stl_filename);
        
        // Create pad skeleton only from the model
        Slic3r::sla::pad_plate(m_inputmesh, m_model_contours);
        
        ASSERT_FALSE(m_model_contours.empty());
        
        // Create a default pad, m_support_contours are empty here
        Slic3r::sla::create_pad({}, m_model_contours, m_output_mesh, m_padcfg);
        
        check_validity();
        
        auto bb = m_output_mesh.bounding_box();
        ASSERT_DOUBLE_EQ(bb.max.z() - bb.min.z(),
                         Slic3r::sla::get_pad_fullheight(m_padcfg));
    }

    void test_supports(const std::string &stl_filename)
    {
        using namespace Slic3r;
        load_model(stl_filename);

        TriangleMeshSlicer slicer{&m_inputmesh};

        auto bb             = m_inputmesh.bounding_box();
        double zmin         = bb.min.z();
        double zmax         = bb.max.z();
        auto layer_h        = 0.05f;
        auto closing_radius = 0.005f;

        m_slice_grid = grid(float(zmin), float(zmax), layer_h);
        slicer.slice(m_slice_grid, closing_radius, &m_model_slices, []{});

        // Create the special index-triangle mesh with spatial indexing which
        // is the input of the support point and support mesh generators
        sla::EigenMesh3D emesh{m_inputmesh};

        // Create the support point generator
        sla::SLAAutoSupports::Config autogencfg;
        autogencfg.head_diameter = float(2 * m_supportcfg.head_front_radius_mm);
        sla::SLAAutoSupports point_gen{emesh, m_model_slices, m_slice_grid,
                                       autogencfg, [] {}, [](int) {}};

        // Get the calculated support points.
        std::vector<sla::SupportPoint> support_points = point_gen.output();

        // If there is no elevation, support points shall be removed from the
        // bottom of the object.
        if (m_supportcfg.object_elevation_mm < EPSILON) {
            sla::remove_bottom_points(support_points, zmin,
                                      m_supportcfg.base_height_mm);
        } else {
            // Should be support points at least on the bottom of the model
            ASSERT_FALSE(support_points.empty());
        }

        // Generate the actual support tree
        sla::SLASupportTree supporttree(support_points, emesh, m_supportcfg);

        // Get the TriangleMesh object for the generated supports
        m_output_mesh = supporttree.merged_mesh();

        // Check the mesh for sanity
        check_validity(ASSUME_NO_EMPTY | ASSUME_NO_REPAIR);

        // Quick check if the dimensions and placement of supports are correct
        auto obb = m_output_mesh.bounding_box();
        ASSERT_DOUBLE_EQ(obb.min.z(), zmin - m_supportcfg.object_elevation_mm);
        ASSERT_LE(obb.max.z(), zmax);
    }
};

TEST_F(SLASupportGeneration, FlatPad) {
    test_pad("20mm_cube.stl");
}

TEST_F(SLASupportGeneration, WingedPad) {
    // Add some wings to the pad to test the cavity
    m_padcfg.min_wall_height_mm = 1.;
    
    test_pad("20mm_cube.stl");
}

TEST_F(SLASupportGeneration, ElevatedSupports) {
    test_supports("20mm_cube.stl");
}

TEST_F(SLASupportGeneration, FloorSupports) {
    m_supportcfg.object_elevation_mm = 0;
    test_supports("20mm_cube.stl");
}

TEST_F(SLASupportGeneration, SupportsShouldNotPierceModel) {
    // Set head penetration to a small negative value which should ensure that
    // the supports will not touch the model body.
    m_supportcfg.head_penetration_mm = -0.1;

    test_supports("20mm_cube.stl");

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
