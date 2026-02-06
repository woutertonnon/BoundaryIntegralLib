#include <gtest/gtest.h>
#include "mfem.hpp"
#include "StokesDGS.hpp"
#include "StokesOperator.hpp"

TEST(StokesOperatorTest, Convergence)
{
    const unsigned int n = 5;
    const double theta = 1.0,
                 penalty = 3.0,
                 factor = 1.0;
    const double tol = 1e-12;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n, n, mfem::Element::TETRAHEDRON
    );

    StokesNitsche::StokesNitscheOperator op(mesh, theta, penalty, factor);

    op.setDECMode();
    ASSERT_EQ(op.getOperatorMode(), StokesNitsche::DEC);

    std::unique_ptr<mfem::SparseMatrix> A = op.getFullDECSystem();
    auto solver = std::make_unique<mfem::UMFPackSolver>(
        *A
    );

    const unsigned int nv = mesh.GetNV(),
                       ne = mesh.GetNEdges();

    mfem::Vector rhs(nv + ne),
                 sol(nv + ne),
                 sol_dgs(nv + ne),
                 sol_lu(nv + ne),
                 residual(nv + ne);
    sol.Randomize(1);
    op.Mult(sol, rhs);

    mfem::Vector rhs_extended(nv + ne + 1),
                 sol_extended(nv + ne + 1);
    for(unsigned int k = 0; k < nv + ne; ++k)
        rhs_extended(k) = rhs(k);
    rhs_extended(nv + ne) = 0;

    solver->Mult(rhs_extended, sol_extended);
    sol_lu.MakeRef(sol_extended, 0, ne + nv);

    mfem::Vector residual_lu = rhs;
    op.AddMult(sol_lu, residual_lu, -1.0);

    ASSERT_NEAR(sol_extended(nv + ne) / sol_extended.Norml2(), 0., tol);
    ASSERT_LT(residual_lu.Norml2() / rhs.Norml2(), tol);

    StokesNitsche::StokesNitscheDGS dgs(op);

    sol_dgs.Randomize(2);

    const unsigned int maxit = 10'000;
    double err = tol + 1;
    unsigned int iter;
    mfem::Vector residual_dgs;

    for(iter = 0; iter < maxit && err > tol; ++iter)
    {
        residual_dgs = rhs;
        op.AddMult(sol_dgs, residual_dgs, -1.0);
        ASSERT_EQ(residual_dgs.CheckFinite(), 0);

        dgs.Mult(rhs, sol_dgs);
        ASSERT_EQ(sol_dgs.CheckFinite(), 0);

        err = residual_dgs.Norml2() / rhs.Norml2();

        if(!(iter % 100) || !(err > tol))
            std::cout << iter << " Relative residual: "
                    << err
                    << std::endl;
    }

    ASSERT_LT(iter, maxit);
    ASSERT_LT(residual_dgs.Norml2() / rhs.Norml2(), tol);
}
