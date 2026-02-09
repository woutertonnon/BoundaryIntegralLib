#include "StokesOperator.hpp"
#include "incidence.hpp"
#include "BoundaryOperators.hpp"

namespace StokesNitsche
{

void StokesNitscheOperator::initFESpaces()
{
    const int dim = mesh_.Dimension();

    h1_fec_ = std::make_unique<mfem::H1_FECollection>(
        1, dim
    );
    hcurl_fec_ = std::make_unique<mfem::ND_FECollection>(
        1, dim
    );

    if(dim == 2)
        hdiv_or_l2_fec_ = std::make_unique<mfem::L2_FECollection>(
            0, dim, mfem::BasisType::GaussLegendre, mfem::FiniteElement::INTEGRAL
        );
    else // dim == 3
        hdiv_or_l2_fec_ = std::make_unique<mfem::RT_FECollection>(
            0, dim
        );

    h1_ = std::make_unique<mfem::FiniteElementSpace>(
        &mesh_, h1_fec_.get()
    );
    hcurl_ = std::make_unique<mfem::FiniteElementSpace>(
        &mesh_, hcurl_fec_.get()
    );
    hdiv_or_l2_ = std::make_unique<mfem::FiniteElementSpace>(
        &mesh_, hdiv_or_l2_fec_.get()
    );
}

void StokesNitscheOperator::initIncidence()
{
    const int dim = mesh_.Dimension();

    d0 = std::move(
        assembleVertexEdge(mesh_)
    );

    d1 = std::move(
        assembleFaceEdge(mesh_, dim)
    );
}

void StokesNitscheOperator::initMass()
{
    mass_h1_ = std::make_unique<mfem::BilinearForm>(
        h1_.get()
    );
    mass_hcurl_ = std::make_unique<mfem::BilinearForm>(
        hcurl_.get()
    );
    mass_hdiv_or_l2_ = std::make_unique<mfem::BilinearForm>(
        hdiv_or_l2_.get()
    );

    mfem::ConstantCoefficient one(1.0);
    mfem::ConstantCoefficient four(4.0);

    mass_h1_->AddDomainIntegrator(new mfem::MassIntegrator(one));
    mass_hcurl_->AddDomainIntegrator(new mfem::VectorFEMassIntegrator(one));
    if(mesh_.Dimension() == 2)
        mass_hdiv_or_l2_->AddDomainIntegrator(new mfem::MassIntegrator(four));
    else // 3
        mass_hdiv_or_l2_->AddDomainIntegrator(new mfem::VectorFEMassIntegrator(four));

    mass_h1_->Assemble(); mass_h1_->Finalize();
    mass_hcurl_->Assemble(); mass_hcurl_->Finalize();
    mass_hdiv_or_l2_->Assemble(); mass_hdiv_or_l2_->Finalize();
}

void StokesNitscheOperator::initLumpedMass()
{
    mass_h1_lumped_.SetSize(h1_->GetNDofs());
    mass_hcurl_lumped_.SetSize(hcurl_->GetNDofs());
    mass_hdiv_or_l2_lumped_.SetSize(hdiv_or_l2_->GetNDofs());

    switch(ml_)
    {
        case NO_MASS_LUMPING:
            break;
        case DIAGONAL_OF_MASS:
            mass_h1_->AssembleDiagonal(mass_h1_lumped_);
            mass_hcurl_->AssembleDiagonal(mass_hcurl_lumped_);
            mass_hdiv_or_l2_->AssembleDiagonal(mass_hdiv_or_l2_lumped_);
            break;
        case BARYCENTRIC:
            throw std::logic_error("BARYCENTRIC mass lumping not implemented (yet)");
            break;
        default:
            throw std::logic_error("Unknown mass lumping in StokesNitscheOperator");
            break;
    }
}

void StokesNitscheOperator::initNitsche(const double& theta,
                                        const double& penalty,
                                        const double& factor)
{
    nitsche_ = std::make_unique<mfem::BilinearForm>(
        hcurl_.get()
    );

    nitsche_->AddBdrFaceIntegrator(new WouterIntegrator(theta, penalty, factor));
    nitsche_->Assemble(); nitsche_->Finalize();
}

StokesNitscheOperator::StokesNitscheOperator(mfem::Mesh& mesh,
                                             const double& theta,
                                             const double& penalty,
                                             const double& factor,
                                             const MassLumping ml):
    mfem::Operator(mesh.GetNV() + mesh.GetNEdges()), mesh_(mesh), ml_(ml)
{
    initFESpaces();
    initIncidence();
    initMass();
    initLumpedMass();
    initNitsche(theta, penalty, factor);
}

const MassLumping StokesNitscheOperator::getMassLumping() const
{
    return ml_;
}

const OperatorMode StokesNitscheOperator::getOperatorMode() const
{
    return opmode_;
}

void StokesNitscheOperator::setGalerkinMode()
{
    opmode_ = GALERKIN;
}

void StokesNitscheOperator::setDECMode()
{
    opmode_ = DEC;
}

std::unique_ptr<mfem::SparseMatrix>
StokesNitscheOperator::getFullGalerkinSystem()
{
    mfem::ConstantCoefficient one(1.0);
    const int nv = mesh_.GetNV();
    const int ne = mesh_.GetNEdges();

    const mfem::Array<int> offsets({0, ne, ne + nv, ne + nv + 1});
    mfem::BlockMatrix block(offsets);

    std::unique_ptr<mfem::SparseMatrix> curlcurl, grad, gradT;

    {
        std::unique_ptr<mfem::MixedBilinearForm> G =
            std::make_unique<mfem::MixedBilinearForm>(
                h1_.get(), hcurl_.get()
            );
        G->AddDomainIntegrator(new mfem::MixedVectorGradientIntegrator(one));
        G->Assemble(); G->Finalize();

        grad = std::unique_ptr<mfem::SparseMatrix>(
            G->LoseMat()
        );
    }

    gradT = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Transpose(*grad)
    );

