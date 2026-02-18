#ifndef STOKES_MG_HPP
#define STOKES_MG_HPP

#include "mfem.hpp"
#include "StokesOperator.hpp"
#include "StokesDGS.hpp"
#include <vector>
#include <memory>

namespace StokesNitsche
{

enum class MGCycleType { VCycle, WCycle };

class StokesMG : public mfem::Solver
{
public:
    /** @brief Geometric Multigrid solver for Stokes-Nitsche systems.
     * Defaults to operator mode(s): DEC.
     *
     * Can NOT solve the true FEM system by itself,
     * use it as a precontioner with e.g. GMRES.
     *
     * If use it to solve the Galerkin (FEM) system, remember to do
     * setOperatorMode(StokesNitsche::OperatorMode::Galerkin)
     * before passing it as a precontioner. */
    StokesMG(std::shared_ptr<mfem::Mesh> coarse_mesh,
             const double theta,
             const double penalty,
             const double factor,
             const MassLumping ml = MassLumping::Diagonal,
             const SmootherType st = SmootherType::GaussSeidelForw);

    ~StokesMG() override = default;

    StokesMG(const StokesMG&) = delete;
    StokesMG& operator=(const StokesMG&) = delete;

    /** @brief Refines the finest mesh and adds a new MG level. */
    void addRefinedLevel();

    void setCoarseSolver(std::shared_ptr<const mfem::Solver> solver)
    {
        coarse_solver_ = std::move(solver);
    }

    void setSmoothIterations(const int pre, const int post)
    {
        pre_smooth_ = pre;
        post_smooth_ = post;
    }

    void setCycleType(const MGCycleType type) { cycle_type_ = type; }
    void setIterativeMode(const bool mode) { iterative_mode = mode; }

    /** @brief Set the mode of the input system.
     * If Galerkin: Input 'b' is scaled by M_lumped^{-1} before the MG cycle.
     * If DEC: Input 'b' is used as-is. */
    void setOperatorMode(const OperatorMode mode) const { mode_ = mode; }

    void Mult(const mfem::Vector& b, mfem::Vector& x) const override;
    void SetOperator(const mfem::Operator& op) override;

    const StokesNitscheOperator& getOperator(const int level) const
    {
        MFEM_VERIFY(level >= 0 && level < levels_.size(),
                    "StokesMG::getOperator: Level index out of bounds.");
        return *levels_[level]->op;
    }

    const StokesNitscheDGS& getSmoother(const int level) const
    {
        MFEM_VERIFY(level >= 0 && level < levels_.size(),
                    "StokesMG::getSmoother: Level index out of bounds.");
        return *levels_[level]->smoother;
    }

    const StokesNitscheOperator& getFinestOperator() const
    {
        MFEM_VERIFY(!levels_.empty(),
                    "StokesMG::getFinestOperator: No levels exist.");
        return *levels_.back()->op;
    }

    const StokesNitscheDGS& getFinestSmoother() const
    {
        MFEM_VERIFY(!levels_.empty(),
                    "StokesMG::getFinestSmoother: No levels exist.");
        return *levels_.back()->smoother;
    }

    MGCycleType getCycleType() const { return cycle_type_; }
    MassLumping getMassLumping() const { return ml_; }
    SmootherType getSmootherType() const { return st_; }
    int getNumLevels() const { return static_cast<int>(levels_.size()); }
    OperatorMode getOperatorMode() const { return mode_; }

private:
    struct Level
    {
        const std::shared_ptr<const StokesNitscheOperator> op;
        const std::shared_ptr<const StokesNitscheDGS> smoother;
        const std::unique_ptr<const mfem::Operator> P;
        const std::unique_ptr<const mfem::Operator> R;

        mutable mfem::Vector x, b, res;

        Level(std::shared_ptr<const StokesNitscheOperator> o,
              std::shared_ptr<const StokesNitscheDGS> s,
              std::unique_ptr<const mfem::Operator> p,
              std::unique_ptr<const mfem::Operator> r)
            : op(std::move(o)),
              smoother(std::move(s)),
              P(std::move(p)),
              R(std::move(r)),
              x(op->NumRows()),
              b(op->NumRows()),
              res(op->NumRows())
        { }
    };

    std::vector<std::unique_ptr<Level>> levels_;
    std::shared_ptr<const mfem::Solver> coarse_solver_;

#ifdef MFEM_USE_SUITESPARSE
    std::unique_ptr<mfem::SparseMatrix> coarse_mat_;
    std::unique_ptr<mfem::UMFPackSolver> umf_solver_;
    mutable mfem::Vector coarse_b_ext_;
    mutable mfem::Vector coarse_x_ext_;
#endif

    const double theta_, penalty_, factor_;
    const MassLumping ml_;
    const SmootherType st_;

    int pre_smooth_ = 1;
    int post_smooth_ = 1;
    MGCycleType cycle_type_ = MGCycleType::VCycle;
    mutable OperatorMode mode_ = OperatorMode::Galerkin;

    mutable mfem::Vector b_scaled_;

    void cycle(const int level_idx,
               const mfem::Vector& b,
               mfem::Vector& x) const;

    void buildTransfers(const StokesNitscheOperator& coarse,
                        const StokesNitscheOperator& fine,
                        std::unique_ptr<const mfem::Operator>& P,
                        std::unique_ptr<const mfem::Operator>& R) const;
};

} // namespace StokesNitsche

#endif
