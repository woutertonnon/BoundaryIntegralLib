#ifndef STOKES_OPERATOR_H
#define STOKES_OPERATOR_H

#include "BoundaryOperators.hpp"
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

class StokesNitscheOperator: mfem::Operator
{
private:
    const MassLumping ml_ = DIAGONAL_OF_MASS;
    OperatorMode opmode_ = DEC;
    
    void initIncidence();
    void initMass();
    void initLumpedMass();

protected:
    mfem::SparseMatrix d0, d1, d2;
    mfem::BilinearForm mass_h1_, mass_hcurl_;
    mfem::Vector mass_h1_lumped_, mass_hcurl_lumped_;
    
public:
    StokesNitscheOperator(const mfem::Mesh& mesh,
                          const double& theta,
                          const double& penalty,
                          const double& factor,
                          const MassLumping ml = DIAGONAL_OF_MASS,
                          const bool use_ml = true);
    MassLumping getMassLumping() const;
    OperatorMode getOperatorMode() const;
    void setGalerkinMode();
    void setDECMode();
    void residual(const mfem::Vector& x,
                        mfem::Vector& r) const;
    const mfem::SparseMatrix& getD0() const;
    const mfem::SparseMatrix& getD1() const;
    const mfem::SparseMatrix& getD2() const;
    const mfem::BilinearForm& getMassH1() const;
    const mfem::BilinearForm& getMassHCurl() const;
    const mfem::Vector& getMassH1Lumped() const;
    const mfem::Vector& getMassHCurlLumped() const;
};

enum SmootherType
{
    GAUSS_SEIDEL,
    JACOBI
};

class StokesNitscheDGS: mfem::Solver
{
private:
    StokesNitscheOperator& op_;
    mfem::SparseMatrix A11, A12, A21, A22;
public:
    StokesNitscheDGS(StokesNitscheOperator& op,
                     const SmootherType type);
};


} // namespace StokesNitsche

#endif