    {
        // auto tmp = std::unique_ptr<mfem::SparseMatrix>(
        //     mfem::Mult(mass_hdiv_or_l2_->SpMat(), d1)
        // );

        // auto d1T = std::unique_ptr<mfem::SparseMatrix>(
        //     mfem::Transpose(d1)
        // );
        // auto product = std::unique_ptr<mfem::SparseMatrix>(
        //     mfem::Mult(*d1T, *tmp)
        // );

        // curlcurl = std::unique_ptr<mfem::SparseMatrix>(
        //     mfem::Add(*product, nitsche_->SpMat())
        // );
        // For some reason, gives something different (???)

        std::unique_ptr<mfem::BilinearForm> cc =
            std::make_unique<mfem::BilinearForm>(
                hcurl_.get()
            );

        // auto* curl_integ = new mfem::CurlCurlIntegrator(one);
        // const int ir_order = 10;
        // const mfem::IntegrationRule &ir = mfem::IntRules.Get(mfem::Geometry::TETRAHEDRON, ir_order);
        // curl_integ->SetIntRule(&ir);
        // cc->AddDomainIntegrator(curl_integ);

        cc->AddDomainIntegrator(new mfem::CurlCurlIntegrator(one));

        cc->Assemble(); cc->Finalize();

        curlcurl = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Add(cc->SpMat(), nitsche_->SpMat())
        );
    }

    mfem::Array<int> cols(nv);
    for(unsigned int k = 0; k < nv; ++k)
        cols[k] = k;

    mfem::SparseMatrix mean(1, nv);

    mfem::Vector ones(nv),
                 mass_x_ones(nv);
    ones = 1.;
    mass_h1_->Mult(ones, mass_x_ones);

    mean.AddRow(0, cols, mass_x_ones);
    mean.Finalize();

    auto meanT = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Transpose(mean)
    );

    block.SetBlock(0, 0, curlcurl.get());
    block.SetBlock(0, 1, grad.get());
    block.SetBlock(1, 0, gradT.get());
    block.SetBlock(2, 1, &mean);
    block.SetBlock(1, 2, meanT.get());

    return std::unique_ptr<mfem::SparseMatrix>(block.CreateMonolithic());
}

