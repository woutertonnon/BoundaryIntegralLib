#include "StokesOperator.hpp"
#include "incidence.hpp"
#include "BoundaryOperators.hpp"
#include <iostream>

namespace StokesNitsche
{

void StokesNitscheOperator::initFESpaces()
{
    MFEM_VERIFY(order_ > 0,
                "StokesNitscheOperator: order == 0, use order > 0");
    const int dim = mesh_->Dimension();

    // 1. Create Collections
    h1_fec_ = std::make_unique<mfem::H1_FECollection>(order_, dim);
    hcurl_fec_ = std::make_unique<mfem::ND_FECollection>(order_, dim);

    if (dim == 2)
        hdiv_or_l2_fec_ = std::make_unique<mfem::L2_FECollection>(
            0, dim, mfem::BasisType::GaussLegendre,
            mfem::FiniteElement::INTEGRAL
        );
    else
        hdiv_or_l2_fec_ = std::make_unique<mfem::RT_FECollection>(
            order_ - 1, dim
        );

    // 2. Create Spaces
    h1_space_ = std::make_unique<mfem::FiniteElementSpace>(
        mesh_.get(), h1_fec_.get()
    );
    hcurl_space_ = std::make_unique<mfem::FiniteElementSpace>(
        mesh_.get(), hcurl_fec_.get()
    );
    hdiv_or_l2_space_ = std::make_unique<mfem::FiniteElementSpace>(
        mesh_.get(), hdiv_or_l2_fec_.get()
    );

    // Need to do this, cannot (easily) be done in the constructor
    this->height = h1_space_->GetNDofs() + hcurl_space_->GetNDofs();
    this->width = this->height;
}

void StokesNitscheOperator::initIncidence()
{
    d0_ = assembleDiscreteGradient(h1_space_.get(), hcurl_space_.get());
    d1_ = assembleDiscreteCurl(hcurl_space_.get(), hdiv_or_l2_space_.get());
}

void StokesNitscheOperator::initMass()
{
    mass_h1_ = std::make_unique<mfem::BilinearForm>(h1_space_.get());
    mass_hcurl_ = std::make_unique<mfem::BilinearForm>(hcurl_space_.get());
    mass_hdiv_or_l2_ = std::make_unique<mfem::BilinearForm>(
        hdiv_or_l2_space_.get()
    );

    mfem::ConstantCoefficient one(1.0);

    mass_h1_->AddDomainIntegrator(new mfem::MassIntegrator(one));
    mass_hcurl_->AddDomainIntegrator(
        new mfem::VectorFEMassIntegrator(one)
    );

    if (mesh_->Dimension() == 2)
        mass_hdiv_or_l2_->AddDomainIntegrator(
            new mfem::MassIntegrator(one)
        );
    else
        mass_hdiv_or_l2_->AddDomainIntegrator(
            new mfem::VectorFEMassIntegrator(one)
        );

    mass_h1_->Assemble();
    mass_h1_->Finalize();

    mass_hcurl_->Assemble();
    mass_hcurl_->Finalize();

    mass_hdiv_or_l2_->Assemble();
    mass_hdiv_or_l2_->Finalize();
}

void StokesNitscheOperator::initLumpedMass()
{
    mass_h1_lumped_.SetSize(h1_space_->GetNDofs());
    mass_hcurl_lumped_.SetSize(hcurl_space_->GetNDofs());
    mass_hdiv_or_l2_lumped_.SetSize(hdiv_or_l2_space_->GetNDofs());

    switch (ml_)
    {
        case MassLumping::None:
            break;
        case MassLumping::Diagonal:
            mass_h1_->AssembleDiagonal(mass_h1_lumped_);
            mass_hcurl_->AssembleDiagonal(mass_hcurl_lumped_);
            mass_hdiv_or_l2_->AssembleDiagonal(mass_hdiv_or_l2_lumped_);
            break;
        case MassLumping::Barycentric:
            throw std::logic_error(
                "BARYCENTRIC mass lumping not implemented (yet)"
            );
        default:
            std::unreachable();
    }
}

void StokesNitscheOperator::initNitsche(const double theta,
                                        const double penalty,
                                        const double factor)
{
    nitsche_ = std::make_unique<mfem::BilinearForm>(hcurl_space_.get());

    nitsche_->AddBdrFaceIntegrator(
        new ND_NitscheIntegrator(theta, penalty, factor)
    );

    nitsche_->Assemble();
    nitsche_->Finalize();
}

StokesNitscheOperator::StokesNitscheOperator(
    std::shared_ptr<mfem::Mesh> mesh_ptr,
    const unsigned order,
    const double theta,
    const double penalty,
    const double factor,
    const MassLumping ml)
    : mfem::Operator(),
      order_(order),
      mesh_(mesh_ptr),
      ml_(ml)
{
    initFESpaces();
    initIncidence();
    initMass();
    initLumpedMass();
    initNitsche(theta, penalty, factor);

    offsets_.SetSize(3);
    offsets_[0] = 0;
    offsets_[1] = hcurl_space_->GetNDofs();
    offsets_[2] = hcurl_space_->GetNDofs() +
                  h1_space_->GetNDofs();
    // offsets_[3] = hcurl_space_->GetNDofs() +
                  // h1_space_->GetNDofs() + 1;
}

// -------------------------------------------------------------------------
// System Generation
// -------------------------------------------------------------------------

std::unique_ptr<mfem::SparseMatrix>
StokesNitscheOperator::getFullGalerkinSystem() const
{
    mfem::ConstantCoefficient one(1.0);
    const int nv = h1_space_->GetNDofs();
    const int ne = hcurl_space_->GetNDofs();

    const mfem::Array<int> offsets({0, ne, ne + nv, ne + nv + 1});
    mfem::BlockMatrix block(offsets);

    std::unique_ptr<mfem::SparseMatrix> curlcurl, grad, gradT;

    // Gradient Block (G)
    {
        auto G = std::make_unique<mfem::MixedBilinearForm>(
            h1_space_.get(), hcurl_space_.get()
        );
        G->AddDomainIntegrator(
            new mfem::MixedVectorGradientIntegrator(one)
        );
        G->Assemble();
        G->Finalize();
        grad = std::unique_ptr<mfem::SparseMatrix>(G->LoseMat());
    }

    // -Divergence Block (G^T)
    gradT = std::unique_ptr<mfem::SparseMatrix>(mfem::Transpose(*grad));

    // CurlCurl Block + Nitsche
    {
        auto cc = std::make_unique<mfem::BilinearForm>(hcurl_space_.get());
        cc->AddDomainIntegrator(new mfem::CurlCurlIntegrator(one));
        cc->Assemble();
        cc->Finalize();

        curlcurl = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Add(cc->SpMat(), nitsche_->SpMat())
        );
    }

