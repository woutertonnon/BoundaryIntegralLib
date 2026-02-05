#include <gtest/gtest.h>
#include "mfem.hpp"
#include "StokesOperator.hpp"

TEST(StokesOperatorTest, FullGalerkinSystem)
{
    const unsigned int n = 6;
    const double theta = 1.0,
                 penalty = 3.0,
                 factor = 1.0;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n, n, mfem::Element::TETRAHEDRON
    );

    StokesNitsche::StokesNitscheOperator op(mesh, theta, penalty, factor);

    ASSERT_EQ(op.getOperatorMode(), StokesNitsche::GALERKIN);

    std::unique_ptr<mfem::SparseMatrix> A = op.getFullSystem();
    auto solver = std::make_unique<mfem::UMFPackSolver>(
        *A
    );

    const unsigned int nv = mesh.GetNV(),
                       ne = mesh.GetNEdges();

    mfem::Vector rhs(nv + ne),
                 sol(nv + ne),
                 err_op(nv + ne),
                 err_mat(nv + ne);
    sol.Randomize(1);
    A->Mult(sol, rhs);
    err_op.Randomize(0);
    err_mat.Randomize(0);

    solver->Mult(rhs, err_mat);
    // mfem::MINRES(op, rhs, err_op, 1, 100'000, 1e-12, 1e-12);

    err_op -= sol;
    err_mat -= sol;

    std::cout << "LU error: "
              << err_mat.Norml2() / sol.Norml2()
              << std::endl;
    std::cout << "MINRES error: "
              << err_op.Norml2() / sol.Norml2()
              << std::endl;

    mfem::Vector tmp(nv + ne);
    op.Mult(err_mat, tmp);
    std::cout << "LU Residual: "
              << tmp.Norml2() / rhs.Norml2()
              << std::endl;

    op.Mult(err_op, tmp);
    std::cout << "MINRES Residual: "
              << tmp.Norml2() / rhs.Norml2()
              << std::endl;

    mfem::Vector ew;
    std::unique_ptr<mfem::DenseMatrix> Ad =
        std::unique_ptr<mfem::DenseMatrix>(
            A->ToDenseMatrix()
        );
    Ad->Eigenvalues(ew);

    ew.Abs();
    std::cout << "Smallest EW (absolute value): "
              << ew.Min() << std::endl;
}
