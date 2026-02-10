#ifndef BD_SEP_H
#define BD_SEP_H

#include "mfem.hpp"

mfem::SparseMatrix
getBDPermutationMatrix(const mfem::Mesh& mesh,
                       const mfem::FiniteElementSpace& fes);

#endif
