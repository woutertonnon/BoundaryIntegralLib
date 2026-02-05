#ifndef STOKES_DGS_H
#define STOKES_DGS_H

namespace StokesNitsche
{

enum SmootherType
{
  GAUSS_SEIDEL_FORWARD,
  GAUSS_SEIDEL_BACKWARD,
  JACOBI
};

class StokesNitscheDGS: mfem::Solver
{
private:
    StokesNitscheOperator& op_;
    const SmootherType ST = GAUSS_SEIDEL_FORWARD;
    mfem::SparseMatrix A11, A12, A21, A22;

    mutable mfem::Vector residual_, corr_;
public:
    StokesNitscheDGS(StokesNitscheOperator& op,
                     const SmootherType type = GAUSS_SEIDEL_FORWARD);
    void Mult(const mfem::Vector& x,
                    mfem::Vector& y) const override;
};

} // namespace StokesNitsche

#endif
