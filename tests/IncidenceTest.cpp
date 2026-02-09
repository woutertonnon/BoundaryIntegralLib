#include <gtest/gtest.h>
#include "mfem.hpp"
#include "incidence.hpp"

TEST(IncidenceTest, ComplexPropertyTria)
{
    const unsigned int n = 5,
                       DIM = 2;

    const mfem::Element::Type el_type = 
        mfem::Element::TRIANGLE;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian2D(
        n, n + 1, el_type
    );

    const mfem::SparseMatrix
        d0 = assembleVertexEdge(mesh),
        d1 = assembleFaceEdge(mesh, DIM);
        
    const std::unique_ptr<const mfem::SparseMatrix>
        d1d0 = std::unique_ptr<const mfem::SparseMatrix>(
            mfem::Mult(d1, d0)
        );
        
    ASSERT_FLOAT_EQ(d1d0->MaxNorm(), 0.0);
    ASSERT_GT(d0.MaxNorm(), 0.0);
    ASSERT_GT(d1.MaxNorm(), 0.0);
}

TEST(IncidenceTest, ComplexPropertyQuad)
{
    const unsigned int n = 5,
                       DIM = 2;

    const mfem::Element::Type el_type = 
        mfem::Element::QUADRILATERAL;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian2D(
        n, n + 1, el_type
    );

    const mfem::SparseMatrix
        d0 = assembleVertexEdge(mesh),
        d1 = assembleFaceEdge(mesh, DIM);
        
    const std::unique_ptr<const mfem::SparseMatrix>
        d1d0 = std::unique_ptr<const mfem::SparseMatrix>(
            mfem::Mult(d1, d0)
        );
        
    ASSERT_FLOAT_EQ(d1d0->MaxNorm(), 0.0);
    ASSERT_GT(d0.MaxNorm(), 0.0);
    ASSERT_GT(d1.MaxNorm(), 0.0);
}

TEST(IncidenceTest, ComplexPropertyTets)
{
    const unsigned int n = 5,
                       DIM = 3;

    const mfem::Element::Type el_type = 
        mfem::Element::TETRAHEDRON;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n + 1, n + 2, el_type
    );

    const mfem::SparseMatrix
        d0 = assembleVertexEdge(mesh),
        d1 = assembleFaceEdge(mesh, DIM),
        d2 = assembleElementFace(mesh);
        
    const std::unique_ptr<const mfem::SparseMatrix>
        d1d0 = std::unique_ptr<const mfem::SparseMatrix>(
            mfem::Mult(d1, d0)
        ),
        d2d1 = std::unique_ptr<const mfem::SparseMatrix>(
            mfem::Mult(d2, d1)
        );
        
    ASSERT_FLOAT_EQ(d1d0->MaxNorm(), 0.0);
    ASSERT_FLOAT_EQ(d2d1->MaxNorm(), 0.0);
    ASSERT_GT(d0.MaxNorm(), 0.0);
    ASSERT_GT(d1.MaxNorm(), 0.0);
    ASSERT_GT(d2.MaxNorm(), 0.0);
}

TEST(IncidenceTest, ComplexPropertyHex)
{
    const unsigned int n = 5,
                       DIM = 3;

    const mfem::Element::Type el_type = 
        mfem::Element::HEXAHEDRON;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n + 1, n + 2, el_type
    );

    const mfem::SparseMatrix
        d0 = assembleVertexEdge(mesh),
        d1 = assembleFaceEdge(mesh, DIM),
        d2 = assembleElementFace(mesh);
        
    const std::unique_ptr<const mfem::SparseMatrix>
        d1d0 = std::unique_ptr<const mfem::SparseMatrix>(
            mfem::Mult(d1, d0)
        ),
        d2d1 = std::unique_ptr<const mfem::SparseMatrix>(
            mfem::Mult(d2, d1)
        );
        
    ASSERT_FLOAT_EQ(d1d0->MaxNorm(), 0.0);
    ASSERT_FLOAT_EQ(d2d1->MaxNorm(), 0.0);
    ASSERT_GT(d0.MaxNorm(), 0.0);
    ASSERT_GT(d1.MaxNorm(), 0.0);
    ASSERT_GT(d2.MaxNorm(), 0.0);
}
