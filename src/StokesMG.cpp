#include "StokesMG.hpp"

#define OWNED (true)

namespace StokesNitsche
{

StokesMG::StokesMG(mfem::Mesh& mesh,
                   const double& theta,
                   const double& penalty,
                   const double& factor,
                   const MassLumping ml
                   const SmootherType type):
    theta_(theta), penalty_(penalty), factor_(factor),
    ml_(ml), type_(type)
{
    StokesNitscheOperator* new_op = new
        StokesNitscheOperator(mesh, theta, penalty, factor, ml);
    StokesDGS* new_smoother = new
        StokesDGS(*new_op, type);

    AddLevel(new_wop, new_smoother, OWNED, OWNED);

    // Copy the mesh
    meshes_.emplace_back(mesh);
}

void StokesMG::AddUniformlyRefinedLevel()
{
    {
        mfem::Mesh new_mesh(meshes_.back());
        meshes.emplace_back(std::move(new_mesh));
    }

    mfem::Mesh& mesh = meshes_.back();

    StokesNitscheOperator* new_op = new
        StokesNitscheOperator(mesh, theta_, penalty_, factor_, ml_);
    StokesDGS* new_smoother = new
        StokesDGS(*new_op, type_);

    AddLevel(new_wop, new_smoother, OWNED, OWNED);
    generateLatestTransferOps();
}

} // namespace StokesNitsche