    // Mean Constraint Block
    mfem::Array<int> cols(nv);
    for (int k = 0; k < nv; ++k)
        cols[k] = k;

    mfem::SparseMatrix mean(1, nv);

    mfem::GridFunction ones(h1_space_.get());
    ones.ProjectCoefficient(one);

    mfem::Vector mass_x_ones(nv);

    mass_h1_->Mult(ones, mass_x_ones);

    mean.AddRow(0, cols, mass_x_ones);
    mean.Finalize();

    auto meanT = std::unique_ptr<mfem::SparseMatrix>(mfem::Transpose(mean));

    // 5. Assemble Block Matrix
    block.SetBlock(0, 0, curlcurl.get());
    block.SetBlock(0, 1, grad.get());
    block.SetBlock(1, 0, gradT.get());
    block.SetBlock(2, 1, &mean);
    block.SetBlock(1, 2, meanT.get());

    return std::unique_ptr<mfem::SparseMatrix>(block.CreateMonolithic());
}

std::unique_ptr<mfem::SparseMatrix>
StokesNitscheOperator::getFullDECSystem() const
{
    const int nv = h1_space_->GetNDofs();
    const int ne = hcurl_space_->GetNDofs();

    const mfem::Array<int> offsets({0, ne, ne + nv, ne + nv + 1});
    mfem::BlockMatrix block(offsets);

    // Gradient (d0)
    auto grad = std::make_unique<mfem::SparseMatrix>(d0_);

    // Divergence (scaled d0^T)
    mfem::Vector inv_mass_h1(mass_h1_lumped_);
    inv_mass_h1.Reciprocal();

    auto gradT = std::unique_ptr<mfem::SparseMatrix>(mfem::Transpose(*grad));
    gradT->ScaleRows(inv_mass_h1);
    gradT->ScaleColumns(mass_hcurl_lumped_);

    // CurlCurl
    std::unique_ptr<mfem::SparseMatrix> curlcurl;
    {
        auto tmp = std::make_unique<mfem::SparseMatrix>(d1_);
        tmp->ScaleRows(mass_hdiv_or_l2_lumped_);

        auto d1T = std::unique_ptr<mfem::SparseMatrix>(mfem::Transpose(d1_));

        auto product = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Mult(*d1T, *tmp)
        );

        curlcurl = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Add(*product, nitsche_->SpMat())
        );

        mfem::Vector inv_mass_hcurl(mass_hcurl_lumped_);
        inv_mass_hcurl.Reciprocal();
        curlcurl->ScaleRows(inv_mass_hcurl);
    }

    // Mean Constraint
    mfem::Array<int> cols(nv);
    for (int k = 0; k < nv; ++k)
        cols[k] = k;

    mfem::SparseMatrix mean(1, nv);
    mean.AddRow(0, cols, mass_h1_lumped_);
    mean.Finalize();

    auto meanT = std::unique_ptr<mfem::SparseMatrix>(mfem::Transpose(mean));

    // Assemble
    block.SetBlock(0, 0, curlcurl.get());
    block.SetBlock(0, 1, grad.get());
    block.SetBlock(1, 0, gradT.get());
    block.SetBlock(2, 1, &mean);
    block.SetBlock(1, 2, meanT.get());

    return std::unique_ptr<mfem::SparseMatrix>(block.CreateMonolithic());
}