std::unique_ptr<mfem::SparseMatrix>
StokesNitscheOperator::getFullDECSystem()
{
    const int nv = mesh_.GetNV();
    const int ne = mesh_.GetNEdges();

    const mfem::Array<int> offsets({0, ne, ne + nv, ne + nv + 1});
    mfem::BlockMatrix block(offsets);

    std::unique_ptr<mfem::SparseMatrix> curlcurl, grad, gradT;

    grad = std::make_unique<mfem::SparseMatrix>(d0);

    mfem::Vector inv_mass_h1_lumped_(mass_h1_lumped_);
    inv_mass_h1_lumped_.Reciprocal();

    gradT = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Transpose(*grad)
    );
    gradT->ScaleRows(inv_mass_h1_lumped_);
    gradT->ScaleColumns(mass_hcurl_lumped_);

    mfem::Vector inv_mass_hcurl_lumped_(mass_hcurl_lumped_);
    inv_mass_hcurl_lumped_.Reciprocal();

    {
        auto tmp = std::make_unique<mfem::SparseMatrix>(d1);
        tmp->ScaleRows(mass_hdiv_or_l2_lumped_);

        auto d1T = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Transpose(d1)
        );
        auto product = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Mult(*d1T, *tmp)
        );

        curlcurl = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Add(*product, nitsche_->SpMat())
        );

        curlcurl->ScaleRows(inv_mass_hcurl_lumped_);
    }

    mfem::Array<int> cols(nv);
    for(unsigned int k = 0; k < nv; ++k)
        cols[k] = k;

    mfem::SparseMatrix mean(1, nv);
    mean.AddRow(0, cols, mass_h1_lumped_);
    mean.Finalize();

    auto meanT = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Transpose(mean)
    );

    block.SetBlock(0, 0, curlcurl.get());
    block.SetBlock(0, 1, grad.get());
    block.SetBlock(1, 0, gradT.get());
    block.SetBlock(2, 1, &mean);
    block.SetBlock(1, 2, meanT.get());

    return std::unique_ptr<mfem::SparseMatrix>(block.CreateMonolithic());
}

std::unique_ptr<mfem::SparseMatrix>
StokesNitscheOperator::getFullSystem()
{
    if(opmode_ == GALERKIN)
        return getFullGalerkinSystem();
    else // DEC
        return getFullDECSystem();
}

const mfem::SparseMatrix& StokesNitscheOperator::getD0() const
{
    return d0;
}

const mfem::SparseMatrix& StokesNitscheOperator::getD1() const
{
    return d1;
}

mfem::SparseMatrix& StokesNitscheOperator::getD0()
{
    return d0;
}

mfem::SparseMatrix& StokesNitscheOperator::getD1()
{
    return d1;
}

const mfem::BilinearForm& StokesNitscheOperator::getMassH1() const
{
    return *mass_h1_;
}

const mfem::BilinearForm& StokesNitscheOperator::getMassHCurl() const
{
    return *mass_hcurl_;
}

const mfem::BilinearForm& StokesNitscheOperator::getMassHDivOrL2() const
{
    return *mass_hdiv_or_l2_;
}

const mfem::BilinearForm& StokesNitscheOperator::getNitsche() const
{
    return *nitsche_;
}

const mfem::Vector& StokesNitscheOperator::getMassH1Lumped() const
{
    return mass_h1_lumped_;
}

const mfem::Vector& StokesNitscheOperator::getMassHCurlLumped() const
{
    return mass_hcurl_lumped_;
}

