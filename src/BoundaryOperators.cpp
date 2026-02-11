#include "BoundaryOperators.h"
#include "mfem.hpp"

void WouterIntegrator::AssembleElementMatrix(const mfem::FiniteElement &el, mfem::ElementTransformation &Trans,
                                             mfem::DenseMatrix &elmat)
{
   return;
}

void WouterIntegrator::AssembleFaceMatrix(
    const mfem::FiniteElement &el1, const mfem::FiniteElement &el2,
    mfem::FaceElementTransformations &Trans, mfem::DenseMatrix &elmat)
{
   MFEM_ASSERT(Trans.Elem2No < 0,
               "support for interior faces is not implemented");
   std::cout << "el2 = " << Trans.Elem2 << std::endl;
   assert(Trans.Elem2==nullptr);
   if(Trans.Elem2!=nullptr)std::abort();
   //std::abort();
   int dim = el1.GetDim();
   mfem::Vector normal(dim);

   mfem::IntegrationPoint ip_face;

   // Build a reasonable quadrature on the actual face geometry
   const mfem::IntegrationRule *ir = &mfem::IntRules.Get(static_cast<mfem::Geometry::Type>(Trans.FaceGeom),
                            10);
   std::cout << "GetNPoints = " << ir->GetNPoints() << std::endl;

   elmat.SetSize(el1.GetDof(), el1.GetDof());
   elmat = 0.;
   auto weights = ir->GetWeights();
   std::cout << "weights.Sum() = " << weights.Sum() << std::endl;
   //std::abort();
   for (int i = 0; i < ir->GetNPoints(); ++i)
   {
      ip_face = ir->IntPoint(i);
      //std::cout << "ip_face: " << ip_face.x << ", " << ip_face.y << ", " << ip_face.z << std::endl;
      //if (ip_face.x*ip_face.x+ip_face.y*ip_face.y > 1.) std::cout << "ip_face not on triangle!\n";

      // Sync face + element integration points. This ensures ip on the element
      // matches the face point orientation (important for tangential fields).
      Trans.SetAllIntPoints(&ip_face);

      // Face normal atu this quadrature point
      //Trans.Face->SetIntPoint(&ip_face);
      mfem::CalcOrtho(Trans.Face->Jacobian(), normal);
      //std::cout << "normal: " << std::endl;
      //normal.Print(std::cout);
      double area = normal.Norml2();
      //std::cout << "area = " << area << std::endl;
      //std::cout << "area2 = " << Trans.Face->Weight() << std::endl;
      double h = sqrt(normal.Norml2());
      //std::abort();

      //std::cout << "h = " << h << std::endl;
      mfem::DenseMatrix vshape(el1.GetDof(), Trans.GetSpaceDim()), shape(el1.GetDof(), Trans.GetSpaceDim());
      mfem::DenseMatrix curl_shape(el1.GetDof(), 3);

      mfem::ElementTransformation *tr1 = Trans.Elem1 ;

      Trans.SetIntPoint(&ip_face);
      //auto ip_test = Trans.GetElement1IntPoint();
      //std::cout << "ip_test1: " << ip_test.x << ", " << ip_test.y << ", " << ip_test.z << std::endl;
      
      //tr1->SetIntPoint(&ip_face);
      //ip_test = Trans.GetElement1IntPoint();
      //std::cout << "ip_test2: " << ip_test.x << ", " << ip_test.y << ", " << ip_test.z << std::endl;
      
      //std::cout << "ip_face: " << ip_face.x << ", " << ip_face.y << ", " << ip_face.z << std::endl;
      vshape = 0.;
      shape = 0.;
      el1.CalcPhysVShape(*tr1, vshape);
      mfem::Mult(vshape, tr1->InverseJacobian(), shape);
     // shape *= sqrt(.5);
      //shape.Print(std::cout);
      //shape.Print(std::cout);
      //el1.CalcPhysCurlShape(*tr1, curl_shape);
      //std::cout << "dofs = " << el1.GetDof() << std::endl;
      //std::cout << "shape.size = " << shape.Width() << ", " << shape.Height() << std::endl;
      //std::cout << "curl_shape.size = " << curl_shape.Width() << ", " << curl_shape.Height() << std::endl;
      for (int l = 0; l < el1.GetDof(); l++)
         for (int k = 0; k < el1.GetDof(); k++)
         {
            // Extract u and v
            mfem::Vector u(dim), v(dim);
            shape.GetRow(k, u);
            shape.GetRow(l, v);

            // Extract curl(u) and curl(v)
            //mfem::Vector curl_u(dim), curl_v(dim);
            //curl_shape.GetRow(k, curl_u);
            //curl_shape.GetRow(l, curl_v);

            //mfem::Vector n_x_curl_u(dim), n_x_curl_v(dim), n_x_u(dim), n_x_v(dim), n_x_u_x_n(dim), n_x_v_x_n(dim);
            //normal.cross3D(curl_u,n_x_curl_u);
            //normal.cross3D(curl_v,n_x_curl_v);
            //normal.cross3D(u,n_x_u);
            //normal.cross3D(v,n_x_v);
            //std::cout << weights[i] << std::endl;
            //curl_u.Print(std::cout);
            elmat.Elem(l,k) += (Trans.Weight()*ip_face.weight) *(u*v);
            //elmat.Elem(l,k) += factor_ * theta_ * weights[i] * (u * n_x_curl_v);
            //elmat.Elem(l,k) += factor_ * Cw_/(h*area) * weights[i]* (n_x_u * n_x_v); // ||n|| ~ area and n appears twice here

         } 
   }
   elmat *= .5;
   //elmat.Print(std::cout);
}

