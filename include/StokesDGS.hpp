#ifndef STOKES_DGS_HPP
#define STOKES_DGS_HPP

#include "mfem.hpp"
#include "StokesOperator.hpp"

namespace StokesNitsche
{

enum class SmootherType { GaussSeidelForw, GaussSeidelSym, Jacobi };

class StokesNitscheDGS : public mfem::Solver
{
public:
    /** @brief Distributed Gauss-Seidel Smoother for Stokes-Nitsche systems. */
    StokesNitscheDGS(std::shared_ptr<StokesNitscheOperator> op,
                     const SmootherType type = SmootherType::GaussSeidelForw);

    ~StokesNitscheDGS() override = default;

    StokesNitscheDGS(const StokesNitscheDGS&) = delete;
    StokesNitscheDGS& operator=(const StokesNitscheDGS&) = delete;
    StokesNitscheDGS(StokesNitscheDGS&&) = delete;
    StokesNitscheDGS& operator=(StokesNitscheDGS&&) = delete;

    void SetOperator(const mfem::Operator& op) override;

    void Mult(const mfem::Vector& x, mfem::Vector& y) const override;

    double computeResidualNorm(const mfem::Vector& x,
                               const mfem::Vector& y) const;

private:
    std::shared_ptr<StokesNitscheOperator> op_;
    const SmootherType st_;

    mfem::IdentityOperator id_u_;

    std::unique_ptr<mfem::SparseMatrix> grad_adj_;
    std::unique_ptr<mfem::SparseMatrix> Lu_;
    std::unique_ptr<mfem::SparseMatrix> Lp_;
    std::unique_ptr<mfem::SparseMatrix> bd_;
    std::unique_ptr<mfem::BlockOperator> T_;

    mutable mfem::Vector residual_;
    mutable mfem::Vector corr_;

    void initTransformation();
    void initTransformedSystem();

    void computeResidual(const mfem::Vector& x, const mfem::Vector& y) const;
    void computeCorrection(const SmootherType st) const;
    void distributeCorrection(mfem::Vector& y) const;
};

} // namespace StokesNitsche

#endif
