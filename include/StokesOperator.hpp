#ifndef STOKES_OPERATOR_H
#define STOKES_OPERATOR_H

#include "mfem.hpp"

namespace StokesNitsche {

enum MassLumping
{
    NO_MASS_LUMPING,
    DIAGONAL_OF_MASS,
    BARYCENTRIC
};

enum OperatorMode
{
    GALERKIN,
    DEC
};

class StokesNitscheOperator: public mfem::Operator
{
private:
    const MassLumping ml_ = DIAGONAL_OF_MASS;
    // mesh_ is not const because mfem::FiniteElementSpace does not take const meshes (?)
    mfem::Mesh& mesh_;
    OperatorMode opmode_ = GALERKIN;
    std::unique_ptr<mfem::FiniteElementCollection> h1_fec_, hcurl_fec_, hdiv_or_l2_fec_;
    std::unique_ptr<mfem::FiniteElementSpace> h1_, hcurl_, hdiv_or_l2_;

    mfem::SparseMatrix d0, d1;
    std::unique_ptr<mfem::BilinearForm> mass_h1_, mass_hcurl_, mass_hdiv_or_l2_, nitsche_;
    mfem::Vector mass_h1_lumped_, mass_hcurl_lumped_, mass_hdiv_or_l2_lumped_;

    void initFESpaces();
    void initIncidence();
    void initMass();
    void initLumpedMass();
    void initNitsche(const double& theta,
                     const double& penalty,
                     const double& factor);

public:
    StokesNitscheOperator(mfem::Mesh& mesh,
                          const double& theta,
                          const double& penalty,
                          const double& factor,
                          const MassLumping ml = DIAGONAL_OF_MASS);

    const MassLumping getMassLumping() const;
    const OperatorMode getOperatorMode() const;

    void setGalerkinMode();
    void setDECMode();

    std::unique_ptr<mfem::SparseMatrix> getFullGalerkinSystem();
    std::unique_ptr<mfem::SparseMatrix> getFullDECSystem();
    std::unique_ptr<mfem::SparseMatrix> getFullSystem();

    const mfem::SparseMatrix& getD0() const;
    const mfem::SparseMatrix& getD1() const;
    mfem::SparseMatrix& getD0();
    mfem::SparseMatrix& getD1();
    const mfem::BilinearForm& getMassH1() const;
    const mfem::BilinearForm& getMassHCurl() const;
    const mfem::BilinearForm& getMassHDivOrL2() const;
    const mfem::BilinearForm& getNitsche() const;
    const mfem::Vector& getMassH1Lumped() const;
    const mfem::Vector& getMassHCurlLumped() const;
    const mfem::Vector& getMassHDivOrL2Lumped() const;
    const mfem::Mesh& getMesh() const;

    void Mult(const mfem::Vector& x,
                    mfem::Vector& y) const override;
    void MultDEC(const mfem::Vector& x,
                       mfem::Vector& y) const;
    void MultGalerkin(const mfem::Vector& x,
                            mfem::Vector& y) const;
};

} // namespace StokesNitsche

#endif
