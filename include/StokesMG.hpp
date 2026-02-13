#ifndef STOKES_MG_HPP
#define STOKES_MG_HPP

#include "mfem.hpp"
#include "StokesOperator.hpp"
#include "StokesDGS.hpp"
#include <vector>
#include <memory>

namespace StokesNitsche
{

enum class MGCycleType
{
    VCycle,
    WCycle
};

class StokesMG : public mfem::Solver
{
public:
    StokesMG(std::shared_ptr<mfem::Mesh> coarse_mesh,
             const double theta,
             const double penalty,
             const double factor,
             const MassLumping ml = MassLumping::Diagonal,
             const SmootherType st = GAUSS_SEIDEL_FORW);

    ~StokesMG() override = default;

    StokesMG(const StokesMG&) = delete;
    StokesMG& operator=(const StokesMG&) = delete;

    // --- Setup & Configuration ---

    void addRefinedLevel();

    void setCoarseSolver(std::shared_ptr<const mfem::Solver> solver);

    void setSmoothIterations(const int pre, const int post);

    void setCycleType(const MGCycleType type);

    // Set the mode of the input system.
    // If Galerkin: Input 'b' is scaled by M_lumped^{-1} before the MG cycle.
    // If DEC: Input 'b' is used as-is.
    void setOperatorMode(const OperatorMode mode);

    // --- MFEM Overrides ---

    void Mult(const mfem::Vector& b, mfem::Vector& x) const override;

    void SetOperator(const mfem::Operator& op) override;

    // --- Getters ---

    const StokesNitscheOperator& getOperator(const int level) const;
    const StokesNitscheDGS& getSmoother(const int level) const;
    const StokesNitscheOperator& getFinestOperator() const;
    const StokesNitscheDGS& getFinestSmoother() const;

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
        {
        }
    };

    std::vector<std::unique_ptr<Level>> levels_;

    // User-provided override
    std::shared_ptr<const mfem::Solver> coarse_solver_;

    // Default UMFPACK coarse solver components
#ifdef MFEM_USE_SUITESPARSE
    std::unique_ptr<mfem::SparseMatrix> coarse_mat_;
    std::unique_ptr<mfem::UMFPackSolver> umf_solver_;

    // Auxiliary vectors for extended system (if coarse_mat_ is larger than op)
    mutable mfem::Vector coarse_b_ext_;
    mutable mfem::Vector coarse_x_ext_;
#endif

    const double theta_;
    const double penalty_;
    const double factor_;
    const MassLumping ml_;
    const SmootherType st_;

    int pre_smooth_ = 1;
    int post_smooth_ = 1;
    MGCycleType cycle_type_ = MGCycleType::VCycle;

    // Default to DEC (expects point-values/pre-scaled inputs).
    OperatorMode mode_ = OperatorMode::DEC;
    mutable mfem::Vector b_scaled_; // Temporary vector for Galerkin scaling

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