std::unique_ptr<mfem::SparseMatrix>
StokesNitscheOperator::getFullSystem() const
{
    if (opmode_ == OperatorMode::Galerkin)
        return getFullGalerkinSystem();
    else
        return getFullDECSystem();
}

void StokesNitscheOperator::eliminateConstants(mfem::Vector& x) const
{
    MFEM_ASSERT(x.Size() == this->NumCols(), "Vector size mismatch");

    const int nv = h1_space_->GetNDofs();
    const int ne = hcurl_space_->GetNDofs();

    std::cerr << "eliminateConstants needs to be fixed" << std::endl;
    std::abort();
    mfem::Vector ones(nv);
    ones = 1.0;

    mfem::Vector x_p(x.GetData() + ne, nv);

    double proj = 0.0;

    if (opmode_ == OperatorMode::Galerkin)
    {
        const double denom = mass_h1_->InnerProduct(ones, ones);
        proj = mass_h1_->InnerProduct(ones, x_p) / denom;
    }
    else // DEC
    {
        MFEM_ASSERT(ml_ != MassLumping::None,
            "EliminateConstants: Need mass lumping in DEC mode");

        mfem::Vector tmp(x_p);
        tmp *= mass_h1_lumped_;

        const double denom = mass_h1_lumped_ * ones;
        proj = (tmp * ones) / denom;
    }

    ones *= proj;
    x_p -= ones;
}

// -------------------------------------------------------------------------
// Operations
// -------------------------------------------------------------------------

void StokesNitscheOperator::Mult(const mfem::Vector& x, mfem::Vector& y) const
{
    if (opmode_ == OperatorMode::Galerkin)
        MultGalerkin(x, y);
    else
        MultDEC(x, y);
}

void StokesNitscheOperator::MultDEC(const mfem::Vector& x, mfem::Vector& y) const
{
    const int nv = h1_space_->GetNDofs();
    const int ne = hcurl_space_->GetNDofs();

    MFEM_ASSERT(x.Size() == nv + ne && y.Size() == nv + ne,
        "Vector size mismatch");

    const mfem::Vector x_u(x.GetData(), ne);
    const mfem::Vector x_p(x.GetData() + ne, nv);

    mfem::Vector y_u(y.GetData(), ne);
    mfem::Vector y_p(y.GetData() + ne, nv);

    mfem::Vector tmp_u(y_u.Size());
    mfem::Vector tmp_du(hdiv_or_l2_space_->GetNDofs());

    // Curl-Curl part
    d1_.Mult(x_u, tmp_du);
    tmp_du *= mass_hdiv_or_l2_lumped_;
    d1_.MultTranspose(tmp_du, y_u);

    nitsche_->AddMult(x_u, y_u);

    y_u /= mass_hcurl_lumped_;

    // Gradient part
    d0_.AddMult(x_p, y_u);

    // Divergence part
    tmp_u = x_u;
    tmp_u *= mass_hcurl_lumped_;
    d0_.MultTranspose(tmp_u, y_p);

    y_p /= mass_h1_lumped_;
}

void StokesNitscheOperator::MultGalerkin(const mfem::Vector& x,
                                               mfem::Vector& y) const
{
    const int nv = h1_space_->GetNDofs();
    const int ne = hcurl_space_->GetNDofs();

    MFEM_ASSERT(x.Size() == nv + ne && y.Size() == nv + ne,
        "Vector size mismatch");

    const mfem::Vector x_u(x.GetData(), ne);
    const mfem::Vector x_p(x.GetData() + ne, nv);

    mfem::Vector y_u(y.GetData(), ne);
    mfem::Vector y_p(y.GetData() + ne, nv);

    mfem::Vector tmp_u(y_u.Size());
    mfem::Vector tmp_du(hdiv_or_l2_space_->GetNDofs());
    mfem::Vector tmp_mdu(hdiv_or_l2_space_->GetNDofs());

    // Curl-Curl Term
    d1_.Mult(x_u, tmp_du);
    mass_hdiv_or_l2_->Mult(tmp_du, tmp_mdu);
    d1_.MultTranspose(tmp_mdu, y_u);

    // Gradient Term
    d0_.Mult(x_p, tmp_u);
    mass_hcurl_->AddMult(tmp_u, y_u);

    // Divergence Term
    mass_hcurl_->Mult(x_u, tmp_u);
    d0_.MultTranspose(tmp_u, y_p);

    // Nitsche Boundary Term
    nitsche_->AddMult(x_u, y_u);
}

} // namespace StokesNitsche
