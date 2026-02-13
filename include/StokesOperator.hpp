#ifndef STOKES_NITSCHE_OPERATOR_H
#define STOKES_NITSCHE_OPERATOR_H

#include "mfem.hpp"
#include <memory> // Required for std::unique_ptr

namespace StokesNitsche {

enum class MassLumping
{
    None,
    Diagonal,
    Barycentric
};

enum class OperatorMode
{
    Galerkin,
    DEC
};

class StokesNitscheOperator : public mfem::Operator
{
public:
    StokesNitscheOperator(std::shared_ptr<mfem::Mesh> mesh_ptr,
                          const double theta,
                          const double penalty,
                          const double factor,
                          const MassLumping ml = MassLumping::Diagonal);

    // Disable copying
    StokesNitscheOperator(const StokesNitscheOperator&) = delete;
    StokesNitscheOperator& operator=(const StokesNitscheOperator&) = delete;

    // -- Configuration --
    MassLumping getMassLumping() const { return ml_; }
    OperatorMode getOperatorMode() const { return opmode_; }

    void setGalerkinMode() { opmode_ = OperatorMode::Galerkin; }
    void setDECMode()      { opmode_ = OperatorMode::DEC; }

    // -- Accessors --
    // Return const references for inspection
    const mfem::SparseMatrix& getD0() const { return d0_; }
    const mfem::SparseMatrix& getD1() const { return d1_; }

    // Return non-const references only if modification is strictly necessary
    mfem::SparseMatrix& getD0() { return d0_; }
    mfem::SparseMatrix& getD1() { return d1_; }

    const mfem::FiniteElementSpace& getH1Space()       const { return *h1_space_; }
    const mfem::FiniteElementSpace& getHCurlSpace()    const { return *hcurl_space_; }
    const mfem::FiniteElementSpace& getHDivOrL2Space() const { return *hdiv_or_l2_space_; }

    const mfem::BilinearForm& getMassH1()       const { return *mass_h1_; }
    const mfem::BilinearForm& getMassHCurl()    const { return *mass_hcurl_; }
    const mfem::BilinearForm& getMassHDivOrL2() const { return *mass_hdiv_or_l2_; }
    const mfem::BilinearForm& getNitsche()      const { return *nitsche_; }

    const mfem::Vector& getMassH1Lumped()       const { return mass_h1_lumped_; }
    const mfem::Vector& getMassHCurlLumped()    const { return mass_hcurl_lumped_; }
    const mfem::Vector& getMassHDivOrL2Lumped() const { return mass_hdiv_or_l2_lumped_; }

    const mfem::Mesh& getMesh() const { return *mesh_; }

    const mfem::Array<int>& getOffsets() const { return offsets_; }

    // -- System Generation --
    std::unique_ptr<mfem::SparseMatrix> getFullGalerkinSystem() const;
    std::unique_ptr<mfem::SparseMatrix> getFullDECSystem() const;
    std::unique_ptr<mfem::SparseMatrix> getFullSystem() const;

    void eliminateConstants(mfem::Vector& x) const;

    // -- Operations --
    void Mult(const mfem::Vector& x, mfem::Vector& y) const override;
    void MultDEC(const mfem::Vector& x, mfem::Vector& y) const;
    void MultGalerkin(const mfem::Vector& x, mfem::Vector& y) const;

private:
    void initFESpaces();
    void initIncidence();
    void initMass();
    void initLumpedMass();
    void initNitsche(const double theta, const double penalty, const double factor);

    // Member variables
    std::shared_ptr<mfem::Mesh> mesh_;
    const MassLumping ml_;
    const mfem::Array<int> offsets_;
    OperatorMode opmode_ = OperatorMode::Galerkin;

    // Finite Element Collections & Spaces
    std::unique_ptr<mfem::FiniteElementCollection> h1_fec_;
    std::unique_ptr<mfem::FiniteElementCollection> hcurl_fec_;
    std::unique_ptr<mfem::FiniteElementCollection> hdiv_or_l2_fec_;

    std::unique_ptr<mfem::FiniteElementSpace> h1_space_;
    std::unique_ptr<mfem::FiniteElementSpace> hcurl_space_;
    std::unique_ptr<mfem::FiniteElementSpace> hdiv_or_l2_space_;

    // Discrete Derivatives
    mfem::SparseMatrix d0_;
    mfem::SparseMatrix d1_;

    // Forms
    std::unique_ptr<mfem::BilinearForm> mass_h1_;
    std::unique_ptr<mfem::BilinearForm> mass_hcurl_;
    std::unique_ptr<mfem::BilinearForm> mass_hdiv_or_l2_;
    std::unique_ptr<mfem::BilinearForm> nitsche_;

    // Lumped Masses
    mfem::Vector mass_h1_lumped_;
    mfem::Vector mass_hcurl_lumped_;
    mfem::Vector mass_hdiv_or_l2_lumped_;
};

} // namespace StokesNitsche

#endif
