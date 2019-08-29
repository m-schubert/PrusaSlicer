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

public:
    SLASupportGeneration() {}
    
    void load_model(const std::string &stl_filename) {
        auto fpath = std::string(TEST_DATA_DIR PATH_SEPARATOR) + stl_filename;
        m_inputmesh.ReadSTLFile(fpath.c_str());
        m_inputmesh.repair();
    }
    
    void check_validity() {
        ASSERT_TRUE(!m_output_mesh.empty());
        ASSERT_TRUE(stl_validate(&m_output_mesh.stl));
        
        bool do_update_shared_vertices = false;
        m_output_mesh.repair(do_update_shared_vertices);
        ASSERT_FALSE(m_output_mesh.needed_repair());
        
        // Pad should be stricly manifold
        m_output_mesh.require_shared_vertices();
        ASSERT_TRUE(m_output_mesh.is_manifold());
    }
    
    void test_pad(const std::string &stl_filename) {
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
    using namespace Slic3r; 
    load_model("20mm_cube.stl");
    
    std::vector<Slic3r::ExPolygons> slices;
    TriangleMeshSlicer slicer{&m_inputmesh};

    auto bb             = m_inputmesh.bounding_box();
    double zmin         = bb.min.z();
    double zmax         = bb.max.z();
    auto layer_h        = 0.05f;
    auto closing_radius = 0.005f;
    std::vector<float> heights = grid(float(zmin), float(zmax), layer_h);
    slicer.slice(heights, closing_radius, &slices, []{});
    
    sla::EigenMesh3D emesh{m_inputmesh};
    SLAAutoSupports::Config autogencfg;
    autogencfg.head_diameter = float(2 * m_supportcfg.head_front_radius_mm);
    SLAAutoSupports support_point_generator{emesh,      slices, heights,
                                            autogencfg, [] {},  [](int) {}};
    
    const auto& support_points = support_point_generator.output();
    ASSERT_FALSE(support_points.empty());

    sla::SLASupportTree supporttree(support_points, emesh, m_supportcfg);
    
    m_output_mesh = supporttree.merged_mesh();

    check_validity();
    
    auto obb = m_output_mesh.bounding_box();
    ASSERT_LE(obb.max.z() - obb.min.z(), zmax - zmin);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
