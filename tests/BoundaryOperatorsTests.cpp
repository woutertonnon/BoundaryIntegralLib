#include <gtest/gtest.h>

#include "BoundaryOperators.hpp"
#include "mfem.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

TEST(ND_NitscheIntegratorTest, ThirdOrderExactIntegral)
{
   // Checks <n x curl(u), v> + theta * <u, n x curl(v)> with u=(z,-z*z,y) with theta = -2, v=(y*y,x,1);
   //  exact value is 3. Runs on two meshes, projects u and v into an ND space, assembles
   // ND_NitscheIntegrator(theta,0.), then verifies v^T (A u) == 3 (prints context on failure).

   double tol = 1e-12;
   int order = 3;
   double theta = -2.;

   std::vector<std::string> meshfiles{
      "../extern/mfem/data/ref-cube.mesh"
      // "../tests/mesh/LidDrivenCavity3D.msh"
   };

   auto u_func = [](const mfem::Vector &x, double, mfem::Vector &y) -> void
   {
      double X = x.Elem(0);
      double Y = x.Elem(1);
      double Z = x.Elem(2);
      y.Elem(0) = Z;
      y.Elem(1) = -Z*Z;
      y.Elem(2) = Y;
   };
   auto v_func = [](const mfem::Vector &x, double, mfem::Vector &y) -> void
   {
      double X = x.Elem(0);
      double Y = x.Elem(1);
      double Z = x.Elem(2);
      y.Elem(0) = Y*Y;
      y.Elem(1) = X;
      y.Elem(2) = 1.;
   };
   mfem::VectorFunctionCoefficient u_coef(3, u_func);
   mfem::VectorFunctionCoefficient v_coef(3, v_func);

   for (std::string meshfile : meshfiles)
   {
      const int n = 1;

      mfem::Mesh mesh(meshfile);
      int dim = mesh.Dimension();
      mfem::FiniteElementCollection *fec_ND = new mfem::ND_FECollection(order, dim);
      mfem::FiniteElementSpace ND(&mesh, fec_ND);
      mfem::GridFunction u(&ND), v(&ND);

      u.ProjectCoefficient(u_coef);
      v.ProjectCoefficient(v_coef);

      // u and v should be exact representatives
      ASSERT_NEAR(u.ComputeL2Error(u_coef),0.,tol);
      ASSERT_NEAR(v.ComputeL2Error(v_coef),0.,tol);

      mfem::BilinearForm blf_A(&ND);
      blf_A.AddBdrFaceIntegrator(new ND_NitscheIntegrator(theta, 0.));
      blf_A.Assemble();

      mfem::LinearForm lf_u(&ND);
      lf_u.AddBdrFaceIntegrator(new ND_NitscheLFIntegrator(theta, 0.,u_coef));
      lf_u.Assemble();

      mfem::Vector A_u(blf_A.Height());
      blf_A.Mult(u, A_u);
      ASSERT_FLOAT_EQ(3., v * A_u)
         << "BilinearForm not correctly computed for order=" << order << " mesh=" << meshfile << "\n";
      ASSERT_FLOAT_EQ(2., v * lf_u)
         << "LinearForm not correctly computed for order=" << order << " mesh=" << meshfile << "\n";
   }
}



TEST(ND_NitscheIntegratorTest, DefaultsHaveNoCoefficient)
{
   // Exact-value regression for <n x curl(u), v>_∂Ω with theta=0, Cw=0.
   // Uses u=(0,0,xy), v=(0,0,x+y) on a refined reference cube.
   // Expected result: -1.
   const int refinements = 3;
   const int order = 1;

   mfem::Mesh mesh("../extern/mfem/data/ref-cube.mesh", 1, 1);
   for (int l = 0; l < refinements; ++l) { mesh.UniformRefinement(); }
   const int dim = mesh.Dimension();

   auto u_func = [](const mfem::Vector &x, double, mfem::Vector &y)
   {
      y.SetSize(3);
      y = 0.0;
      y(2) = x(0) * x(1);
   };
   auto v_func = [](const mfem::Vector &x, double, mfem::Vector &y)
   {
      y.SetSize(3);
      y = 0.0;
      y(2) = x(0) + x(1);
   };

   mfem::VectorFunctionCoefficient u_coef(3, u_func);
   mfem::VectorFunctionCoefficient v_coef(3, v_func);

   auto fec = std::make_unique<mfem::ND_FECollection>(order, dim);
   mfem::FiniteElementSpace nd(&mesh, fec.get());

   mfem::GridFunction u(&nd), v(&nd);
   u.ProjectCoefficient(u_coef);
   v.ProjectCoefficient(v_coef);

   mfem::BilinearForm A(&nd);
   A.AddBdrFaceIntegrator(new ND_NitscheIntegrator(0.0, 0.0));
   A.Assemble();
   A.Finalize();

   mfem::Vector Au(nd.GetNDofs());
   A.Mult(u, Au);

   ASSERT_FLOAT_EQ(-1.0, v * Au);
}

