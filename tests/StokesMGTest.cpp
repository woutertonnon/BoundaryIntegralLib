#include <gtest/gtest.h>
#include <iomanip>
#include "mfem.hpp"
#include "StokesDGS.hpp"
#include "StokesOperator.hpp"
#include "StokesMG.hpp"

// Helper function that performs a standalone Multigrid V-Cycle test on a given mesh.
// It builds the MG hierarchy, sets up a random linear system, and iteratively checks
// if the residual norm decreases below a tolerance within 128 cycles.
void RunVCycleTest(std::shared_ptr<mfem::Mesh> mesh_ptr,
                   const double tol = 1e-6,
                   const double penalty = 9.0,
                   const int refinements = 4)
{
#ifdef MFEM_USE_SUITESPARSE
    std::cout << "Using SuiteSparse for Coarse Grid Solve" << std::endl;
#endif

    const double theta = 1.0, factor = 1.0;

    StokesNitsche::StokesMG mg(mesh_ptr, theta, penalty, factor);

    for (int i = 0; i < refinements; ++i)
        mg.addRefinedLevel();

    const auto& fine_op = mg.getFinestOperator();

    const int num_rows = fine_op.NumRows();
    mfem::Vector x_exact(num_rows), b(num_rows),
                 x_sol(num_rows), residual(num_rows);

    x_exact.Randomize(1);
    fine_op.Mult(x_exact, b);
    x_sol = 0.0;

    mg.setCycleType(StokesNitsche::MGCycleType::VCycle);
    mg.setSmoothIterations(1, 1);

    double initial_norm = 0.0;
    const int max_iter = 128;

    std::cout << "NDof = " << fine_op.NumRows() << std::endl;
    std::cout << "\n  Iter | Rel. Residual \n-------|---------------\n";

    for (int iter = 0; iter < max_iter; ++iter)
    {
        residual = b;
        fine_op.AddMult(x_sol, residual, -1.0);
        double current_norm = residual.Norml2();

        if (iter == 0) initial_norm = current_norm;
        double rel_norm = current_norm / initial_norm;

        std::cout << "  " << std::setw(4) << iter << " | "
                  << std::scientific << std::setprecision(4)
                  << rel_norm << std::endl;

        if (rel_norm < tol) break;
        mg.Mult(b, x_sol);
    }

    residual = b;
    fine_op.AddMult(x_sol, residual, -1.0);
    double final_rel_norm = residual.Norml2() / initial_norm;

    ASSERT_LT(final_rel_norm, tol)
        << "MG V-Cycle failed to converge within tolerance.";
}

// Helper function that tests GMRES convergence using MG as a preconditioner.
// It constructs a Galerkin operator on the finest level and validates that the
// preconditioned GMRES solver converges to the exact solution within the tolerance.
void RunGMRESTest(std::shared_ptr<mfem::Mesh> mesh_ptr,
                  const double tol = 1e-6,
                  const double penalty = 9.0,
                  const int refinements = 4)
{
#ifdef MFEM_USE_SUITESPARSE
    std::cout << "Using SuiteSparse for Coarse Grid Solve" << std::endl;
#endif

    const double theta = 1.0, factor = 1.0;

    StokesNitsche::StokesMG mg(mesh_ptr, theta, penalty, factor);

    for (int i = 0; i < refinements; ++i)
        mg.addRefinedLevel();

    std::cout << "NDof = " << mg.NumRows() << std::endl;

    mg.setCycleType(StokesNitsche::MGCycleType::VCycle);
    mg.setSmoothIterations(1, 1);
    mg.setOperatorMode(StokesNitsche::OperatorMode::Galerkin);

    auto fine_mesh_ptr = std::make_shared<mfem::Mesh>(
        mg.getFinestOperator().getMesh());

    StokesNitsche::StokesNitscheOperator op_galerkin(
        fine_mesh_ptr, theta, penalty, factor);
    op_galerkin.setGalerkinMode();

    mfem::GMRESSolver gmres;
    gmres.SetOperator(op_galerkin);
    gmres.SetPreconditioner(mg);
    gmres.SetAbsTol(1e-12);
    gmres.SetRelTol(tol);
    gmres.SetMaxIter(100);
    gmres.SetPrintLevel(1);
    gmres.SetKDim(20);

    const int num_rows = op_galerkin.NumRows();
    mfem::Vector x_exact(num_rows), b(num_rows), x_sol(num_rows);

    x_exact.Randomize(1);
    op_galerkin.Mult(x_exact, b);
    x_sol = 0.0;

    std::cout << "Running GMRES (Galerkin) with MG (Galerkin) Precond...\n";
    gmres.Mult(b, x_sol);

    ASSERT_TRUE(gmres.GetConverged()) << "GMRES failed to converge.";

    const double rel_res = gmres.GetFinalRelNorm();
    std::cout << "Final Relative Residual (according to mfem): "
              << rel_res << std::endl;

    EXPECT_LT(rel_res, tol);
}

// Tests V-Cycle convergence on a Hexahedral mesh.
// Initializes a 1x1x1 Hex mesh and delegates validation to the RunVCycleTest helper.
TEST(StokesMGTest, VCycleConvergenceHex)
{
    const unsigned int n = 1;
    auto mesh_ptr = std::make_shared<mfem::Mesh>(
        mfem::Mesh::MakeCartesian3D(n, n, n, mfem::Element::HEXAHEDRON)
    );
    RunVCycleTest(mesh_ptr);
}

// Tests GMRES convergence with MG preconditioning on a Hexahedral mesh.
// Initializes a 1x1x1 Hex mesh and delegates validation to the RunGMRESTest helper.
TEST(StokesMGTest, GMRESGalerkinPreconditionerHex)
{
    const unsigned int n = 1;
    auto mesh_ptr = std::make_shared<mfem::Mesh>(
        mfem::Mesh::MakeCartesian3D(n, n, n, mfem::Element::HEXAHEDRON)
    );
    RunGMRESTest(mesh_ptr);
}

// Tests V-Cycle convergence on a Tetrahedral mesh.
// Initializes a 1x1x1 Tet mesh and delegates validation to the RunVCycleTest helper.
TEST(StokesMGTest, VCycleConvergenceTetra)
{
    const unsigned int n = 1;
    auto mesh_ptr = std::make_shared<mfem::Mesh>(
        mfem::Mesh::MakeCartesian3D(n, n, n, mfem::Element::TETRAHEDRON)
    );
    RunVCycleTest(mesh_ptr);
}

// Tests GMRES convergence with MG preconditioning on a Tetrahedral mesh.
// Initializes a 1x1x1 Tet mesh and delegates validation to the RunGMRESTest helper.
TEST(StokesMGTest, GMRESGalerkinPreconditionerTetra)
{
    const unsigned int n = 1;
    auto mesh_ptr = std::make_shared<mfem::Mesh>(
        mfem::Mesh::MakeCartesian3D(n, n, n, mfem::Element::TETRAHEDRON)
    );
    RunGMRESTest(mesh_ptr);
}
