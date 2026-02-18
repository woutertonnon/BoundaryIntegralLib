#include "StokesMG.hpp"
#include "BoundaryOperators.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <memory>
#include <vector>

// ============================================================================
// 1. Exact Solutions (Analytic Test Case)
// ============================================================================

// Exact Velocity: u = ( y^2, z^2, x^2 )
// Div(u) = 0
// Delta u = (2, 2, 2)
void u_exact_func(const mfem::Vector& x, mfem::Vector& u)
{
    u(0) = x(1) * x(1);
    u(1) = x(2) * x(2);
    u(2) = x(0) * x(0);
}

// Exact Pressure: p = x + y + z - 3/2
// Mean of x on [0, 1] is 0.5, so mean(p) = 0
// Grad(p) = (1, 1, 1)
double p_exact_func(const mfem::Vector& x)
{
    return x(0) + x(1) + x(2) - 1.5;
}

// Source Term: f = -Delta u + grad p
// f = -(2, 2, 2) + (1, 1, 1) = (-1, -1, -1)
void f_rhs_func(const mfem::Vector& x, mfem::Vector& f)
{
    f(0) = -1.0;
    f(1) = -1.0;
    f(2) = -1.0;
}

// ============================================================================
// 2. Convergence Study Function
// ============================================================================

void RunStokesMGStudy(std::shared_ptr<mfem::Mesh> mesh_ptr,
                      const int max_refs)
{
    const double theta   = 1.0,
                 penalty = 10.0,
                 factor  = 1.0;

    StokesNitsche::StokesMG mg_solver(mesh_ptr, theta, penalty, factor);
    mg_solver.setOperatorMode(StokesNitsche::OperatorMode::Galerkin);
    mg_solver.setIterativeMode(false);

    mg_solver.setCycleType(StokesNitsche::MGCycleType::VCycle);
    mg_solver.setSmoothIterations(1, 1);

    mfem::VectorFunctionCoefficient u_coeff(3, u_exact_func);
    mfem::FunctionCoefficient p_coeff(p_exact_func);
    mfem::VectorFunctionCoefficient f_coeff(3, f_rhs_func);

    std::cout << "\n======================================================================\n";
    std::cout << " Stokes-Nitsche Geometric Multigrid Convergence Study\n";
    std::cout << " Cycle: V-Cycle " << "\n";
    std::cout << "======================================================================\n";
    std::cout << std::setw(10) << "Level"
              << std::setw(15) << "DOFs"
              << std::setw(15) << "GMRES Iters"
              << std::setw(15) << "L2 Error(u)"
              << std::setw(15) << "L2 Rate" << std::endl;
    std::cout << std::string(70, '-') << std::endl;

    double err_u_prev = 0.0;

    // 3. Refinement Loop
    for (int l = 0; l <= max_refs; ++l)
    {
        // Add level (skip on first iteration as constructor created level 0)
        if (l > 0)
            mg_solver.addRefinedLevel();

        // Get the System Operator for the finest level
        StokesNitsche::StokesNitscheOperator& op =
          *const_cast<StokesNitsche::StokesNitscheOperator*>(
            &mg_solver.getFinestOperator()
          );

        const mfem::Mesh& current_mesh = op.getMesh();

        op.setOperatorMode(StokesNitsche::OperatorMode::Galerkin);

        const unsigned int nu = op.getHCurlSpace().GetNDofs(),
                           np = op.getH1Space().GetNDofs();

        mfem::Vector rhs(nu + np);
        rhs = 0.0;

        mfem::FiniteElementSpace& hcurl = op.getHCurlSpace();
        mfem::FiniteElementSpace& h1 = op.getH1Space();
        // Assemble right into rhs
        mfem::LinearForm fu(&hcurl, rhs.GetData());
        fu.AddBdrFaceIntegrator(
          new ND_NitscheLFIntegrator(theta, penalty, u_coeff, factor)
        );
        fu.AddDomainIntegrator(
          new mfem::VectorFEDomainLFIntegrator(f_coeff)
        );
        fu.Assemble();

        mfem::Vector x(nu + np);
        x = 0.0;

        // --- Solve ---
        mfem::GMRESSolver gmres;
        gmres.SetAbsTol(1e-12);
        gmres.SetRelTol(1e-9);
        gmres.SetMaxIter(500);
        gmres.SetPrintLevel(0);
        gmres.SetOperator(op);
        gmres.SetPreconditioner(mg_solver);
        gmres.SetKDim(64);
        // gmres.SetPrintLevel(1);

        // Solve
        gmres.Mult(rhs, x);
        op.eliminateConstants(x);

        // D. Compute Errors
        mfem::GridFunction u_h(&hcurl, x.GetData());
        const double err_u = u_h.ComputeL2Error(u_coeff);

        mfem::GridFunction p_h(&h1, x.GetData() + nu);
        const double err_p = p_h.ComputeL2Error(p_coeff);

        double rate = 0.0;
        if (l > 0)
            rate = std::log2(err_u_prev / err_u);

        std::cout << std::setw(10) << l
                  << std::setw(15) << op.Height()
                  << std::setw(15) << gmres.GetNumIterations()
                  << std::setw(15) << std::scientific << err_u
                  << std::setw(15) << std::fixed << std::setprecision(2) << rate << std::endl;

        err_u_prev = err_u;
    }
    std::cout << std::string(70, '-') << std::endl;
}

int main(int argc, char *argv[])
{
    const unsigned int n = 2;
    auto mesh_ptr = std::make_shared<mfem::Mesh>(
      mfem::Mesh::MakeCartesian3D(
        n, n, n, mfem::Element::HEXAHEDRON
      )
    );
    RunStokesMGStudy(mesh_ptr, 3);
    return 0;
}
