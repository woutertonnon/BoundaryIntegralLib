#include <gtest/gtest.h>
#include "mfem.hpp"
#include "StokesOperator.hpp"

TEST(StokesOperatorTest, MatrixRegularityGalerkin)
{
    const unsigned int n = 4;
    const double theta = 1.0,
                 penalty = 3.0,
                 factor = 1.0;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n, n, mfem::Element::TETRAHEDRON
    );

    StokesNitsche::StokesNitscheOperator op(mesh, theta, penalty, factor);

    ASSERT_EQ(op.getOperatorMode(), StokesNitsche::GALERKIN);
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

    StokesNitsche::StokesNitscheOperator op(mesh, theta, penalty, factor);

    op.setDECMode();

    ASSERT_EQ(op.getOperatorMode(), StokesNitsche::DEC);
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
    const unsigned int n = 8;
    const double theta = 1.0,
                 penalty = 3.0,
                 factor = 1.0;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n, n, mfem::Element::TETRAHEDRON
    );

    const int nv = mesh.GetNV(),
              ne = mesh.GetNEdges();

    StokesNitsche::StokesNitscheOperator op(mesh, theta, penalty, factor);

    ASSERT_EQ(op.getOperatorMode(), StokesNitsche::GALERKIN);

    std::unique_ptr<mfem::SparseMatrix> A = op.getFullSystem();

    mfem::Vector x_(nv + ne + 1),
                 y_op(nv + ne),
                 y_mat(nv + ne);
    x_(nv + ne) = 0;
    mfem::Vector x(x_, 0, nv + ne);
    x.Randomize(1);

    op.Mult(x, y_op);

    mfem::Vector y_extended(ne + nv + 1);
    A->Mult(x_, y_extended);
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

    StokesNitsche::StokesNitscheOperator op(mesh, theta, penalty, factor);
    op.setDECMode();

    ASSERT_EQ(op.getOperatorMode(), StokesNitsche::DEC);

    std::unique_ptr<mfem::SparseMatrix> A = op.getFullSystem();

    mfem::Vector x_(nv + ne + 1),
    y_op(nv + ne),
    y_mat(nv + ne);
    x_(nv + ne) = 0;
    mfem::Vector x(x_, 0, nv + ne);
    x.Randomize(1);

    op.Mult(x, y_op);

    mfem::Vector y_extended(ne + nv + 1);
    A->Mult(x_, y_extended);
    y_mat.MakeRef(y_extended, 0, ne + nv);

    mfem::Vector y_err(y_mat);
    y_err -= y_op;

    ASSERT_NEAR(y_err.Norml2() / y_op.Norml2(), 0., 1e-12);
}