TEST(ND_NitscheIntegratorTest, DefaultsHaveNoCoefficient2)
{
   // Exact-value regression for <n x curl(u), v>_∂Ω with theta=-1, Cw=0.
   // Uses u=(xyz,xxz,xyy), v=(xx+y,yy+z,zz+x) on the reference cube.
   // Expected result: -3/4 - 1/3.
   const int refinements = 0;
   const int order = 2;

   mfem::Mesh mesh("../extern/mfem/data/ref-cube.mesh", 1, 1);
   for (int l = 0; l < refinements; ++l) { mesh.UniformRefinement(); }
   const int dim = mesh.Dimension();

   auto u_func = [](const mfem::Vector &x, double, mfem::Vector &y)
   {
      const double X = x(0), Y = x(1), Z = x(2);
      y.SetSize(3);
      y(0) = X * Y * Z;
      y(1) = X * X * Z;
      y(2) = X * Y * Y;
   };
   auto v_func = [](const mfem::Vector &x, double, mfem::Vector &y)
   {
      const double X = x(0), Y = x(1), Z = x(2);
      y.SetSize(3);
      y(0) = X * X + Y;
      y(1) = Y * Y + Z;
      y(2) = Z * Z + X;
   };

   mfem::VectorFunctionCoefficient u_coef(3, u_func);
   mfem::VectorFunctionCoefficient v_coef(3, v_func);

   auto fec = std::make_unique<mfem::ND_FECollection>(order, dim);
   mfem::FiniteElementSpace nd(&mesh, fec.get());

   mfem::GridFunction u(&nd), v(&nd);
   u.ProjectCoefficient(u_coef);
   v.ProjectCoefficient(v_coef);

   mfem::BilinearForm A(&nd);
   A.AddBdrFaceIntegrator(new ND_NitscheIntegrator(-1.0, 0.0));
   A.Assemble();
   A.Finalize();

   mfem::Vector Au(nd.GetNDofs());
   A.Mult(u, Au);

   ASSERT_FLOAT_EQ(-3.0 / 4.0 - 1.0 / 3.0, v * Au);
}