void WouterLFIntegrator::AssembleRHSElementVect(
    const mfem::FiniteElement &el, mfem::ElementTransformation &Tr, mfem::Vector &elvect)
{
   return;
}

void WouterLFIntegrator::AssembleRHSElementVect(
    const mfem::FiniteElement &el, mfem::FaceElementTransformations &Tr, mfem::Vector &elvect)
{
   int dim = el.GetDim();
   mfem::Vector normal(dim);

   mfem::IntegrationPoint ip_face;

   // Build a reasonable quadrature on the actual face geometry
   const mfem::IntegrationRule *ir = IntRule;
   ir = &mfem::IntRules.Get(static_cast<mfem::Geometry::Type>(Tr.FaceGeom),
                            2*el.GetOrder()+1);

   elvect.SetSize(el.GetDof());
   elvect = 0.;
   auto weights = ir->GetWeights();
   for (int i = 0; i < ir->GetNPoints(); ++i)
   {
      ip_face = ir->IntPoint(i);

      // Sync face + element integration points. This ensures ip on the element
      // matches the face point orientation (important for tangential fields).
      Tr.SetAllIntPoints(&ip_face);
      const mfem::IntegrationPoint &ip_elem = Tr.Elem1->GetIntPoint();

      // Face normal at this quadrature point
      mfem::CalcOrtho(Tr.Face->Jacobian(), normal);
      double h = sqrt(normal.Norml2());
      mfem::DenseMatrix shape(el.GetDof(), Tr.GetSpaceDim());
      mfem::DenseMatrix curl_shape(el.GetDof(), 3);

      mfem::ElementTransformation *tr1 = Tr.Elem1;
      el.CalcVShape(*tr1, shape);
      el.CalcPhysCurlShape(*tr1, curl_shape);

      mfem::Vector temp_out(3);
      Tr.Transform(ip_face,temp_out);


      mfem::Vector u(3);
      Q.Eval(u,Tr,ip_face);

      for (int k = 0; k < el.GetDof(); k++)
      {
         // Extract u and v
         mfem::Vector v(dim);
         shape.GetRow(k, v);

         // Extract curl(u) and curl(v)
         mfem::Vector curl_v(dim);
         curl_shape.GetRow(k, curl_v);

         mfem::Vector n_x_curl_v(dim), n_x_v(dim), n_x_u(dim);
         normal.cross3D(curl_v,n_x_curl_v);
         normal.cross3D(v,n_x_v);
         normal.cross3D(u,n_x_u);

         elvect.Elem(k) += factor_ * theta_ * weights[i] * (u * n_x_curl_v);
         elvect.Elem(k) += factor_ * Cw_/(h*h*h) * weights[i]* (n_x_u * n_x_v);
      } 
   }


}
