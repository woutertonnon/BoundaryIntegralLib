#include <gtest/gtest.h>
#include "BoundarySeparator.hpp"
#include "mfem.hpp"
#include <fstream>

void randomizeVectorIdx(mfem::Vector& v,
                        const mfem::Array<int>& idxs,
                        const int seed)
{
    srand(seed);
    for(const int idx: idxs)
        v(idx) = (double)(rand()) / (double)(RAND_MAX);
}

void testMesh(mfem::Mesh& mesh)
{
    const int DIM = mesh.Dimension();
    int seed = 1;

    auto h1_fec = std::make_unique<mfem::H1_FECollection>(
        1, DIM
    );
    auto h1 = std::make_unique<mfem::FiniteElementSpace>(
        &mesh, h1_fec.get()
    );

    auto hcurl_fec = std::make_unique<mfem::ND_FECollection>(
        1, DIM
    );
    auto hcurl = std::make_unique<mfem::FiniteElementSpace>(
        &mesh, hcurl_fec.get()
    );

    auto hdiv_fec = std::make_unique<mfem::RT_FECollection>(
        0, DIM
    );
    auto hdiv = std::make_unique<mfem::FiniteElementSpace>(
        &mesh, hdiv_fec.get()
    );

    std::array<mfem::FiniteElementSpace*, 3>
        fespaces({
            h1.get(), hcurl.get(), hdiv.get()
        });

    const mfem::Array<int>
        node_perm = createBDPermutation(*h1),
        edge_perm = createBDPermutation(*hcurl),
        face_perm = createBDPermutation(*hdiv);

    const std::array<const mfem::Array<int>*, 3>
        perms({
            &node_perm, &edge_perm, &face_perm
        });

    mfem::Array<int> bd_dof;
    unsigned int n_dof, n_bd_dof, n_int_dof;
    mfem::Vector randvec, randvec_permuted,
                 randvec_permuted_int, randvec_permuted_bd;
    mfem::SparseMatrix P;

    for(unsigned int k = 0; k < 3; ++k)
    {
        auto* fes = fespaces[k];
        const auto& perm = *perms[k];

        fes->GetBoundaryTrueDofs(bd_dof);
        n_dof = fes->GetNDofs();
        n_bd_dof = bd_dof.Size();
        ASSERT_GE(n_dof, n_bd_dof);
        n_int_dof = n_dof - n_bd_dof;

        for(const int bd_dof_idx: bd_dof)
            ASSERT_GE(perm[bd_dof_idx], n_int_dof);

        P = std::move(
            createPermutationMatrixFromVector(perm)
        );

        randvec.SetSize(n_dof);
        randvec = 0.0;
        randvec_permuted.SetSize(n_dof);

        randomizeVectorIdx(randvec, bd_dof, seed);
        ++seed;

        P.MultTranspose(randvec, randvec_permuted);

        randvec_permuted_int.MakeRef(randvec_permuted, 0, n_int_dof);
        randvec_permuted_bd.MakeRef(randvec_permuted, n_int_dof, n_bd_dof);

        ASSERT_FLOAT_EQ(randvec_permuted_int.Normlinf(), 0);
        ASSERT_FLOAT_EQ(randvec_permuted_bd.Normlinf(), randvec.Normlinf());
    }
}

TEST(BoundarySeparatorTests, Structured2DTria)
{
    const int n = 7;
    const mfem::Element::Type el_type =
        mfem::Element::TRIANGLE;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian2D(
        n, n + 1, el_type
    );

    testMesh(mesh);
}

TEST(BoundarySeparatorTests, Structured2DQuad)
{
    const int n = 7;
    const mfem::Element::Type el_type =
        mfem::Element::QUADRILATERAL;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian2D(
        n, n + 1, el_type
    );

    testMesh(mesh);
}

TEST(BoundarySeparatorTests, OneElement2D)
{
    const int n = 1;
    const mfem::Element::Type el_type =
        mfem::Element::QUADRILATERAL;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian2D(
        n, n, el_type
    );

    testMesh(mesh);
}

TEST(BoundarySeparatorTests, Structured3DTets)
{
    const int n = 7;
    const mfem::Element::Type el_type =
        mfem::Element::TETRAHEDRON;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n + 1, n + 2, el_type
    );

    testMesh(mesh);
}

TEST(BoundarySeparatorTests, Structured3DHex)
{
    const int n = 7;
    const mfem::Element::Type el_type =
        mfem::Element::HEXAHEDRON;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n + 1, n + 2, el_type
    );

    testMesh(mesh);
}

TEST(BoundarySeparatorTests, OneElement3D)
{
    const int n = 1;
    const mfem::Element::Type el_type =
        mfem::Element::HEXAHEDRON;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n, n, el_type
    );

    testMesh(mesh);
}

TEST(BoundarySeparatorTests, Unstructured2D)
{
    mfem::Mesh mesh("../extern/mfem/data/star-mixed.mesh", 1);
    for(unsigned int k = 0; k < 2; ++k)
        mesh.UniformRefinement();

    testMesh(mesh);
}

TEST(BoundarySeparatorTests, Unstructured3D)
{
    mfem::Mesh mesh("../extern/mfem/data/tinyzoo-3d.mesh", 1);
    for(unsigned int k = 0; k < 2; ++k)
        mesh.UniformRefinement();

    testMesh(mesh);
}