TEST(ND_NitscheIntegratorTest, ApproximationTest)
{
   // Convergence test for the Nitsche boundary operator (theta=1, Cw=0).
   // Uses smooth non-polynomial u,v and compares v*(A*u) to a fixed reference value.
   // Expects error reduction ~ O(h^order) under refinement.
   double last_err = 0.0, prev_err = 0.0;

   for (int order = 1; order < 3; ++order)
   {
      for (int refinements = 0; refinements < 9 - 2 * order; ++refinements)
      {
         mfem::Mesh mesh("../extern/mfem/data/ref-cube.mesh", 1, 1);
         for (int l = 0; l < refinements; ++l) { mesh.UniformRefinement(); }
         const int dim = mesh.Dimension();

         auto u_func = [](const mfem::Vector &x, double, mfem::Vector &y)
         {
            const double X = x(0), Y = x(1), Z = x(2);
            y.SetSize(3);
            y(0) = std::exp(X - 2 * Y + Z)
                 + std::sin(2 * M_PI * X) * std::cos(M_PI * Z)
                 + X * Y * (1 - Z);
            y(1) = X * X * std::sin(M_PI * Y)
                 + std::cos(2 * M_PI * Z) * (Y - Z)
                 + std::exp(-X * Z);
            y(2) = std::sin(M_PI * X * Y)
                 + Z * Z * std::cos(2 * M_PI * Y)
                 + (X - Y) * std::exp(Z);
         };

         auto v_func = [](const mfem::Vector &x, double, mfem::Vector &y)
         {
            const double X = x(0), Y = x(1), Z = x(2);
            y.SetSize(3);
            y(0) = std::cos(M_PI * X) * std::exp(Y - Z)
                 + X * (1 - X) * Y
                 + std::sin(2 * M_PI * Z);
            y(1) = std::sin(2 * M_PI * X * Z)
                 + std::exp(-Y)
                 + std::pow(Y - 0.5, 3);
            y(2) = std::cos(2 * M_PI * Y * Z)
                 + std::exp(X * Y)
                 - Z * (1 - Z);
         };

         mfem::VectorFunctionCoefficient u_coef(3, u_func);
         mfem::VectorFunctionCoefficient v_coef(3, v_func);

         auto fec = std::make_unique<mfem::ND_FECollection>(order, dim);
         mfem::FiniteElementSpace nd(&mesh, fec.get());

         mfem::GridFunction u(&nd), v(&nd);
         u.ProjectCoefficient(u_coef);
         v.ProjectCoefficient(v_coef);

         mfem::BilinearForm A(&nd);
         A.AddBdrFaceIntegrator(new ND_NitscheIntegrator(1.0, 0.0));
         A.Assemble();
         A.Finalize();

         mfem::Vector Au(nd.GetNDofs());
         A.Mult(u, Au);

         prev_err = last_err;
         last_err = 4.4722583402915601 - (v * Au);

         std::cout << "refinement: " << refinements
                   << ", order: " << order
                   << ", error: " << last_err << '\n';
      }

      EXPECT_LT(last_err, (std::pow(0.5, order) + 0.01) * prev_err);
   }
}

TEST(ND_NitscheIntegratorTest, ApproximationTestAsymmetricPenalty)
{
   // Convergence test with asymmetric penalty (theta=-1, Cw=100).
   // "Exact" integral depends on h; compares against precomputed references per refinement.
   // Expects error reduction consistent with order.
   double last_err = 0.0, prev_err = 0.0;

   const std::vector<double> exact = {
      667.0180872213067, 1330.817580069015, 2658.416565764433,
      5313.614537155269, 10624.01047993694, 21244.80236550028
   };

   for (int order = 1; order < 3; ++order)
   {
      for (int refinements = 0; refinements < static_cast<int>(exact.size()); ++refinements)
      {
         mfem::Mesh mesh("../extern/mfem/data/ref-cube.mesh", 1, 1);
         for (int l = 0; l < refinements; ++l) { mesh.UniformRefinement(); }
         const int dim = mesh.Dimension();

         auto u_func = [](const mfem::Vector &x, double, mfem::Vector &y)
         {
            const double X = x(0), Y = x(1), Z = x(2);
            y.SetSize(3);
            y(0) = std::exp(X - 2 * Y + Z)
                 + std::sin(2 * M_PI * X) * std::cos(M_PI * Z)
                 + X * Y * (1 - Z);
            y(1) = X * X * std::sin(M_PI * Y)
                 + std::cos(2 * M_PI * Z) * (Y - Z)
                 + std::exp(-X * Z);
            y(2) = std::sin(M_PI * X * Y)
                 + Z * Z * std::cos(2 * M_PI * Y)
                 + (X - Y) * std::exp(Z);
         };

         auto v_func = [](const mfem::Vector &x, double, mfem::Vector &y)
         {
            const double X = x(0), Y = x(1), Z = x(2);
            y.SetSize(3);
            y(0) = std::cos(M_PI * X) * std::exp(Y - Z)
                 + X * (1 - X) * Y
                 + std::sin(2 * M_PI * Z);
            y(1) = std::sin(2 * M_PI * X * Z)
                 + std::exp(-Y)
                 + std::pow(Y - 0.5, 3);
            y(2) = std::cos(2 * M_PI * Y * Z)
                 + std::exp(X * Y)
                 - Z * (1 - Z);
         };

         mfem::VectorFunctionCoefficient u_coef(3, u_func);
         mfem::VectorFunctionCoefficient v_coef(3, v_func);

         auto fec = std::make_unique<mfem::ND_FECollection>(order, dim);
         mfem::FiniteElementSpace nd(&mesh, fec.get());

         mfem::GridFunction u(&nd), v(&nd);
         u.ProjectCoefficient(u_coef);
         v.ProjectCoefficient(v_coef);

         mfem::BilinearForm A(&nd);
         A.AddBdrFaceIntegrator(new ND_NitscheIntegrator(-1.0, 100.0));
         A.Assemble();
         A.Finalize();

         mfem::Vector Au(nd.GetNDofs());
         A.Mult(u, Au);

         prev_err = last_err;
         last_err = std::abs(exact[refinements] - (v * Au));

         std::cout << "refinement: " << refinements
                   << ", order: " << order
                   << ", error: " << last_err << '\n';
      }

      EXPECT_LT(last_err, (std::pow(0.5, order) + 0.03) * prev_err);
   }
}

