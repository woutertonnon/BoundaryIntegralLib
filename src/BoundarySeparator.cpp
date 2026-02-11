#include "BoundarySeparator.hpp"
#include <utility>

// Helpers

enum class FEType_
{
    H1, HCURL, HDIV
};

FEType_
extractType(const mfem::FiniteElementSpace& fes)
{
    const mfem::FiniteElementCollection& col = *fes.FEColl();

    const int order = col.GetOrder();
    MFEM_VERIFY(order == 1, "getBDPermutationMatrix: not lowest order");

    const char* name = col.Name();
    FEType_ type;

    if(!strncmp(name, "H1_", 3))
        type = FEType_::H1;
    else if(!strncmp(name, "ND_", 3))
        type = FEType_::HCURL;
    else if(!strncmp(name, "RT_", 3))
        type = FEType_::HDIV;
    else
        throw std::invalid_argument(
            "extractType: fes not of type H1, HCURL or HDIV"
        );

    return type;
}

mfem::SparseMatrix
createPermutationMatrixFromVector(const mfem::Array<int>& p)
{
    const unsigned int n = p.Size();
    mfem::SparseMatrix P(n);

    for(unsigned int r = 0; r < n; ++r)
        P.Set(r, p[r], 1);

    P.Finalize();
    return P;
}

mfem::Array<int>
createBDPermutation(const mfem::FiniteElementSpace& fes)
{
    const mfem::Mesh& mesh = *fes.GetMesh();
    const FEType_ type = extractType(fes);

    const int dim = mesh.Dimension();
    mfem::Array<int> bd_dof;

    // Need this because GetBoundaryTrueDofs
    // is not a const function (?)
    {
        mfem::Array<int> ess_bdr(
            mesh.bdr_attributes.Max()
        );
        ess_bdr = 1;
        fes.GetEssentialTrueDofs(ess_bdr, bd_dof, -1);
    }
    // Anyways ...

    const unsigned int n_bd_dof = bd_dof.Size();
    unsigned int n_dof;

    switch(type)
    {
        case FEType_::H1:
            n_dof = mesh.GetNV();
            break;
        case FEType_::HCURL:
            n_dof = mesh.GetNEdges();
            break;
        case FEType_::HDIV:
            n_dof =
                (dim == 2 ?
                    mesh.GetNEdges() : mesh.GetNFaces()
                );
            break;
        default:
            std::unreachable();
    }

    MFEM_VERIFY(n_dof >= n_bd_dof,
                "getBDPermutationMatrix: more boundary unknowns than total unknowns?");

    const unsigned int n_int_dof = n_dof - n_bd_dof;

    mfem::Array<int> bd_mask;
    bd_mask.SetSize(n_dof);
    for(unsigned int k = 0; k < n_dof; ++k)
        bd_mask[k] = 0;
    for(const int k: bd_dof)
        bd_mask[k] = 1;

    unsigned int int_idx = 0,
                 bd_idx = n_int_dof;

    mfem::Array<int> perm;
    perm.SetSize(n_dof);

    for(unsigned int k = 0; k < n_dof; ++k)
    {
        if(bd_mask[k])
        {
            perm[k] = bd_idx;
            ++bd_idx;
        }
        else
        {
            perm[k] = int_idx;
            ++int_idx;
        }
    }

    return perm;
}

mfem::SparseMatrix
getBDPermutationMatrix(const mfem::FiniteElementSpace& fes)
{
    const mfem::Array<int> perm = createBDPermutation(fes);
    return createPermutationMatrixFromVector(perm);
}
