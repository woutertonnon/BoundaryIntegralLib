#include "incidence.hpp"

mfem::SparseMatrix assembleVertexEdge(const mfem::Mesh& mesh)
{
  // Number of edges and vertices
  const int nv = mesh.GetNV(),
            ne = mesh.GetNEdges();
  mfem::SparseMatrix D(ne, nv);

  mfem::Array<int> vert(2), cols(2);
  mfem::Vector srow(2);
  for(int ei = 0; ei < ne; ++ei)
  {
    // Get vertices of edge `ei`
    mesh.GetEdgeVertices(ei, vert);
    const int sign = (vert[0] < vert[1] ? 1 : -1);
    // Add to matrix
    //cols[0] = vert[0];
    srow[0] = -sign;

    //cols[1] = vert[1];
    srow[1] = sign;

    D.AddRow(ei, vert, srow);
  }

  D.Finalize();
  return D;
}

mfem::SparseMatrix assembleFaceEdge(const mfem::Mesh& mesh,
                                    const int DIM)
{
  int ne = mesh.GetNEdges(),
      nf = -1;

  assert(DIM == 2 || DIM == 3);

  if(DIM == 3)
    nf = mesh.GetNFaces();
  else
    nf = mesh.GetNE();

  mfem::SparseMatrix D(nf, ne);

  mfem::Array<int> edges, ori;
  mfem::Vector srow;
  for(int fi = 0; fi < nf; ++fi)
  {
    const mfem::Geometry::Type face_type = (
      DIM == 3 ? mesh.GetFaceGeometry(fi) : mesh.GetElementGeometry(fi)
    );
    const int num_edges = mfem::Geometry::NumEdges[face_type];
    // Re-size bc mfem does
    // not do it
    srow.SetSize(num_edges);
    // Get edges and orientations of element fi
    if(DIM == 3)
      mesh.GetFaceEdges(fi, edges, ori);
    else
      mesh.GetElementEdges(fi, edges, ori);

    for(unsigned int k = 0; k < num_edges; ++k)
      srow(k) = static_cast<double>(ori[k]);
    // Add to matrix
    D.AddRow(fi, edges, srow);
  }

  D.Finalize();
  return D;
}

mfem::SparseMatrix assembleElementFace(const mfem::Mesh& mesh)
{
    const int nel = mesh.GetNE(),
              nf = mesh.GetNFaces();

    mfem::SparseMatrix D(nel, nf);

    mfem::Array<int> faces, face_ori;
    mfem::Vector srow;

    for(int eli = 0; eli < nel; ++eli)
    {
        mesh.GetElementFaces(eli, faces, face_ori);
        srow.SetSize(faces.Size());

        for(int k = 0; k < faces.Size(); ++k)
            srow(k) = (face_ori[k] % 2 == 0 ? 1.0 : -1.0);

        D.AddRow(eli, faces, srow);
    }

    D.Finalize();
    return D;
}

mfem::SparseMatrix assembleElementFace_(const mfem::Mesh& mesh)
{
  const int nel = mesh.GetNE(),
            nf = mesh.GetNFaces();
  mfem::SparseMatrix D(nel, nf);

  mfem::Array<int> faces(4), face_ori(4),
  edges(3), edge_ori(3);
  mfem::Vector srow(4);

  for(int eli = 0; eli < nel; ++eli)
  {
    MFEM_ASSERT(
      mesh.GetElementType(eli) == mfem::Element::TETRAHEDRON,
      "assembleElementFace: Only tetrahedra are supported, to be fixed."
    );
    // Get edges and orientations of faces
    mesh.GetElementFaces(eli, faces, face_ori);
    // Add to matrix
    for(unsigned int k = 0; k < 4; ++k)
      srow[k] = (face_ori[k] % 2 == 0 ? 1 : -1);

    D.AddRow(eli, faces, srow);
  }

  D.Finalize();
  return D;
}