TEST(ND_NitscheIntegratorTest, DefaultsHaveNoCoefficient3)
{
   // Sanity: constant u should make the boundary curl term vanish.
   // Checks v*(A*u) ≈ 0 for theta=0, Cw=0.
   // Uses polynomial v on the reference cube.
   const int refinements = 0;
   const int order = 2;

   mfem::Mesh mesh("../extern/mfem/data/ref-cube.mesh", 1, 1);
   for (int l = 0; l < refinements; ++l) { mesh.UniformRefinement(); }
   const int dim = mesh.Dimension();

   auto u_func = [](const mfem::Vector &, double, mfem::Vector &y)
   {
      y.SetSize(3);
      y = 1.0;
   };
   auto v_func = [](const mfem::Vector &x, double, mfem::Vector &y)
   {
      const double X = x(0), Y = x(1), Z = x(2);
      y.SetSize(3);
      y(0) = X * X + Y;
      y(1) = Y * Y + Z;
      y(2) = Z * Z + X;
   };

   mfem::VectorFunctionCoefficient u_coef(3, u_func);
   mfem::VectorFunctionCoefficient v_coef(3, v_func);

   auto fec = std::make_unique<mfem::ND_FECollection>(order, dim);
   mfem::FiniteElementSpace nd(&mesh, fec.get());

   mfem::GridFunction u(&nd), v(&nd);
   u.ProjectCoefficient(u_coef);
   v.ProjectCoefficient(v_coef);

   mfem::BilinearForm A(&nd);
   A.AddBdrFaceIntegrator(new ND_NitscheIntegrator(0.0, 0.0));
   A.Assemble();
   A.Finalize();

   mfem::Vector Au(nd.GetNDofs());
   A.Mult(u, Au);

   ASSERT_NEAR(0.0, v * Au, 1e-12);
}

TEST(ND_NitscheIntegratorTest, DefaultsHaveNoCoefficient4)
{
   // Sanity: constant u should yield A*u ≈ 0 entrywise.
   // Applies the operator with theta=0, Cw=0.
   // Verifies all entries of A*u are ~0.
   const int refinements = 0;
   const int order = 2;

   mfem::Mesh mesh("../extern/mfem/data/ref-cube.mesh", 1, 1);
   for (int l = 0; l < refinements; ++l) { mesh.UniformRefinement(); }
   const int dim = mesh.Dimension();

   auto u_func = [](const mfem::Vector &, double, mfem::Vector &y)
   {
      y.SetSize(3);
      y = 1.0;
   };

   mfem::VectorFunctionCoefficient u_coef(3, u_func);

   auto fec = std::make_unique<mfem::ND_FECollection>(order, dim);
   mfem::FiniteElementSpace nd(&mesh, fec.get());

   mfem::GridFunction u(&nd);
   u.ProjectCoefficient(u_coef);

   mfem::BilinearForm A(&nd);
   A.AddBdrFaceIntegrator(new ND_NitscheIntegrator(0.0, 0.0));
   A.Assemble();
   A.Finalize();

   mfem::Vector Au(nd.GetNDofs());
   A.Mult(u, Au);

   for (int i = 0; i < Au.Size(); ++i)
   {
      ASSERT_NEAR(0.0, Au(i), 1e-10);
   }
}

