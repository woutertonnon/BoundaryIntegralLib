#include <gtest/gtest.h>
#include <iomanip>
#include "mfem.hpp"
#include "StokesDGS.hpp"
#include "StokesOperator.hpp"
#include "StokesMG.hpp"

TEST(StokesMGTest, VCycleConvergence)
{
#ifdef MFEM_USE_SUITESPARSE
    std::cout << "Using suitesparse" << std::endl;
#endif
    // 1. Setup Parameters
    const unsigned int n = 1;
    const double theta = 1.0,
                 penalty = 3.0,
                 factor = 1.0;
    const double tol = 1e-6;
    const int refinements = 4;

    const mfem::Element::Type el_type =
        mfem::Element::HEXAHEDRON;

    // 2. Initialize Coarse Mesh
    auto mesh_ptr = std::make_shared<mfem::Mesh>(
        mfem::Mesh::MakeCartesian3D(n, n, n, el_type)
    );

    // 3. Initialize MG Solver
    StokesNitsche::StokesMG mg(mesh_ptr, theta, penalty, factor);

    // 4. Build Hierarchy
    // We no longer need to maintain a separate fine_mesh;
    // StokesMG handles it internally.
    for (int i = 0; i < refinements; ++i)
        mg.addRefinedLevel();

    // 5. Retrieve Reference Operator
    // We get the operator directly from the MG hierarchy.
    // Note: The MG class ensures setDECMode() is called during addRefinedLevel.
    const auto& fine_op = mg.getFinestOperator();

    // 6. Setup Linear System (Ax = b)
    const int num_rows = fine_op.NumRows();
    mfem::Vector x_exact(num_rows),
                 b(num_rows),
                 x_sol(num_rows),
                 residual(num_rows);

    x_exact.Randomize(1);

    // Generate consistent RHS: b = A * x_exact
    fine_op.Mult(x_exact, b);

    // Start with zero guess
    x_sol = 0.0;

    // 7. Configure MG
    mg.setCycleType(StokesNitsche::MGCycleType::VCycle);
    mg.setSmoothIterations(1, 1);

    // 8. Run Convergence Loop
    double initial_norm = 0.0;
    const int max_iter = 50;

    std::cout << "\n  Iter | Rel. Residual \n";
    std::cout << "-------|---------------\n";

    for (int iter = 0; iter < max_iter; ++iter)
    {
        // Calculate true residual: r = b - A_fine * x
        residual = b;
        fine_op.AddMult(x_sol, residual, -1.0);

        double current_norm = residual.Norml2();

        if (iter == 0) initial_norm = current_norm;
        double rel_norm = current_norm / initial_norm;

        std::cout << "  " << std::setw(4) << iter << " | "
                  << std::scientific << std::setprecision(4)
                  << rel_norm << std::endl;

        if (rel_norm < tol) break;

        // Perform one V-Cycle
        mg.Mult(b, x_sol);
    }

    // 9. Final Assertion
    residual = b;
    fine_op.AddMult(x_sol, residual, -1.0);
    double final_rel_norm = residual.Norml2() / initial_norm;

    ASSERT_LT(final_rel_norm, tol)
        << "MG V-Cycle failed to converge within tolerance.";
}
