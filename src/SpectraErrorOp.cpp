#include "SpectraErrorOp.hpp"
#include <Spectra/GenEigsSolver.h>
#include <iostream>
#include <algorithm>

ErrorOperator::ErrorOperator(const mfem::Operator& mat,
                             const mfem::Operator& prec)
    : mfem::Operator(mat.Height()),
      matOp(mat),
      precOp(prec),
      zVec(mat.Height())
{
    MFEM_VERIFY(mat.Height() == mat.Width(),
                "Matrix must be square");
    MFEM_VERIFY(prec.Height() == prec.Width(),
                "Preconditioner must be square");
    MFEM_VERIFY(mat.Height() == prec.Height(),
                "Matrix and Preconditioner dimensions must match");
}

void ErrorOperator::Mult(const mfem::Vector& x, mfem::Vector& y) const
{
    y = x;
    matOp.Mult(x, zVec);
    precOp.AddMult(zVec, y, -1.0);
}

SpectraAdapter::SpectraAdapter(const mfem::Operator& op)
    : mfemOp(op),
      xVec(const_cast<double*>(static_cast<double*>(nullptr)), 0),
      yVec(static_cast<double*>(nullptr), 0)
{}

int SpectraAdapter::rows() const
{
    return mfemOp.Height();
}

int SpectraAdapter::cols() const
{
    return mfemOp.Width();
}

void SpectraAdapter::perform_op(const double* xIn, double* yOut) const
{
    xVec.SetDataAndSize(const_cast<double*>(xIn), mfemOp.Width());
    yVec.SetDataAndSize(yOut, mfemOp.Height());

    mfemOp.Mult(xVec, yVec);

    xVec.SetDataAndSize(nullptr, 0);
    yVec.SetDataAndSize(nullptr, 0);
}

Eigen::VectorXcd computeErrorOperatorEigenvalues(
    const mfem::Operator& mat,
    const mfem::Operator& prec,
    const int numEigenvalues,
    const double tol, // <--- New Parameter
    const bool printResults)
{
    ErrorOperator errorOp(mat, prec);
    SpectraAdapter spectraOp(errorOp);

    const int ncv = std::min(2 * numEigenvalues + 1,
                             errorOp.Height());

    Spectra::GenEigsSolver<SpectraAdapter> eigs(
        spectraOp, numEigenvalues, ncv
    );

    eigs.init();

    // Pass tolerance here (max iterations default is 1000, can also be parameterized)
    const int nConv = eigs.compute(Spectra::SortRule::LargestMagn, 1000, tol);

    Eigen::VectorXcd results;

    if (eigs.info() == Spectra::CompInfo::Successful)
    {
        results = eigs.eigenvalues();

        if (printResults)
        {
            std::cout << "Spectra: Computed " << nConv << " converged eigenvalues for Error Operator.\n";
            std::cout << "------------------------------------------------\n";
            for (int i = 0; i < numEigenvalues; i++)
            {
                std::complex<double> ev = results(i);
                std::cout << "Eigenvalue " << i << ": "
                          << ev.real() << (ev.imag() >= 0 ? " + " : " - ")
                          << std::abs(ev.imag()) << "i  (Mag: " << std::abs(ev) << ")\n";
            }
            std::cout << "------------------------------------------------\n";
        }
    }
    else
    {
        std::cerr << "Spectra computation failed or did not converge.\n";
        return Eigen::VectorXcd();
    }

    return results;
}