TEST(ND_NitscheIntegratorTest, ConsistencyTest)
{
   // Consistency: LF integrator equals applying bilinear form to the same u.
   // Sweeps theta ∈ {-1,0,1} and Cw ∈ {0,10,...,90}.
   // Checks f - A*u is near zero (loose tolerance).
   const int refinements = 0;
   const int order = 1;

   mfem::Mesh mesh("../extern/mfem/data/ref-cube.mesh", 1, 1);
   for (int l = 0; l < refinements; ++l) { mesh.UniformRefinement(); }
   const int dim = mesh.Dimension();

   auto u_func = [](const mfem::Vector &x, double, mfem::Vector &y)
   {
      const double X = x(0), Y = x(1);
      y.SetSize(3);
      y(0) = -Y;
      y(1) =  X;
      y(2) =  1.0;
   };

   mfem::VectorFunctionCoefficient u_coef(3, u_func);

   auto fec = std::make_unique<mfem::ND_FECollection>(order, dim);
   mfem::FiniteElementSpace nd(&mesh, fec.get());

   mfem::GridFunction u(&nd);
   u.ProjectCoefficient(u_coef);

   for (double theta : {-1.0, 0.0, 1.0})
   {
      for (double Cw = 0.0; Cw < 100.0; Cw += 10.0)
      {
         mfem::BilinearForm A(&nd);
         A.AddBdrFaceIntegrator(new ND_NitscheIntegrator(theta, Cw));
         A.Assemble();
         A.Finalize();

         mfem::Vector Au(A.Height());
         A.Mult(u, Au);

         mfem::LinearForm f(&nd);
         f.AddBdrFaceIntegrator(new ND_NitscheLFIntegrator(theta, Cw, u_coef));
         f.Assemble();

         mfem::Vector dif(f.Size());
         dif = f;
         dif -= Au;

         for (int i = 0; i < dif.Size(); ++i)
         {
            ASSERT_NEAR(0.0, dif(i), 10.0);
         }
      }
   }
}

TEST(ND_NitscheIntegratorTest, RotationVanishingTest)
{
   // Regression: rigid rotation u=(-y,x,0) yields vanishing curlcurl + Nitsche bdr action.
   // Builds Cartesian cube mesh, projects u, applies operator A, expects A*u ≈ 0.
   // Also checks projection error is ~0.
   const int refinements = 1;
   const int order = 1;

   mfem::Mesh mesh = mfem::Mesh::MakeCartesian3D(
      1, 1, 1,
      mfem::Element::HEXAHEDRON,
      1.0, 1.0, 1.0);

   for (int l = 0; l < refinements; ++l) { mesh.UniformRefinement(); }
   const int dim = mesh.Dimension();

   auto u_func = [](const mfem::Vector &x, double, mfem::Vector &y)
   {
      (void)x;
      y.SetSize(3);
      y(0) = -x(1);
      y(1) =  x(0);
      y(2) =  0.0;
   };

   mfem::VectorFunctionCoefficient u_coef(3, u_func);

   auto fec = std::make_unique<mfem::ND_FECollection>(order, dim);
   mfem::FiniteElementSpace nd(&mesh, fec.get());

   mfem::GridFunction u(&nd);
   u.ProjectCoefficient(u_coef);

   ASSERT_NEAR(u.ComputeL2Error(u_coef), 0.0, 1e-13);

   mfem::ConstantCoefficient one(1.0);
   mfem::BilinearForm A(&nd);
   A.AddDomainIntegrator(new mfem::CurlCurlIntegrator(one));
   A.AddBdrFaceIntegrator(new ND_NitscheIntegrator(0.0, 0.0));
   A.Assemble();
   A.Finalize();

   mfem::Vector Au(A.Height());
   A.Mult(u, Au);

   for (int i = 0; i < Au.Size(); ++i)
   {
      ASSERT_NEAR(0.0, Au(i), 1e-13);
   }
}
