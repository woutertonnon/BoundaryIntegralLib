#ifndef BD_SEP_H
#define BD_SEP_H

#include "mfem.hpp"

mfem::SparseMatrix
createPermutationMatrixFromVector(const mfem::Array<int>& p);

mfem::Array<int>
createBDPermutation(const mfem::FiniteElementSpace& fes);

mfem::SparseMatrix
getBDPermutationMatrix(const mfem::FiniteElementSpace& fes);

#endif
