#include "StokesMG.hpp"
#include "BoundaryOperators.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <memory>
#include <vector>
#include <string>

#define DIRECT_SOLVE true

void u_exact_func(const mfem::Vector& x, mfem::Vector& u)
{
    u(0) = x(1) * x(1);
    u(1) = x(2) * x(2);
    u(2) = x(0) * x(0);
}

double p_exact_func(const mfem::Vector& x)
{
    return x(0) + x(1) + x(2) - 1.5;
}

void f_rhs_func(const mfem::Vector& x, mfem::Vector& f)
{
    f(0) = -1.0;
    f(1) = -1.0;
    f(2) = -1.0;
}

void RunStokesMGStudy(std::shared_ptr<mfem::Mesh> mesh_ptr, const int max_refs)
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

    std::cout << (DIRECT_SOLVE ? "Using UMFPACK" : "Using MG")
              << std::endl << std::endl;;

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
        if (l > 0)
            mg_solver.addRefinedLevel();

        // for whathever reason, mfem wants non const pointers to the fe spaces
        // TODO: Fix
        StokesNitsche::StokesNitscheOperator& op =
          *const_cast<StokesNitsche::StokesNitscheOperator*>(
            &mg_solver.getFinestOperator()
          );

        // Access the current mesh (const is fine for saving)
        // We cast away const here just to satisfy ParaViewDataCollection constructor
        mfem::Mesh& current_mesh = const_cast<mfem::Mesh&>(op.getMesh());

        op.setOperatorMode(StokesNitsche::OperatorMode::Galerkin);

        const unsigned int nu = op.getHCurlSpace().GetNDofs(),
                           np = op.getH1Space().GetNDofs();

        mfem::Vector rhs(nu + np + DIRECT_SOLVE);
        rhs = 0.0;

        mfem::FiniteElementSpace& hcurl = op.getHCurlSpace();
        mfem::FiniteElementSpace& h1 = op.getH1Space();

        mfem::LinearForm fu(&hcurl, rhs.GetData());
        fu.AddBdrFaceIntegrator(
            new ND_NitscheLFIntegrator(theta, penalty, u_coeff, factor)
        );
        fu.AddDomainIntegrator(
            new mfem::VectorFEDomainLFIntegrator(f_coeff)
        );
        fu.Assemble();

        mfem::Vector x(nu + np + DIRECT_SOLVE);
        x = 0.0;


        mfem::GMRESSolver gmres;

        if(DIRECT_SOLVE)
        {
            mfem::UMFPackSolver solver;
            mfem::SparseMatrix A = *op.getFullGalerkinSystem();
            solver.SetOperator(A);
            solver.Mult(rhs, x);
        }
        else
        {
            gmres.SetAbsTol(1e-12);
            gmres.SetRelTol(1e-9);
            gmres.SetMaxIter(500);
            gmres.SetPrintLevel(0);
            gmres.SetOperator(op);
            gmres.SetPreconditioner(mg_solver);
            gmres.SetKDim(128);

            gmres.Mult(rhs, x);
            op.eliminateConstants(x);
        }
        // mfem::Vector x_u(x.GetData(), nu);
        // mfem::Vector x_p(x.GetData() + nu, np);
        //
        // mfem::Vector ones(np);
        // ones = 1.0;
        // // Should be 0
        // std::cout << op.getMassH1().InnerProduct(ones, x_p) << std::endl;

        // --- Post-Processing & Error ---

        // 1. Recover Solution GridFunctions
        mfem::GridFunction u_h(&hcurl, x.GetData());
        mfem::GridFunction p_h(&h1, x.GetData() + nu);

        const double err_u = u_h.ComputeL2Error(u_coeff),
                     err_p = p_h.ComputeL2Error(p_coeff);

        double rate = 0.0;
        if (l > 0)
            rate = std::log2(err_u_prev / err_u);

        std::cout << std::setw(10) << l
                  << std::setw(15) << op.Height()
                  << std::setw(15) << gmres.GetNumIterations()
                  << std::setw(15) << std::scientific << err_u
                  << std::setw(15) << std::fixed << std::setprecision(2) << rate << std::endl;

        err_u_prev = err_u;

        // ============================================================
        //  VTK OUTPUT SECTION
        // ============================================================

        // 2. Prepare Exact Solutions as GridFunctions for comparison
        mfem::GridFunction u_exact_gf(&hcurl);
        u_exact_gf.ProjectCoefficient(u_coeff);

        mfem::GridFunction p_exact_gf(&h1);
        p_exact_gf.ProjectCoefficient(p_coeff);

        // 3. Setup ParaView Collection
        // Naming convention: Stokes_Refinement_L0, Stokes_Refinement_L1, etc.
        std::string name = "Stokes_Refinement_L" + std::to_string(l);

        mfem::ParaViewDataCollection pd(name, &current_mesh);
        pd.SetLevelsOfDetail(1); // Standard resolution
        pd.SetDataFormat(mfem::VTKFormat::BINARY); // Binary is smaller/faster than ASCII
        pd.SetHighOrderOutput(true); // Preserve high-order info if needed

        // 4. Register Fields
        pd.RegisterField("velocity", &u_h);
        pd.RegisterField("pressure", &p_h);
        pd.RegisterField("velocity_exact", &u_exact_gf);
        pd.RegisterField("pressure_exact", &p_exact_gf);

        // 5. Save to disk
        pd.Save();
    }
    std::cout << std::string(70, '-') << std::endl;
}

int main(int argc, char *argv[])
{
    const unsigned int n = 1;
    auto mesh_ptr = std::make_shared<mfem::Mesh>(
      mfem::Mesh::MakeCartesian3D(
        n, n, n, mfem::Element::HEXAHEDRON
      )
    );
    RunStokesMGStudy(mesh_ptr, 3);
    return 0;
}
