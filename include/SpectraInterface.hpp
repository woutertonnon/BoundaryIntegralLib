#ifndef SPECTRA_MFEM_OP_HPP
#define SPECTRA_MFEM_OP_HPP

#include <mfem.hpp>
#include <complex>
#include <vector>

class SpectraMfemOp
{
private:
    const mfem::Operator& mfemOp;
    mutable mfem::Vector xVec;
    mutable mfem::Vector yVec;

public:
    using Scalar = double;

    SpectraMfemOp(const mfem::Operator& op);

    int rows() const;
    int cols() const;

    void perform_op(const double* xIn, double* yOut) const;
};

std::vector<std::complex<double>> computeLargestEigenvalues(
    const mfem::Operator& op,
    const int numEigenvalues,
    const bool printResults = true
);

#endif // SPECTRA_MFEM_OP_HPP
