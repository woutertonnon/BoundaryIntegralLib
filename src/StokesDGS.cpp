#include "StokesDGS.hpp"

namespace StokesNitsche
{

void StokesNitscheDGS::initTransformation()
{
    const mfem::Mesh& mesh = op_.getMesh();
    const int nv = mesh.GetNV(),
              ne = mesh.GetNEdges();

    T_ = std::make_unique<mfem::BlockOperator>(
        offsets_T_
    );

    grad_adj_ = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Transpose(op_.getD0())
    );

    mfem::Vector inv_mass_hcurl_lumped_ = op_.getMassHCurlLumped();
    mfem::Vector inv_mass_h1_lumped_ = op_.getMassH1Lumped();
    inv_mass_hcurl_lumped_.Reciprocal();
    inv_mass_h1_lumped_.Reciprocal();

    grad_adj_->ScaleRows(inv_mass_h1_lumped_);
    grad_adj_->ScaleColumns(op_.getMassHCurlLumped());

    T_->SetBlock(0, 0, &id_u_);
    T_->SetBlock(0, 1, &op_.getD0());
    T_->SetBlock(1, 0, grad_adj_.get());
}

void StokesNitscheDGS::initTransformedSystem()
{
    Lp_ = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Mult(*grad_adj_, op_.getD0())
    );

    mfem::Vector inv_mass_hcurl_lumped_ = op_.getMassHCurlLumped();
    inv_mass_hcurl_lumped_.Reciprocal();

    std::unique_ptr<mfem::SparseMatrix> curlcurl;

    bd_ = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Mult(op_.getNitsche().SpMat(), op_.getD0())
    );
    bd_->ScaleRows(inv_mass_hcurl_lumped_);

    std::unique_ptr<mfem::SparseMatrix> curlcurl_nitsche;
    {
        std::unique_ptr<mfem::SparseMatrix> product;
        {
            auto tmp = std::make_unique<mfem::SparseMatrix>(op_.getD1());
            tmp->ScaleRows(op_.getMassHDivOrL2Lumped());

            auto d1T = std::unique_ptr<mfem::SparseMatrix>(
                mfem::Transpose(op_.getD1())
            );
            product = std::unique_ptr<mfem::SparseMatrix>(
                mfem::Mult(*d1T, *tmp)
            );
        }

        curlcurl_nitsche = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Add(*product, op_.getNitsche().SpMat())
        );
        curlcurl_nitsche->ScaleRows(inv_mass_hcurl_lumped_);
    }

    auto graddiv = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Mult(op_.getD0(), *grad_adj_)
    );

    Lu_ = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Add(*graddiv, *curlcurl_nitsche)
    );
}

void StokesNitscheDGS::computeResidual(const mfem::Vector& x,
                                             mfem::Vector& y) const
{
    residual_ = x;
    op_.AddMult(y, residual_, -1.0);
}

void StokesNitscheDGS::computeCorrection() const
{
    const mfem::Mesh& mesh = op_.getMesh();
    const int nv = mesh.GetNV(),
              ne = mesh.GetNEdges();

    mfem::Vector r_u(residual_.GetData(), ne);
    mfem::Vector r_p(residual_.GetData() + ne, nv);

    mfem::Vector corr_u(corr_.GetData(), ne);
    mfem::Vector corr_p(corr_.GetData() + ne, nv);

    assert(corr_.CheckFinite() == 0);
    switch(st_)
    {
        case GAUSS_SEIDEL:
            Lu_->Gauss_Seidel_forw(r_u, corr_u);
            grad_adj_->AddMult(corr_u, r_p, -1.0);
            Lp_->Gauss_Seidel_forw(r_p, corr_p);
            break;
        case JACOBI:
            Lu_->DiagScale(r_u, corr_u);
            grad_adj_->AddMult(corr_u, r_p, -1.0);
            Lp_->DiagScale(r_p, corr_p);
            break;
        default:
            MFEM_ABORT("StokesNitscheDGS::computeCorrection: unknown smoother");
            break;
    }
    assert(corr_.CheckFinite() == 0);
}

void StokesNitscheDGS::distributeCorrection(mfem::Vector& y) const
{
    T_->AddMult(corr_, y);
}

StokesNitscheDGS::StokesNitscheDGS(StokesNitscheOperator& op,
                                   const SmootherType type):
    mfem::Solver(op.NumRows(), true), op_(op), st_(type),
    offsets_T_({
        0, op.getMesh().GetNEdges(), op.getMesh().GetNV() + op.getMesh().GetNEdges()
    }),
    id_u_(op.getMesh().GetNEdges()),
    corr_(op.NumRows())
{
    if(op.getOperatorMode() != DEC)
        MFEM_ABORT("StokesNitscheDGS::StokesNitscheDGS: op_.getOperatorMode() != DEC");
    if(op.getMassLumping() == NO_MASS_LUMPING)
        MFEM_ABORT("StokesNitscheDGS::StokesNitscheDGS: op_.getMassLumping() == NO_MASS_LUMPING");

    initTransformation();
    initTransformedSystem();
}

void StokesNitscheDGS::Mult(const mfem::Vector& x,
                                  mfem::Vector& y) const
{
    assert(op_.getOperatorMode() == DEC);
    computeResidual(x, y);
    computeCorrection();
    distributeCorrection(y);
}

void StokesNitscheDGS::SetOperator(const mfem::Operator&)
{
    MFEM_ABORT("StokesNitscheDGS::SetOperator undefined!");
}

}
