#include "StokesMG.hpp"
#include <iomanip>

namespace StokesNitsche
{

StokesMG::StokesMG(std::shared_ptr<mfem::Mesh> coarse_mesh,
                   const double theta,
                   const double penalty,
                   const double factor,
                   const MassLumping ml,
                   const SmootherType st)
    : mfem::Solver(0, 0),
      theta_(theta),
      penalty_(penalty),
      factor_(factor),
      ml_(ml),
      st_(st)
{
    iterative_mode = true;

    // 1. Create Level 0 Operator
    auto op = std::make_shared<StokesNitscheOperator>(
        coarse_mesh, theta_, penalty_, factor_, ml_
    );
    op->setDECMode();
    auto smoother = std::make_shared<StokesNitscheDGS>(op, st_);

    height = op->NumRows();
    width = op->NumCols();

    levels_.push_back(
        std::make_unique<Level>(
            std::move(op), std::move(smoother), nullptr, nullptr
        )
    );

    // 2. Setup Default Coarse Solver (UMFPACK)
#ifdef MFEM_USE_SUITESPARSE
    // We must persist the matrix because UMFPACK expects it to stay alive.
    coarse_mat_ = levels_[0]->op->getFullSystem();

    // Initialize extended vectors if the system is larger (e.g. pressure constraint)
    if (coarse_mat_->NumRows() > height)
    {
        coarse_b_ext_.SetSize(coarse_mat_->NumRows());
        coarse_x_ext_.SetSize(coarse_mat_->NumRows());
        coarse_b_ext_ = 0.0;
        coarse_x_ext_ = 0.0;
    }

    umf_solver_ = std::make_unique<mfem::UMFPackSolver>();
    umf_solver_->Control[UMFPACK_ORDERING] = UMFPACK_ORDERING_METIS;
    umf_solver_->SetOperator(*coarse_mat_);
#else
    mfem::out << "Warning: StokesMG: MFEM not built with SuiteSparse. "
              << "Defaulting to smoothing for coarse solve." << std::endl;
#endif
}

void StokesMG::addRefinedLevel()
{
    const Level& coarse_lvl = *levels_.back();

    auto fine_mesh = std::make_shared<mfem::Mesh>(coarse_lvl.op->getMesh());
    fine_mesh->UniformRefinement();

    auto fine_op = std::make_shared<StokesNitscheOperator>(
        fine_mesh, theta_, penalty_, factor_, ml_
    );
    fine_op->setDECMode();
    auto fine_smoother = std::make_shared<StokesNitscheDGS>(fine_op, st_);

    std::unique_ptr<const mfem::Operator> P;
    std::unique_ptr<const mfem::Operator> R;
    buildTransfers(*coarse_lvl.op, *fine_op, P, R);

    height = fine_op->NumRows();
    width = fine_op->NumCols();

    levels_.push_back(std::make_unique<Level>(std::move(fine_op),
                                              std::move(fine_smoother),
                                              std::move(P),
                                              std::move(R)));
}

void StokesMG::buildTransfers(const StokesNitscheOperator& coarse,
                              const StokesNitscheOperator& fine,
                              std::unique_ptr<const mfem::Operator>& P,
                              std::unique_ptr<const mfem::Operator>& R) const
{
    auto P_block = std::make_unique<mfem::BlockOperator>(
        fine.getOffsets(), coarse.getOffsets()
    );
    auto R_block = std::make_unique<mfem::BlockOperator>(
        coarse.getOffsets(), fine.getOffsets()
    );

    auto CreateL2Dual = [](const mfem::SparseMatrix& P_mat,
                           const mfem::Vector& M_f,
                           const mfem::Vector& M_c) -> mfem::SparseMatrix*
    {
        mfem::SparseMatrix* R_mat = mfem::Transpose(P_mat);
        mfem::Vector Mc_inv(M_c);
        Mc_inv.Reciprocal();
        R_mat->ScaleRows(Mc_inv);
        R_mat->ScaleColumns(M_f);
        return R_mat;
    };

    // Velocity Transfer (HCurl)
    {
        mfem::OperatorPtr P_u(mfem::Operator::MFEM_SPARSEMAT);
        fine.getHCurlSpace().GetTransferOperator(coarse.getHCurlSpace(), P_u);
        P_u.SetOperatorOwner(false);

        auto* P_u_mat = dynamic_cast<mfem::SparseMatrix*>(P_u.Ptr());
        mfem::SparseMatrix* R_u_mat = CreateL2Dual(*P_u_mat,
                                                   fine.getMassHCurlLumped(),
                                                   coarse.getMassHCurlLumped());
        P_block->SetBlock(0, 0, P_u_mat);
        R_block->SetBlock(0, 0, R_u_mat);
    }

    // Pressure Transfer (H1)
    {
        mfem::OperatorPtr P_p(mfem::Operator::MFEM_SPARSEMAT);
        fine.getH1Space().GetTransferOperator(coarse.getH1Space(), P_p);
        P_p.SetOperatorOwner(false);

        auto* P_p_mat = dynamic_cast<mfem::SparseMatrix*>(P_p.Ptr());
        mfem::SparseMatrix* R_p_mat = CreateL2Dual(*P_p_mat,
                                                   fine.getMassH1Lumped(),
                                                   coarse.getMassH1Lumped());
        P_block->SetBlock(1, 1, P_p_mat);
        R_block->SetBlock(1, 1, R_p_mat);
    }

    P_block->owns_blocks = 1;
    R_block->owns_blocks = 1;
    P = std::move(P_block);
    R = std::move(R_block);
}

void StokesMG::cycle(const int level_idx,
                     const mfem::Vector& b,
                     mfem::Vector& x) const
{
    const Level& lvl = *levels_[level_idx];

    // Coarsest Grid Solve
    if (level_idx == 0)
    {
        if (coarse_solver_)
        {
            coarse_solver_->Mult(b, x);
        }
#ifdef MFEM_USE_SUITESPARSE
        else if (umf_solver_)
        {
            // If the matrix is extended (constraint added), we map b -> b_ext
            if (coarse_b_ext_.Size() > 0)
            {
                // 1. Pad input: copy b (small) into b_ext (large)
                coarse_b_ext_ = 0.0;
                coarse_b_ext_.SetVector(b, 0);

                umf_solver_->Mult(coarse_b_ext_, coarse_x_ext_);

                // 2. Extract solution: copy only the first 'x.Size()' elements
                // Create a view of the extended vector limited to the original size
                mfem::Vector x_view(coarse_x_ext_.GetData(), x.Size());
                x = x_view;
            }
            else
            {
                // Normal solve (sizes match)
                umf_solver_->Mult(b, x);
            }
        }
#endif
        else
        {
            // Fallback to smoothing
            for (int i = 0; i < pre_smooth_ + post_smooth_; ++i)
                lvl.smoother->Mult(b, x);
        }
        return;
    }

    // 1. Pre-smoothing
    for (int i = 0; i < pre_smooth_; ++i)
        lvl.smoother->Mult(b, x);

    // 2. Compute Residual: r = b - Ax
    lvl.res = b;
    lvl.op->AddMult(x, lvl.res, -1.0);

    // 3. Restriction: b_coarse = R * r
    Level& coarse_lvl = *levels_[level_idx - 1];
    lvl.R->Mult(lvl.res, coarse_lvl.b);

    // 4. Coarse Grid Correction
    coarse_lvl.x = 0.0;

    cycle(level_idx - 1, coarse_lvl.b, coarse_lvl.x);

    if (cycle_type_ == MGCycleType::WCycle)
        cycle(level_idx - 1, coarse_lvl.b, coarse_lvl.x);

    // 5. Prolongation: x += P * x_coarse
    lvl.P->Mult(coarse_lvl.x, lvl.res);
    x += lvl.res;

    // 6. Post-smoothing
    for (int i = 0; i < post_smooth_; ++i)
        lvl.smoother->Mult(b, x);
}

void StokesMG::Mult(const mfem::Vector& b, mfem::Vector& x) const
{
    if (levels_.empty())
        MFEM_ABORT("StokesMG: No levels defined.");

    if (!iterative_mode)
        x = 0.0;

    cycle(static_cast<int>(levels_.size()) - 1, b, x);
}

void StokesMG::SetOperator(const mfem::Operator&)
{
    MFEM_ABORT("StokesMG: Use addRefinedLevel to manage the hierarchy.");
}

void StokesMG::setCoarseSolver(std::shared_ptr<const mfem::Solver> solver)
{
    coarse_solver_ = std::move(solver);
}

void StokesMG::setSmoothIterations(const int pre, const int post)
{
    pre_smooth_ = pre;
    post_smooth_ = post;
}

void StokesMG::setCycleType(const MGCycleType type)
{
    cycle_type_ = type;
}

// --- Getters Implementation ---

const StokesNitscheOperator& StokesMG::getOperator(const int level) const
{
    MFEM_VERIFY(level >= 0 && level < levels_.size(),
                "StokesMG::getOperator: Level index out of bounds.");
    return *levels_[level]->op;
}

const StokesNitscheDGS& StokesMG::getSmoother(const int level) const
{
    MFEM_VERIFY(level >= 0 && level < levels_.size(),
                "StokesMG::getSmoother: Level index out of bounds.");
    return *levels_[level]->smoother;
}

const StokesNitscheOperator& StokesMG::getFinestOperator() const
{
    MFEM_VERIFY(!levels_.empty(),
                "StokesMG::getFinestOperator: No levels exist.");
    return *levels_.back()->op;
}

const StokesNitscheDGS& StokesMG::getFinestSmoother() const
{
    MFEM_VERIFY(!levels_.empty(),
                "StokesMG::getFinestSmoother: No levels exist.");
    return *levels_.back()->smoother;
}

} // namespace StokesNitsche
