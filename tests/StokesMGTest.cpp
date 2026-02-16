#include <gtest/gtest.h>
#include <iomanip>
#include "mfem.hpp"
#include "StokesDGS.hpp"
#include "StokesOperator.hpp"
#include "StokesMG.hpp"

#define penalty (9.0)
#define refinements (4)

TEST(StokesMGTest, VCycleConvergenceHex)
{
#ifdef MFEM_USE_SUITESPARSE
    std::cout << "Using SuiteSparse for Coarse Grid Solve" << std::endl;
#endif
    // 1. Setup Parameters
    const unsigned int n = 1;
    const double theta = 1.0, factor = 1.0;
    const double tol = 1e-6;
    // const int refinements = 4;
    const mfem::Element::Type el_type =
        mfem::Element::HEXAHEDRON;

    // 2. Initialize Coarse Mesh
    auto mesh_ptr = std::make_shared<mfem::Mesh>(
        mfem::Mesh::MakeCartesian3D(n, n, n, el_type));

    // 3. Initialize MG Solver
    StokesNitsche::StokesMG mg(mesh_ptr, theta, penalty, factor);

    // 4. Build Hierarchy
    for (int i = 0; i < refinements; ++i)
        mg.addRefinedLevel();

    // 5. Retrieve Reference Operator
    const auto& fine_op = mg.getFinestOperator();

    // 6. Setup Linear System (Ax = b)
    const int num_rows = fine_op.NumRows();
    mfem::Vector x_exact(num_rows), b(num_rows),
                 x_sol(num_rows), residual(num_rows);

    x_exact.Randomize(1);
    fine_op.Mult(x_exact, b);
    x_sol = 0.0;

    // 7. Configure MG
    mg.setCycleType(StokesNitsche::MGCycleType::VCycle);
    mg.setSmoothIterations(1, 1);

    // 8. Run Convergence Loop
    double initial_norm = 0.0;
    const int max_iter = 128;

    std::cout << "NDof = "
              << fine_op.NumRows()
              << std::endl;
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

    // 9. Final Assertion
    residual = b;
    fine_op.AddMult(x_sol, residual, -1.0);
    double final_rel_norm = residual.Norml2() / initial_norm;

    ASSERT_LT(final_rel_norm, tol)
        << "MG V-Cycle failed to converge within tolerance.";
}

TEST(StokesMGTest, GMRESGalerkinPreconditionerHex)
{
#ifdef MFEM_USE_SUITESPARSE
    std::cout << "Using SuiteSparse for Coarse Grid Solve" << std::endl;
#endif

    const unsigned int n = 1;
    const double theta = 1.0, factor = 1.0;
    // const int refinements = 4;
    const mfem::Element::Type el_type =
        mfem::Element::HEXAHEDRON;

    auto mesh_ptr = std::make_shared<mfem::Mesh>(
        mfem::Mesh::MakeCartesian3D(n, n, n, el_type));

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
    gmres.SetRelTol(1e-6);
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

    EXPECT_LT(rel_res, 1e-6);
}

TEST(StokesMGTest, VCycleConvergenceTetra)
{
#ifdef MFEM_USE_SUITESPARSE
    std::cout << "Using SuiteSparse for Coarse Grid Solve" << std::endl;
#endif
    // 1. Setup Parameters
    const unsigned int n = 1;
    const double theta = 1.0, factor = 1.0;
    const double tol = 1e-6;
    // const int refinements = 4;
    const mfem::Element::Type el_type =
        mfem::Element::TETRAHEDRON;

    // 2. Initialize Coarse Mesh
    auto mesh_ptr = std::make_shared<mfem::Mesh>(
        mfem::Mesh::MakeCartesian3D(n, n, n, el_type));

    // 3. Initialize MG Solver
    StokesNitsche::StokesMG mg(mesh_ptr, theta, penalty, factor);

    // 4. Build Hierarchy
    for (int i = 0; i < refinements; ++i)
        mg.addRefinedLevel();

    // 5. Retrieve Reference Operator
    const auto& fine_op = mg.getFinestOperator();

    // 6. Setup Linear System (Ax = b)
    const int num_rows = fine_op.NumRows();
    mfem::Vector x_exact(num_rows), b(num_rows),
                 x_sol(num_rows), residual(num_rows);

    x_exact.Randomize(1);
    fine_op.Mult(x_exact, b);
    x_sol = 0.0;

    // 7. Configure MG
    mg.setCycleType(StokesNitsche::MGCycleType::VCycle);
    mg.setSmoothIterations(1, 1);

    // 8. Run Convergence Loop
    double initial_norm = 0.0;
    const int max_iter = 128;

    std::cout << "NDof = "
              << fine_op.NumRows()
              << std::endl;
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

    // 9. Final Assertion
    residual = b;
    fine_op.AddMult(x_sol, residual, -1.0);
    double final_rel_norm = residual.Norml2() / initial_norm;

    ASSERT_LT(final_rel_norm, tol)
        << "MG V-Cycle failed to converge within tolerance.";
}

TEST(StokesMGTest, GMRESGalerkinPreconditionerTetra)
{
#ifdef MFEM_USE_SUITESPARSE
    std::cout << "Using SuiteSparse for Coarse Grid Solve" << std::endl;
#endif

    const unsigned int n = 1;
    const double theta = 1.0, factor = 1.0;
    // const int refinements = 4;
    const mfem::Element::Type el_type =
        mfem::Element::TETRAHEDRON;

    auto mesh_ptr = std::make_shared<mfem::Mesh>(
        mfem::Mesh::MakeCartesian3D(n, n, n, el_type));

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
    gmres.SetRelTol(1e-6);
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

    EXPECT_LT(rel_res, 1e-6);
}