const mfem::Vector& StokesNitscheOperator::getMassHDivOrL2Lumped() const
{
    return mass_hdiv_or_l2_lumped_;
}

const mfem::Mesh& StokesNitscheOperator::getMesh() const
{
    return mesh_;
}

void StokesNitscheOperator::eliminateConstants(mfem::Vector& x) const
{
    assert(x.Size() == this->NumCols());

    const int nv = mesh_.GetNV(),
              ne = mesh_.GetNEdges();

    mfem::Vector ones(nv);
    ones = 1.0;

    mfem::Vector x_p(x, ne, nv);

    double proj = 0;

    if(opmode_ == GALERKIN)
        proj = mass_h1_->InnerProduct(ones, x_p) /
               mass_h1_->InnerProduct(ones, ones);
    else
    {
        MFEM_ASSERT(
            ml_ != NO_MASS_LUMPING, 
            "StokesNitscheOperator::eliminateConstants: Need mass lumping in opmode DEC"
        );

        mfem::Vector tmp(x_p);
        tmp *= mass_h1_lumped_;

        proj = (tmp * ones) / (mass_h1_lumped_ * ones);
    }

    ones *= proj;
    x_p -= ones;
}

void StokesNitscheOperator::Mult(const mfem::Vector& x,
                                       mfem::Vector& y) const
{
    if(opmode_ == GALERKIN)
        MultGalerkin(x, y);
    else // DEC
        MultDEC(x, y);
}

void StokesNitscheOperator::MultDEC(const mfem::Vector& x,
                                          mfem::Vector& y) const
{
    const int nv = mesh_.GetNV();
    const int ne = mesh_.GetNEdges();

    assert(
        x.Size() == nv + ne &&
        y.Size() == nv + ne
    );

    const mfem::Vector x_u(x.GetData(), ne);
    const mfem::Vector x_p(x.GetData() + ne, nv);

    mfem::Vector y_u(y.GetData(), ne);
    mfem::Vector y_p(y.GetData() + ne, nv);

    mfem::Vector tmp_u(y_u.Size()),
                 tmp_du(hdiv_or_l2_->GetNDofs());

    d1.Mult(x_u, tmp_du);
    tmp_du *= mass_hdiv_or_l2_lumped_;
    d1.MultTranspose(tmp_du, y_u);
    nitsche_->AddMult(x_u, y_u);
    y_u /= mass_hcurl_lumped_;

    d0.AddMult(x_p, y_u);

    tmp_u = x_u;
    tmp_u *= mass_hcurl_lumped_;
    d0.MultTranspose(tmp_u, y_p);
    y_p /= mass_h1_lumped_;
}

void StokesNitscheOperator::MultGalerkin(const mfem::Vector& x,
                                               mfem::Vector& y) const
{
    const int nv = mesh_.GetNV();
    const int ne = mesh_.GetNEdges();

    assert(
        x.Size() == nv + ne &&
        y.Size() == nv + ne
    );

    const mfem::Vector x_u(x.GetData(), ne);
    const mfem::Vector x_p(x.GetData() + ne, nv);

    mfem::Vector y_u(y, 0, ne);
    mfem::Vector y_p(y, ne, nv);

    mfem::Vector tmp_u(y_u.Size()),
                 tmp_du(hdiv_or_l2_->GetNDofs()),
                 tmp_mdu(hdiv_or_l2_->GetNDofs());

    d1.Mult(x_u, tmp_du);
    mass_hdiv_or_l2_->Mult(tmp_du, tmp_mdu);
    d1.MultTranspose(tmp_mdu, y_u);

    d0.Mult(x_p, tmp_u);
    mass_hcurl_->AddMult(tmp_u, y_u);

    mass_hcurl_->Mult(x_u, tmp_u);
    d0.MultTranspose(tmp_u, y_p);

    nitsche_->AddMult(x_u, y_u);
}

}
