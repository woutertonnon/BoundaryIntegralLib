#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <complex>
#include <memory>
#include <cmath>
#include <random>
#include <iomanip>

#include <boost/program_options.hpp>
#include <Eigen/Dense>

#include "mfem.hpp"
#include "StokesMG.hpp"
#include "StokesOperator.hpp"
#include "SpectraErrorOp.hpp"

namespace po = boost::program_options;

// Solves A x = b with random b using GMRES preconditioned by P
int runGMRES(const mfem::Operator& A,
             mfem::Solver& P,
             const double tol,
             const int max_iter = 1000,
             const int restart = 100)
{
    mfem::GMRESSolver gmres;
    gmres.SetOperator(A);
    gmres.SetPreconditioner(P);
    gmres.SetAbsTol(1e-12);
    gmres.SetRelTol(tol);
    gmres.SetMaxIter(max_iter);
    gmres.SetPrintLevel(0);
    gmres.SetKDim(restart);

    int num_rows = A.NumRows();
    mfem::Vector x(num_rows), b(num_rows);
    mfem::Vector x_exact(num_rows);

    x_exact.Randomize();
    A.Mult(x_exact, b);

    x = 0.0;
    gmres.Mult(b, x);

    if (!gmres.GetConverged())
    {
        std::cerr << "Warning: GMRES failed to converge.\n";
        return max_iter;
    }

    return gmres.GetNumIterations();
}

int main(int argc, char* argv[])
{
#ifdef NDEBUG
    mfem::Device("omp");
#endif

    std::string mesh_file, output_file, cycle_str;
    int max_refinements, nev, n_gmres;
    double tol;
    bool verbose;

    // Parse command line options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("mesh,m", po::value<std::string>(&mesh_file)->required(), "mesh filename")
        ("refinements,r", po::value<int>(&max_refinements)->required(), "number of refinements")
        ("output,o", po::value<std::string>(&output_file)->default_value("out.csv"), "output csv filename")
        ("nev,n", po::value<int>(&nev)->default_value(1), "number of eigenvalues (0 to skip)")
        ("gmres,g", po::value<int>(&n_gmres)->default_value(1), "number of GMRES runs")
        ("verbose,v", po::bool_switch(&verbose)->default_value(false), "enable verbose output")
        ("tol,t", po::value<double>(&tol)->default_value(1e-4), "tolerance")
        ("cycle,c", po::value<std::string>(&cycle_str)->default_value("V"), "cycle type (V or W)");

    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help"))
        {
            std::cout << desc << "\n";
            return 0;
        }
        po::notify(vm);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    const double theta = 1.0,
                 penalty = 10.0,
                 factor = 1.0;
    auto mesh_ptr = std::make_shared<mfem::Mesh>(mesh_file.c_str(), 1, 1);
    StokesNitsche::StokesMG mg(mesh_ptr, theta, penalty, factor);

    if (cycle_str == "W" || cycle_str == "w")
        mg.setCycleType(StokesNitsche::MGCycleType::WCycle);
    else
        mg.setCycleType(StokesNitsche::MGCycleType::VCycle);

    mg.setSmoothIterations(1, 1);

    // Prepare Output
    std::ofstream out(output_file);
    if (!out)
    {
        std::cerr << "Cannot open " << output_file << "\n";
        return 1;
    }

    out << "Refinements,DOFs,AvgGMRES";
    for (int i = 0; i < nev; ++i)
        out << ",AbsEval" << i;
    out << "\n";

    if (verbose)
    {
        std::cout << std::string(75, '=') << "\n";
        std::cout << "Mesh: " << mesh_file << ", Refinements: " << max_refinements << "\n";
        std::cout << std::string(75, '=') << "\n";
    }

    // Refinement Loop
    for (int r = 1; r <= max_refinements; ++r)
    {
        mg.addRefinedLevel();

        const auto& finest_op = mg.getFinestOperator();
        const int dofs = finest_op.NumRows();

        if (verbose)
        {
            std::cout << "Refinement Level " << r << " (" << dofs << " DOFs)\n";
            std::cout << std::string(75, '-') << "\n";
        }

        // 1. Compute Eigenvalues (DEC Mode)
        finest_op.setOperatorMode(StokesNitsche::OperatorMode::DEC);
        mg.setOperatorMode(StokesNitsche::OperatorMode::DEC);
        mg.setIterativeMode(false);

        Eigen::VectorXcd evals;
        if (nev > 0)
            evals = computeErrorOperatorEigenvalues(finest_op, mg, nev, tol, verbose);

        // 2. Run GMRES (Galerkin Mode)
        finest_op.setOperatorMode(StokesNitsche::OperatorMode::Galerkin);
        mg.setOperatorMode(StokesNitsche::OperatorMode::Galerkin);
        mg.setIterativeMode(false);

        double avg_gmres = 0.0;
        if (n_gmres > 0)
        {
            long total = 0;
            for (int i = 0; i < n_gmres; ++i)
                total += runGMRES(finest_op, mg, tol);
            avg_gmres = static_cast<double>(total) / n_gmres;
        }

        // 3. Write to CSV
        out << r << "," << dofs << "," << avg_gmres;
        for (int i = 0; i < nev; ++i)
        {
            if (i < evals.size())
                out << "," << std::abs(evals[i]);
            else
                out << ",NaN";
        }
        out << "\n";
        out.flush();

        if (verbose)
        {
            if (n_gmres > 0)
                std::cout << "GMRES: Avg Iterations: " << avg_gmres << "\n";

            std::cout << std::string(75, '=') << "\n";
        }
    }

    if (verbose)
        std::cout << "Results saved to " << output_file << "\n";

    return 0;
}
