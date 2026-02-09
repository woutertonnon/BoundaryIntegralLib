#ifndef INCIDENCE_H
#define INCIDENCE_H

#include "mfem.hpp"
// TODO: Figure out how we can use
//       mfem::Tables directly to add the entries
//       Using mfem::Table::GetI(), mfem::Table::GetJ()

mfem::SparseMatrix assembleVertexEdge(const mfem::Mesh& mesh);
mfem::SparseMatrix assembleFaceEdge(const mfem::Mesh& mesh, const int DIM);
mfem::SparseMatrix assembleElementFace(const mfem::Mesh& mesh);

#endif
