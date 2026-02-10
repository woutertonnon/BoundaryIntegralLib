#include "BoundarySeparator.hpp"

// Helper function
mfem::SparseMatrix
createPermutationMatrixFromVector(
    const mfem::Array<int>& p)
{

}

mfem::SparseMatrix
getBDPermutationMatrix(const mfem::Mesh& mesh,
                       const mfem::FiniteElementSpace& fes)
{
#ifndef NDEBUG
    const mfem::FiniteElementCollection& col =
        fes.FEColl();
    // TODO: Verify that fes is lowest order
    // lagrangian, nedelec, or raviart thomas
    // L2 has no bd unknowns, abort if given
#endif

    const int dim = mesh.Dimension();
    mfem::Array<int> bd_dof;

    // Need this because GetBoundaryTrueDofs
    // is not a const function (?)
    {
        mfem::Array<int> ess_bdr(
            mesh->bdr_attributes.Max()
        );
        ess_bdr = 1;
        fes.GetEssentialTrueDofs(ess_bdr, bd_dof, -1);
    }
    // Anyways ...

    const unsigned int n_bd_dof = bd_dof.Size();
    unsigned int n_dof;

    switch(el_dim)
    {
        case 0:
            n_dof = mesh.GetNV();
            break;
        case 1:
            n_dof = mesh.GetNEdges();
            break;
        case 2:
            n_dof = dim == 2 ?
                mesh.GetNE() : mesh.GetNFaces();
            break;
        case 3:
            n_dof = mesh.GetNE();
            break;
        default:
            MFEM_ABORT("getBDPermutationMatrix: el_dim > mesh.Dimension()");
            break;
    }

    MFEM_VERIFY(n_dof > n_bd_dof,
                "getBDPermutationMatrix: more boundary unknowns than total unknowns?");

    const unsigned int n_int_dof = n_dof - n_bd_dof;

    mfem::Array<int> bd_mask;
    bd_mask.SetSize(n_dof);
    for(unsigned int k = 0; k < n_dof; ++k)
        bd_mask[k] = 0;
    for(const int k: bd_dof)
        bd_mask[k] = 1;

    unsigned int int_idx = 0,
                 bd_idx = ndof - 1;

    mfem::Array<int> perm;
    perm.SetSize(n_dof);

    for(unsigned int k = 0; k < ndof; ++k)
    {
        if(bd_mask[k])
        {
            perm[k] = bd_idx;
            --bd_idx;
        }
        else
        {
            perm[k] = int_idx;
            ++int_idx;
        }
    }

    return createPermutationMatrixFromVector(perm);
}

#endif
