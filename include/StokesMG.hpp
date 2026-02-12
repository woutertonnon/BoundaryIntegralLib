#ifndef STOKESMG_HPP
#define STOKESMG_HPP

#include "mfem.hpp"
#include "StokesOperator.hpp"
#include "StokesDGS.hpp"

namespace StokesNitsche
{

class StokesMG: public mfem::Multigrid
{
protected:
    std::vector<mfem::Mesh> meshes_;
    const double theta_, penalty_, factor_;
    const MassLumping ml_;
    const SmootherType type_;

    void generateLatestTransferOps();
public:
    StokesMG(mfem::Mesh& mesh,
             const double& theta,
             const double& penalty,
             const double& factor = 1,
             const MassLumping ml = DIAGONAL_OF_MASS,
             const SmootherType type = GAUSS_SEIDEL_FORW);
     void AddUniformlyRefinedLevel();
};

} // namespace StokesNitsche
#endif
