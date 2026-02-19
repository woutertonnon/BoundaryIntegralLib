#include "mfem.hpp"
#include "BoundaryOperators.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <memory>
#include <vector>
#include <string>

// --- Configuration Constants ---
constexpr double PENALTY = 10.0;
constexpr double THETA   = 1.0;
constexpr double FACTOR  = 1.0;

// --- Exact Solution & Source Functions ---

void u_exact_func(const mfem::Vector& x, mfem::Vector& u) {
    u(0) = x(1) * x(1);
    u(1) = x(2) * x(2);
    u(2) = x(0) * x(0);
}

double p_exact_func(const mfem::Vector& x) {
    // Mean-zero pressure adjustment based on domain [0,1]^3
    return x(0) + x(1) + x(2) - 1.5;
}

void f_rhs_func(const mfem::Vector& x, mfem::Vector& f) {
    // Based on the Laplacian of u_exact and gradient of p_exact
    f(0) = -1.0;
    f(1) = -1.0;
    f(2) = -1.0;
}

// --- System Assembly ---

mfem::SparseMatrix* assembleSystem(mfem::FiniteElementSpace* h1_space,
                                   mfem::FiniteElementSpace* hcurl_space)
{
    mfem::ConstantCoefficient one(1.0);
    const int nv = h1_space->GetNDofs();
    const int ne = hcurl_space->GetNDofs();

    // Block structure: [ CurlCurl  Grad   0 ] [ u ]
    //                  [ Grad^T    0      M ] [ p ]
    //                  [ 0         M^T    0 ] [ L ]
    mfem::Array<int> offsets({0, ne, ne + nv, ne + nv + 1});
    auto block = std::make_unique<mfem::BlockMatrix>(offsets);

    // 1. Gradient Block (G)
    mfem::MixedBilinearForm G(h1_space, hcurl_space);
    G.AddDomainIntegrator(new mfem::MixedVectorGradientIntegrator(one));
    G.Assemble();
    G.Finalize();
    std::unique_ptr<mfem::SparseMatrix> grad(G.LoseMat());

    // 2. -Divergence Block (G^T)
    std::unique_ptr<mfem::SparseMatrix> gradT(mfem::Transpose(*grad));

    // 3. Curl-Curl Block + Nitsche Boundary Terms
    mfem::BilinearForm cc(hcurl_space);
    cc.AddDomainIntegrator(new mfem::CurlCurlIntegrator(one));
    cc.AddBdrFaceIntegrator(new ND_NitscheIntegrator(THETA, PENALTY, FACTOR));
    cc.Assemble();
    cc.Finalize();
    std::unique_ptr<mfem::SparseMatrix> curlcurl(cc.LoseMat());

    // 4. Mean Constraint Block (Pressure uniqueness)
    mfem::SparseMatrix mean(1, nv);
    mfem::Array<int> cols(nv);
    mfem::Vector ones(nv);
    for (int k = 0; k < nv; ++k) cols[k] = k;
    ones = 1.0;
    mean.AddRow(0, cols, ones);
    mean.Finalize();
    std::unique_ptr<mfem::SparseMatrix> meanT(mfem::Transpose(mean));

    // 5. Build Monolithic Matrix
    block->SetBlock(0, 0, curlcurl.get());
    block->SetBlock(0, 1, grad.get());
    block->SetBlock(1, 0, gradT.get());
    block->SetBlock(2, 1, &mean);
    block->SetBlock(1, 2, meanT.get());

    return block->CreateMonolithic();
}

// --- Convergence Study ---

void RunStokesMGStudy(std::shared_ptr<mfem::Mesh> mesh, const int max_refs) {
    mfem::VectorFunctionCoefficient u_coeff(3, u_exact_func);
    mfem::FunctionCoefficient       p_coeff(p_exact_func);
    mfem::VectorFunctionCoefficient f_coeff(3, f_rhs_func);

    // Header Printing
    std::cout << "\n" << std::setw(6)  << "Level"
              << std::setw(10) << "DOFs"
              << std::setw(14) << "Err(u)" << std::setw(8) << "Rate"
              << std::setw(14) << "Err(p)" << std::setw(8) << "Rate" << "\n";
    std::cout << std::string(62, '-') << std::endl;

    double err_u_prev = 0.0, err_p_prev = 0.0;

    for (int l = 0; l <= max_refs; ++l) {
        if (l > 0) mesh->UniformRefinement();

        // Setup Finite Element Spaces
        mfem::ND_FECollection hcurl_fec(1, 3);
        mfem::FiniteElementSpace hcurl(mesh.get(), &hcurl_fec);

        mfem::H1_FECollection h1_fec(1, 3);
        mfem::FiniteElementSpace h1(mesh.get(), &h1_fec);

        const int nu = hcurl.GetNDofs();
        const int np = h1.GetNDofs();

        // System Assembly
        std::unique_ptr<mfem::SparseMatrix> A(assembleSystem(&h1, &hcurl));

        mfem::Vector rhs(nu + np + 1);
        rhs = 0.0;

        mfem::LinearForm fu(&hcurl, rhs.GetData());
        fu.AddBdrFaceIntegrator(new ND_NitscheLFIntegrator(THETA, PENALTY, u_coeff, FACTOR));
        fu.AddDomainIntegrator(new mfem::VectorFEDomainLFIntegrator(f_coeff));
        fu.Assemble();

        // Solver
        mfem::Vector x(nu + np + 1);
        x = 0.0;
        mfem::UMFPackSolver solver;
        solver.SetOperator(*A);
        solver.Mult(rhs, x);

        // Error Analysis
        mfem::GridFunction u_h(&hcurl, x.GetData());
        mfem::GridFunction p_h(&h1, x.GetData() + nu);

        double err_u = u_h.ComputeL2Error(u_coeff);
        double err_p = p_h.ComputeL2Error(p_coeff);

        double rate_u = (l > 0) ? std::log2(err_u_prev / err_u) : 0.0;
        double rate_p = (l > 0) ? std::log2(err_p_prev / err_p) : 0.0;

        // Print Stats
        std::cout << std::setw(6)  << l
                  << std::setw(10) << nu + np
                  << std::scientific << std::setprecision(3)
                  << std::setw(14) << err_u << std::fixed << std::setprecision(2) << std::setw(8) << rate_u
                  << std::scientific << std::setprecision(3)
                  << std::setw(14) << err_p << std::fixed << std::setprecision(2) << std::setw(8) << rate_p
                  << std::endl;

        err_u_prev = err_u;
        err_p_prev = err_p;

        // Export Results
        mfem::ParaViewDataCollection pd("Stokes_Results", mesh.get());
        pd.SetDataFormat(mfem::VTKFormat::BINARY);
        pd.RegisterField("velocity", &u_h);
        pd.RegisterField("pressure", &p_h);
        pd.SetCycle(l);
        pd.Save();
    }
    std::cout << std::string(62, '-') << "\n" << std::endl;
}

int main() {
    // Initial mesh: 2x2x2 cube
    auto mesh = std::make_shared<mfem::Mesh>(
        mfem::Mesh::MakeCartesian3D(2, 2, 2, mfem::Element::HEXAHEDRON)
    );

    // Run study for 3 levels of refinement
    RunStokesMGStudy(mesh, 3);

    return 0;
}
