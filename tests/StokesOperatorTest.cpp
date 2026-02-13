#include <gtest/gtest.h>
#include "mfem.hpp"
#include "StokesOperator.hpp"

using namespace StokesNitsche;

TEST(StokesOperatorTest, StaticCondensationDoesNothing)
{
    const unsigned int n = 4;
    const double theta = 1.0,
                 penalty = 3.0,
                 factor = 1.0;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n, n, mfem::Element::TETRAHEDRON
    );

    std::shared_ptr<mfem::Mesh> mesh_ptr =
        std::make_unique<mfem::Mesh>(
            std::move(mesh)
        );
    StokesNitscheOperator op(mesh_ptr,
                             theta, penalty, factor);

    const std::array<const mfem::FiniteElementSpace*, 3>
        fespaces({
            &op.getH1Space(), &op.getHCurlSpace(), &op.getHDivOrL2Space()
        });

    for(const auto fes: fespaces)
    {
        // Does not accept const mfem::FiniteElementSpace*
        // But it is fine, we never use it again, so no UB (I hope)
        auto static_condensation = mfem::StaticCondensation(
            const_cast<mfem::FiniteElementSpace*>(fes)
        );

        ASSERT_EQ(static_condensation.GetNPrDofs(), 0);
        ASSERT_EQ(static_condensation.GetNExDofs(), fes->GetNDofs());
        ASSERT_FALSE(static_condensation.ReducesTrueVSize());
    }
}

TEST(StokesOperatorTest, NoLocalInteriorDOF)
{
    const unsigned int n = 4;
    const double theta = 1.0,
                 penalty = 3.0,
                 factor = 1.0;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n, n, mfem::Element::TETRAHEDRON
    );

    std::shared_ptr<mfem::Mesh> mesh_ptr =
        std::make_unique<mfem::Mesh>(
            std::move(mesh)
        );
    StokesNitscheOperator op(mesh_ptr,
                             theta, penalty, factor);

    const std::array<const mfem::FiniteElementSpace*, 3>
        fespaces({
            &op.getH1Space(), &op.getHCurlSpace(), &op.getHDivOrL2Space()
        });

    for(const auto fes: fespaces)
        for(int k = 0; k < fes->GetNE(); ++k)
            ASSERT_EQ(fes->GetNumElementInteriorDofs(k), 0);
}

TEST(StokesOperatorTest, MatrixRegularityGalerkin)
{
    const unsigned int n = 4;
    const double theta = 1.0,
                 penalty = 3.0,
                 factor = 1.0;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n, n, mfem::Element::TETRAHEDRON
    );

    std::shared_ptr<mfem::Mesh> mesh_ptr =
        std::make_unique<mfem::Mesh>(
            std::move(mesh)
        );
    StokesNitscheOperator op(mesh_ptr,
                             theta, penalty, factor);

    ASSERT_EQ(op.getOperatorMode(), OperatorMode::Galerkin);
    std::unique_ptr<mfem::SparseMatrix> A = op.getFullGalerkinSystem();

    mfem::Vector ew;
    std::unique_ptr<mfem::DenseMatrix> Ad =
        std::unique_ptr<mfem::DenseMatrix>(
            A->ToDenseMatrix()
        );
    Ad->Eigenvalues(ew);

    ew.Abs();
    const double min_ew = ew.Min();

    // Should be large enough, so that it is not nearly singular
    ASSERT_GT(min_ew, 1e-12);
}

TEST(StokesOperatorTest, MatrixRegularityDEC)
{
    const unsigned int n = 4;
    const double theta = 1.0,
                 penalty = 3.0,
                 factor = 1.0;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n, n, mfem::Element::TETRAHEDRON
    );

    std::shared_ptr<mfem::Mesh> mesh_ptr =
        std::make_unique<mfem::Mesh>(
            std::move(mesh)
        );
    StokesNitscheOperator op(mesh_ptr, theta, penalty, factor,
                             MassLumping::Diagonal);

    op.setDECMode();

    ASSERT_EQ(op.getOperatorMode(), OperatorMode::DEC);
    std::unique_ptr<mfem::SparseMatrix> A = op.getFullDECSystem();

    mfem::Vector ew;
    std::unique_ptr<mfem::DenseMatrix> Ad =
        std::unique_ptr<mfem::DenseMatrix>(
            A->ToDenseMatrix()
        );
    Ad->Eigenvalues(ew);

    ew.Abs();
    const double min_ew = ew.Min();

    // Should be large enough, so that it is not nearly singular
    ASSERT_GT(min_ew, 1e-12);
}

TEST(StokesOperatorTest, OperatorGalerkin)
{
    const unsigned int n = 5;
    const double theta = 1.0,
                 penalty = 3.0,
                 factor = 0.0;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n, n, mfem::Element::TETRAHEDRON
    );

    const int nv = mesh.GetNV(),
              ne = mesh.GetNEdges();

    std::shared_ptr<mfem::Mesh> mesh_ptr =
        std::make_unique<mfem::Mesh>(
            std::move(mesh)
        );
    StokesNitscheOperator op(mesh_ptr,
                             theta, penalty, factor);

    ASSERT_EQ(op.getOperatorMode(), OperatorMode::Galerkin);

    std::unique_ptr<mfem::SparseMatrix> A = op.getFullSystem();

    mfem::Vector x_(nv + ne + 1),
                 y_op(nv + ne),
                 y_mat(nv + ne);
    x_(nv + ne) = 0;
    mfem::Vector x(x_, 0, nv + ne);
    x.Randomize(1);
    op.eliminateConstants(x);

    op.Mult(x, y_op);

    mfem::Vector y_extended(ne + nv + 1);
    A->Mult(x_, y_extended);

    ASSERT_NEAR(y_extended(ne + nv), 0, 1e-12);
    y_mat.MakeRef(y_extended, 0, ne + nv);

    mfem::Vector y_err(y_mat);
    y_err -= y_op;

    ASSERT_NEAR(y_err.Norml2() / y_op.Norml2(), 0., 1e-12);
}

TEST(StokesOperatorTest, OperatorDEC)
{
    const unsigned int n = 8;
    const double theta = 1.0,
                 penalty = 3.0,
                 factor = 1.0;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n, n, mfem::Element::TETRAHEDRON
    );

    const int nv = mesh.GetNV(),
              ne = mesh.GetNEdges();

    std::shared_ptr<mfem::Mesh> mesh_ptr =
        std::make_unique<mfem::Mesh>(
            std::move(mesh)
        );
    StokesNitscheOperator op(mesh_ptr,
                             theta, penalty, factor,
                             MassLumping::Diagonal);
    op.setDECMode();

    ASSERT_EQ(op.getOperatorMode(), OperatorMode::DEC);

    std::unique_ptr<mfem::SparseMatrix> A = op.getFullSystem();

    mfem::Vector x_(nv + ne + 1),
                 y_op(nv + ne),
                 y_mat(nv + ne);
    x_(nv + ne) = 0;
    mfem::Vector x(x_, 0, nv + ne);
    x.Randomize(1);
    op.eliminateConstants(x);

    op.Mult(x, y_op);

    mfem::Vector y_extended(ne + nv + 1);
    A->Mult(x_, y_extended);

    ASSERT_NEAR(y_extended(ne + nv), 0, 1e-12);
    y_mat.MakeRef(y_extended, 0, ne + nv);

    mfem::Vector y_err(y_mat);
    y_err -= y_op;

    ASSERT_NEAR(y_err.Norml2() / y_op.Norml2(), 0., 1e-12);
}
