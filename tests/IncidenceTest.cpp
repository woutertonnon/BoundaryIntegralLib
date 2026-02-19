#include <gtest/gtest.h>
#include "mfem.hpp"
#include "incidence.hpp"

TEST(IncidenceTest, ComplexPropertyTria)
{
    const unsigned int n = 5,
                       DIM = 2;

    const mfem::Element::Type el_type = 
        mfem::Element::TRIANGLE;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian2D(
        n, n + 1, el_type
    );

    const mfem::SparseMatrix
        d0 = assembleVertexEdge(mesh),
        d1 = assembleFaceEdge(mesh, DIM);
        
    const std::unique_ptr<const mfem::SparseMatrix>
        d1d0 = std::unique_ptr<const mfem::SparseMatrix>(
            mfem::Mult(d1, d0)
        );
        
    ASSERT_FLOAT_EQ(d1d0->MaxNorm(), 0.0);
    ASSERT_GT(d0.MaxNorm(), 0.0);
    ASSERT_GT(d1.MaxNorm(), 0.0);
}

TEST(IncidenceTest, ComplexPropertyQuad)
{
    const unsigned int n = 5,
                       DIM = 2;

    const mfem::Element::Type el_type = 
        mfem::Element::QUADRILATERAL;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian2D(
        n, n + 1, el_type
    );

    const mfem::SparseMatrix
        d0 = assembleVertexEdge(mesh),
        d1 = assembleFaceEdge(mesh, DIM);
        
    const std::unique_ptr<const mfem::SparseMatrix>
        d1d0 = std::unique_ptr<const mfem::SparseMatrix>(
            mfem::Mult(d1, d0)
        );
        
    ASSERT_FLOAT_EQ(d1d0->MaxNorm(), 0.0);
    ASSERT_GT(d0.MaxNorm(), 0.0);
    ASSERT_GT(d1.MaxNorm(), 0.0);
}

TEST(IncidenceTest, ComplexPropertyTets)
{
    const unsigned int n = 5,
                       DIM = 3;

    const mfem::Element::Type el_type = 
        mfem::Element::TETRAHEDRON;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n + 1, n + 2, el_type
    );

    const mfem::SparseMatrix
        d0 = assembleVertexEdge(mesh),
        d1 = assembleFaceEdge(mesh, DIM),
        d2 = assembleElementFace(mesh);
        
    const std::unique_ptr<const mfem::SparseMatrix>
        d1d0 = std::unique_ptr<const mfem::SparseMatrix>(
            mfem::Mult(d1, d0)
        ),
        d2d1 = std::unique_ptr<const mfem::SparseMatrix>(
            mfem::Mult(d2, d1)
        );
        
    ASSERT_FLOAT_EQ(d1d0->MaxNorm(), 0.0);
    ASSERT_FLOAT_EQ(d2d1->MaxNorm(), 0.0);
    ASSERT_GT(d0.MaxNorm(), 0.0);
    ASSERT_GT(d1.MaxNorm(), 0.0);
    ASSERT_GT(d2.MaxNorm(), 0.0);
}

TEST(IncidenceTest, ComplexPropertyHex)
{
    const unsigned int n = 5,
                       DIM = 3;

    const mfem::Element::Type el_type = 
        mfem::Element::HEXAHEDRON;

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n + 1, n + 2, el_type
    );

    const mfem::SparseMatrix
        d0 = assembleVertexEdge(mesh),
        d1 = assembleFaceEdge(mesh, DIM),
        d2 = assembleElementFace(mesh);
        
    const std::unique_ptr<const mfem::SparseMatrix>
        d1d0 = std::unique_ptr<const mfem::SparseMatrix>(
            mfem::Mult(d1, d0)
        ),
        d2d1 = std::unique_ptr<const mfem::SparseMatrix>(
            mfem::Mult(d2, d1)
        );
        
    ASSERT_FLOAT_EQ(d1d0->MaxNorm(), 0.0);
    ASSERT_FLOAT_EQ(d2d1->MaxNorm(), 0.0);
    ASSERT_GT(d0.MaxNorm(), 0.0);
    ASSERT_GT(d1.MaxNorm(), 0.0);
    ASSERT_GT(d2.MaxNorm(), 0.0);
}

