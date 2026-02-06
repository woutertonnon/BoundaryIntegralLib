#ifndef STOKES_DGS_H
#define STOKES_DGS_H

#include "mfem.hpp"
#include "StokesOperator.hpp"

namespace StokesNitsche
{

enum SmootherType
{
    GAUSS_SEIDEL_FORW,
    GAUSS_SEIDEL_BACK,
};

class StokesNitscheDGS: public mfem::Solver
{
private:
    StokesNitscheOperator& op_;
    const SmootherType st_ = GAUSS_SEIDEL_FORW;

    mfem::IdentityOperator id_u_;
    std::unique_ptr<mfem::SparseMatrix> grad_adj_;
    std::unique_ptr<mfem::SparseMatrix> Lu_, Lp_, bd_;
    std::unique_ptr<mfem::BlockOperator> T_;

    const mfem::Array<int> offsets_T_;
    mutable mfem::Vector residual_, corr_;

    void initTransformation();
    void initTransformedSystem();
    void computeResidual(const mfem::Vector& x,
                               mfem::Vector& y) const;
    void computeCorrection(const SmootherType st) const;
    void distributeCorrection(mfem::Vector& y) const;
public:
    StokesNitscheDGS(StokesNitscheOperator& op,
                     const SmootherType type = GAUSS_SEIDEL_FORW);

    void Mult(const mfem::Vector& x,
                    mfem::Vector& y) const override;
    void SetOperator(const mfem::Operator&) override;
};

} // namespace StokesNitsche

#endif
