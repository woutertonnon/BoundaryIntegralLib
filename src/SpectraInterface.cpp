#include "SpectraMfemOp.hpp"
#include <Spectra/GenEigsSolver.h>
#include <iostream>
#include <algorithm>

SpectraMfemOp::SpectraMfemOp(const mfem::Operator& op)
    : mfemOp(op),
      xVec(const_cast<double*>(static_cast<double*>(nullptr)), 0),
      yVec(static_cast<double*>(nullptr), 0)
{
    MFEM_VERIFY(op.Height() == op.Width(),
                "Spectra requires a square mfem::Operator");
}

int SpectraMfemOp::rows() const
{
    return mfemOp.Height();
}

int SpectraMfemOp::cols() const
{
    return mfemOp.Width();
}

void SpectraMfemOp::perform_op(const double* xIn, double* yOut) const
{
    xVec.SetDataAndSize(const_cast<double*>(xIn), mfemOp.Width());
    yVec.SetDataAndSize(yOut, mfemOp.Height());

    mfemOp.Mult(xVec, yVec);

    xVec.SetDataAndSize(nullptr, 0);
    yVec.SetDataAndSize(nullptr, 0);
}

std::vector<std::complex<double>> computeLargestEigenvalues(
    const mfem::Operator& op,
    const int numEigenvalues,
    const bool printResults
)
{
    std::vector<std::complex<double>> results;
    SpectraMfemOp spectraOp(op);

    int ncv = std::min(2 * numEigenvalues + 1, op.Height());
    Spectra::GenEigsSolver<SpectraMfemOp> eigs(spectraOp, numEigenvalues, ncv);

    eigs.init();
    int nConv = eigs.compute(Spectra::SortRule::LargestMagn);

    if (eigs.info() == Spectra::CompInfo::Successful)
    {
        Eigen::VectorXcd eigenValues = eigs.eigenvalues();
        results.reserve(numEigenvalues);

        if (printResults)
            std::cout << "Spectra: Computed " << nConv << " converged eigenvalues.\n";
            std::cout << "------------------------------------------------\n";

        for (int i = 0; i < numEigenvalues; i++)
        {
            std::complex<double> ev = eigenValues(i);
            results.push_back(ev);

            if (printResults)
                std::cout << "Eigenvalue " << i << ": "
                          << ev.real() << (ev.imag() >= 0 ? " + " : " - ")
                          << std::abs(ev.imag()) << "i\n";
        }

        if (printResults)
            std::cout << "------------------------------------------------\n";
    }
    else
        std::cerr << "Spectra computation failed or did not converge.\n";

    return results;
}