TEST(IncidenceTest, CurlCurl2D)
{
    const unsigned int n = 1,
                       DIM = 2;

    const mfem::Element::Type el_type =
        mfem::Element::TRIANGLE;

    mfem::ConstantCoefficient one(1.0);
    mfem::ConstantCoefficient four(4.0);

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian2D(
        n, n, el_type, std::sqrt(2), std::sqrt(3)
    );

    const int nv = mesh.GetNV(),
              ne = mesh.GetNEdges(),
              nf = mesh.GetNE();

    const mfem::SparseMatrix
        d0 = assembleVertexEdge(mesh),
        d1 = assembleFaceEdge(mesh, DIM);

    ASSERT_EQ(d0.NumCols(), nv);
    ASSERT_EQ(d0.NumRows(), ne);
    ASSERT_EQ(d1.NumRows(), nf);
    ASSERT_EQ(d1.NumCols(), ne);

    auto hcurl_fec = std::make_unique<mfem::ND_FECollection>(
        1, DIM
    );
    auto hcurl = std::make_unique<mfem::FiniteElementSpace>(
        &mesh, hcurl_fec.get()
    );

    auto l2_fec = std::make_unique<mfem::L2_FECollection>(
        0, DIM,
        mfem::BasisType::GaussLegendre,
        mfem::FiniteElement::INTEGRAL
    );
    auto l2 = std::make_unique<mfem::FiniteElementSpace>(
        &mesh, l2_fec.get()
    );

    auto mass_ = std::make_unique<mfem::BilinearForm>(
        l2.get()
    );
    mass_->AddDomainIntegrator(new mfem::MassIntegrator(four));
    mass_->Assemble(); mass_->Finalize();
    const mfem::SparseMatrix& mass = mass_->SpMat();

    ASSERT_EQ(mass.NumRows(), nf);

    mfem::Vector diag(mass.NumRows());
    mass_->AssembleDiagonal(diag);
    ASSERT_NEAR(diag(0), 1 / mesh.GetElementVolume(0), 1e-12);

    auto curlcurl_ = std::make_unique<mfem::BilinearForm>(
        hcurl.get()
    );
    curlcurl_->AddDomainIntegrator(new mfem::CurlCurlIntegrator(one));
    curlcurl_->Assemble(); curlcurl_->Finalize();
    const mfem::SparseMatrix& curlcurl = curlcurl_->SpMat();

    ASSERT_EQ(curlcurl.NumRows(), ne);

    std::unique_ptr<mfem::SparseMatrix> curlcurl_incidence_;
    {
        auto tmp = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Mult(mass, d1)
        );
        auto d1T = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Transpose(d1)
        );
        curlcurl_incidence_ = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Mult(*d1T, *tmp)
        );
    }
    const mfem::SparseMatrix& curlcurl_incidence = *curlcurl_incidence_;

    auto diff = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Add( 1.0, curlcurl,
                  -1.0, curlcurl_incidence)
    );

    const mfem::DenseMatrix* curlcurl_dense = curlcurl.ToDenseMatrix();
    const mfem::DenseMatrix* curlcurl_incidence_dense = curlcurl_incidence.ToDenseMatrix();
/*
    curlcurl_dense->Print(std::cout, 6);
    std::cout << std::endl;
    curlcurl_incidence_dense->Print(std::cout, 6);*/

    delete curlcurl_dense; delete curlcurl_incidence_dense;

    ASSERT_GT(curlcurl_incidence.MaxNorm(), 1e-12);
    ASSERT_GT(curlcurl.MaxNorm(), 1e-12);
    ASSERT_LT(diff->MaxNorm() / curlcurl.MaxNorm(), 1e-10);
}


TEST(IncidenceTest, CurlCurl3D)
{
    const unsigned int n = 5,
                       DIM = 3;

    const mfem::Element::Type el_type =
        mfem::Element::TETRAHEDRON;

    mfem::ConstantCoefficient one(1.0);
    mfem::ConstantCoefficient four(4.0);

    mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
        n, n + 1, n + 2, el_type, std::sqrt(2), std::sqrt(3), std::sqrt(5)
    );

    const int nv = mesh.GetNV(),
              ne = mesh.GetNEdges(),
              nf = mesh.GetNFaces();

    const mfem::SparseMatrix
        d0 = assembleVertexEdge(mesh),
        d1 = assembleFaceEdge(mesh, DIM);

    ASSERT_EQ(d0.NumCols(), nv);
    ASSERT_EQ(d0.NumRows(), ne);
    ASSERT_EQ(d1.NumRows(), nf);
    ASSERT_EQ(d1.NumCols(), ne);

    auto hcurl_fec = std::make_unique<mfem::ND_FECollection>(
        1, DIM
    );
    auto hcurl = std::make_unique<mfem::FiniteElementSpace>(
        &mesh, hcurl_fec.get()
    );

    auto hdiv_fec = std::make_unique<mfem::RT_FECollection>(
        0, DIM
    );
    auto hdiv = std::make_unique<mfem::FiniteElementSpace>(
        &mesh, hdiv_fec.get()
    );

    auto mass_ = std::make_unique<mfem::BilinearForm>(
        hdiv.get()
    );
    mass_->AddDomainIntegrator(
        new mfem::VectorFEMassIntegrator(four)
    );
    mass_->Assemble(); mass_->Finalize();
    const mfem::SparseMatrix& mass = mass_->SpMat();

    ASSERT_EQ(mass.NumRows(), nf);

    auto curlcurl_ = std::make_unique<mfem::BilinearForm>(
        hcurl.get()
    );
    curlcurl_->AddDomainIntegrator(new mfem::CurlCurlIntegrator(one));
    curlcurl_->Assemble(); curlcurl_->Finalize();
    const mfem::SparseMatrix& curlcurl = curlcurl_->SpMat();

    ASSERT_EQ(curlcurl.NumRows(), ne);

    std::unique_ptr<mfem::SparseMatrix> curlcurl_incidence_;
    {
        auto tmp = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Mult(mass, d1)
        );
        auto d1T = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Transpose(d1)
        );
        curlcurl_incidence_ = std::unique_ptr<mfem::SparseMatrix>(
            mfem::Mult(*d1T, *tmp)
        );
    }
    const mfem::SparseMatrix& curlcurl_incidence = *curlcurl_incidence_;

    auto diff = std::unique_ptr<mfem::SparseMatrix>(
        mfem::Add( 1.0, curlcurl,
                  -1.0, curlcurl_incidence)
    );

    ASSERT_GT(curlcurl_incidence.MaxNorm(), 1e-12);
    ASSERT_GT(curlcurl.MaxNorm(), 1e-12);
    ASSERT_LT(diff->MaxNorm() / curlcurl.MaxNorm(), 1e-10);
}
