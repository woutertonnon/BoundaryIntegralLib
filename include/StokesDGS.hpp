#ifndef STOKES_DGS_HPP
#define STOKES_DGS_HPP

#include "mfem.hpp"
#include "StokesOperator.hpp"
#include <memory>

namespace StokesNitsche
{

enum class SmootherType
{
    GaussSeidelForw,
    GaussSeidelSym,
    Jacobi,
    Chebyshev,
    Custom
};

/**
 * @brief Distributed Gauss-Seidel (DGS) Preconditioner/Solver for Stokes-Nitsche systems.
 *
 * This class applies a block lower-triangular preconditioner to the transformed
 * Stokes system. It relies on a transformation matrix T to decouple the velocity
 * and pressure updates mathematically.
 */
class StokesNitscheDGS : public mfem::Solver
{
public:
    StokesNitscheDGS(std::shared_ptr<StokesNitscheOperator> op,
                     const SmootherType type = SmootherType::GaussSeidelForw);

    ~StokesNitscheDGS() override = default;

    StokesNitscheDGS(const StokesNitscheDGS&) = delete;
    StokesNitscheDGS& operator=(const StokesNitscheDGS&) = delete;
    StokesNitscheDGS(StokesNitscheDGS&&) = delete;
    StokesNitscheDGS& operator=(StokesNitscheDGS&&) = delete;

    void SetOperator(const mfem::Operator& op) override;

    /**
     * @brief Applies the DGS preconditioner: y = y + B^{-1}(x - A y)
     * @param x Right-hand side vector.
     * @param y Solution vector (initial guess on input, updated on output).
     */
    void Mult(const mfem::Vector& x, mfem::Vector& y) const override;

    double computeResidualNorm(const mfem::Vector& x,
                               const mfem::Vector& y) const;

    /**
     * @brief Injects a custom solver for the velocity block (L_u).
     */
    void SetSmootherU(std::unique_ptr<mfem::Solver> smoother);

    /**
     * @brief Injects a custom solver for the pressure block (L_p).
     */
    void SetSmootherP(std::unique_ptr<mfem::Solver> smoother);

private:
    std::shared_ptr<StokesNitscheOperator> op_;
    const SmootherType st_;

    mfem::IdentityOperator id_u_;

    std::unique_ptr<mfem::SparseMatrix> grad_adj_;
    std::unique_ptr<mfem::SparseMatrix> Lu_;
    std::unique_ptr<mfem::SparseMatrix> Lp_;
    std::unique_ptr<mfem::SparseMatrix> bd_;
    std::unique_ptr<mfem::BlockOperator> T_;

    std::unique_ptr<mfem::Solver> smoother_u_;
    std::unique_ptr<mfem::Solver> smoother_p_;
    std::unique_ptr<mfem::BlockLowerTriangularPreconditioner> block_prec_;

    mutable mfem::Vector residual_;
    mutable mfem::Vector corr_;

    void initTransformation();
    void initTransformedSystem();
    void initSmoothers();
    void buildBlockPreconditioner();

    void computeResidual(const mfem::Vector& x, const mfem::Vector& y) const;
    void computeCorrection() const;
    void distributeCorrection(mfem::Vector& y) const;
};

} // namespace StokesNitsche

#endif
