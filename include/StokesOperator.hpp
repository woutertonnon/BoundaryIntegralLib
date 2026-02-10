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

    const MassLumping getMassLumping() const {return ml_;}
    const OperatorMode getOperatorMode() const {return opmode_;}

    void setGalerkinMode() {opmode_ = GALERKIN;}
    void setDECMode() {opmode_ = DEC;}

    const mfem::SparseMatrix& getD0() const {return d0;}
    const mfem::SparseMatrix& getD1() const {return d1;}
    mfem::SparseMatrix& getD0() {return d0;}
    mfem::SparseMatrix& getD1() {return d1;}
    const mfem::FiniteElementSpace& getH1() const {return *h1_;}
    const mfem::FiniteElementSpace& getHCurl() const {return *hcurl_;}
    const mfem::FiniteElementSpace& getHDivOrL2() const {return *hdiv_or_l2_;}
    const mfem::BilinearForm& getMassH1() const {return *mass_h1_;}
    const mfem::BilinearForm& getMassHCurl() const {return *mass_hcurl_;}
    const mfem::BilinearForm& getMassHDivOrL2() const {return *mass_hdiv_or_l2_;}
    const mfem::BilinearForm& getNitsche() const {return *nitsche_;}
    const mfem::Vector& getMassH1Lumped() const {return mass_h1_lumped_;}
    const mfem::Vector& getMassHCurlLumped() const {return mass_hcurl_lumped_;}
    const mfem::Vector& getMassHDivOrL2Lumped() const {return mass_hdiv_or_l2_lumped_;}
    const mfem::Mesh& getMesh() const {return mesh_;}

    std::unique_ptr<mfem::SparseMatrix> getFullGalerkinSystem();
    std::unique_ptr<mfem::SparseMatrix> getFullDECSystem();
    std::unique_ptr<mfem::SparseMatrix> getFullSystem();

    void eliminateConstants(mfem::Vector& x) const;

    void Mult(const mfem::Vector& x,
                    mfem::Vector& y) const override;
    void MultDEC(const mfem::Vector& x,
                       mfem::Vector& y) const;
    void MultGalerkin(const mfem::Vector& x,
                            mfem::Vector& y) const;
};

} // namespace StokesNitsche

#endif
