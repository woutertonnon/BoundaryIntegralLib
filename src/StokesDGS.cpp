#include "StokesDGS.hpp"

namespace StokesNitsche
{

void StokesNitscheDGS::initTransformation()
{
    T_ = std::make_unique<mfem::BlockOperator>(op_->getOffsets());

    grad_adj_ = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Transpose(op_->getD0())
    );

    mfem::Vector inv_mass_hcurl_lumped = op_->getMassHCurlLumped();
    mfem::Vector inv_mass_h1_lumped    = op_->getMassH1Lumped();

    inv_mass_hcurl_lumped.Reciprocal();
    inv_mass_h1_lumped.Reciprocal();

    grad_adj_->ScaleRows(inv_mass_h1_lumped);
    grad_adj_->ScaleColumns(op_->getMassHCurlLumped());

    T_->SetBlock(0, 0, &id_u_);
    T_->SetBlock(0, 1, &op_->getD0());
    T_->SetBlock(1, 0, grad_adj_.get());
}

void StokesNitscheDGS::initTransformedSystem()
{
    Lp_ = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Mult(*grad_adj_, op_->getD0())
    );

    mfem::Vector inv_mass_hcurl_lumped = op_->getMassHCurlLumped();
    inv_mass_hcurl_lumped.Reciprocal();

    bd_ = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Mult(op_->getNitsche().SpMat(), op_->getD0())
    );
    bd_->ScaleRows(inv_mass_hcurl_lumped);

    std::unique_ptr<mfem::SparseMatrix> curlcurl_nitsche;
    {
        std::unique_ptr<mfem::SparseMatrix> product;
        {
            auto tmp = std::make_unique<mfem::SparseMatrix>(op_->getD1());
            tmp->ScaleRows(op_->getMassHDivOrL2Lumped());

            auto d1T = std::unique_ptr<mfem::SparseMatrix>(
                mfem::Transpose(op_->getD1())
            );
            product = std::unique_ptr<mfem::SparseMatrix>(
                mfem::Mult(*d1T, *tmp)
            );
        }

        curlcurl_nitsche = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Add(*product, op_->getNitsche().SpMat())
        );
        curlcurl_nitsche->ScaleRows(inv_mass_hcurl_lumped);
    }

    auto graddiv = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Mult(op_->getD0(), *grad_adj_)
    );

    Lu_ = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Add(*graddiv, *curlcurl_nitsche)
    );
}

void StokesNitscheDGS::computeResidual(const mfem::Vector& x,
                                       const mfem::Vector& y) const
{
    op_->MultDEC(y, residual_);
    residual_ -= x;
}

void StokesNitscheDGS::computeCorrection(const SmootherType st) const
{
    const mfem::Mesh& mesh = op_->getMesh();
    const int nv = mesh.GetNV();
    const int ne = mesh.GetNEdges();

    mfem::Vector r_u(residual_, 0, ne);
    mfem::Vector r_p(residual_, ne, nv);

    mfem::Vector corr_u(corr_, 0, ne);
    mfem::Vector corr_p(corr_, ne, nv);

    corr_ = 0.0;

    switch (st)
    {
        case SmootherType::GaussSeidelForw:
            Lu_->Gauss_Seidel_forw(r_u, corr_u);
            grad_adj_->AddMult(corr_u, r_p, -1.0);
            Lp_->Gauss_Seidel_forw(r_p, corr_p);
            break;
        case SmootherType::GaussSeidelSym:
        {
            mfem::GSSmoother Lu_s(*Lu_);
            mfem::GSSmoother Lp_s(*Lp_);

            Lu_s.Mult(r_u, corr_u);
            grad_adj_->AddMult(corr_u, r_p, -1.0);
            Lp_s.Mult(r_p, corr_p);
            break;
        }
        case SmootherType::Jacobi: // NOTE: BAD
            Lu_->DiagScale(r_u, corr_u);
            grad_adj_->AddMult(corr_u, r_p, -1.0);
            Lp_->DiagScale(r_p, corr_p);
            break;
        // case SmootherType::GaussSeidelBack:
        //     Lp_->Gauss_Seidel_back(r_p, corr_p);
        //     bd_->AddMult(corr_p, r_u, -1.0);
        //     Lu_->Gauss_Seidel_back(r_u, corr_u);
        //     break;
        default:
            MFEM_ABORT("StokesNitscheDGS::computeCorrection: unknown smoother");
            break;
    }
}

void StokesNitscheDGS::distributeCorrection(mfem::Vector& y) const
{
    T_->AddMult(corr_, y, -1.0);
}

StokesNitscheDGS::StokesNitscheDGS(std::shared_ptr<StokesNitscheOperator> op,
                                   const SmootherType type)
    : mfem::Solver(op->NumRows(), true),
      op_(op),
      st_(type),
      id_u_(op->getMesh().GetNEdges()),
      residual_(op->NumRows()),
      corr_(op->NumRows())
{
    if (op->getMassLumping() == MassLumping::None)
        MFEM_ABORT("StokesNitscheDGS: MassLumping is None");

    initTransformation();
    initTransformedSystem();
}

double StokesNitscheDGS::computeResidualNorm(const mfem::Vector& x,
                                             const mfem::Vector& y) const
{
    computeResidual(x, y);
    return residual_.Norml2();
}

void StokesNitscheDGS::Mult(const mfem::Vector& x,
                                  mfem::Vector& y) const
{
    computeResidual(x, y);
    computeCorrection(st_);
    distributeCorrection(y);
}

void StokesNitscheDGS::SetOperator(const mfem::Operator&)
{
    MFEM_ABORT("StokesNitscheDGS::SetOperator undefined!");
}

} // namespace StokesNitsche
