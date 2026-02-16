#include "BoundaryOperators.h"
#include "mfem.hpp"



void ND_NitscheIntegrator::AssembleElementMatrix(const mfem::FiniteElement &el, mfem::ElementTransformation &Trans,
                                             mfem::DenseMatrix &elmat)
{
   MFEM_ABORT("ND_NitscheIntegrator::AssembleElementMatrix(): method is not implemented for this class");
}

void ND_NitscheIntegrator::AssembleFaceMatrix(
    const mfem::FiniteElement &el1, const mfem::FiniteElement &el2,
    mfem::FaceElementTransformations &Trans, mfem::DenseMatrix &elmat)
{
   MFEM_ASSERT(Trans.Elem2No < 0,
               "support for interior faces is not implemented");

   int dim = el1.GetDim();
   mfem::Vector normal(dim);

   mfem::IntegrationPoint ip_face;

   // Build a reasonable quadrature on the actual face geometry
   const mfem::IntegrationRule *ir = IntRule;
   ir = &mfem::IntRules.Get(static_cast<mfem::Geometry::Type>(Trans.FaceGeom),
                            2*el1.GetOrder()+1);

   elmat.SetSize(el1.GetDof(), el1.GetDof());
   elmat = 0.;
   auto weights = ir->GetWeights();

   for (int i = 0; i < ir->GetNPoints(); ++i)
   {
      ip_face = ir->IntPoint(i);

      // Sync face + element integration points. This ensures ip on the element
      // matches the face point orientation (important for tangential fields).
      Trans.SetAllIntPoints(&ip_face);
      const mfem::IntegrationPoint &ip_elem = Trans.Elem1->GetIntPoint();

      // Face normal atu this quadrature point
      Trans.Face->SetIntPoint(&ip_face);
      mfem::CalcOrtho(Trans.Face->Jacobian(), normal);
      
      double area = normal.Norml2();
      double h = sqrt(area);
      normal *= 1./area; //normalize n

      mfem::DenseMatrix shape(el1.GetDof(), Trans.GetSpaceDim());
      mfem::DenseMatrix curl_shape(el1.GetDof(), 3);

      mfem::ElementTransformation *tr1 = Trans.Elem1;
      //tr1->SetIntPoint(&ip_face);
      el1.CalcVShape(*tr1, shape);
      el1.CalcPhysCurlShape(*tr1, curl_shape);

      mfem::Vector temp_out(3);
      Trans.Transform(ip_face,temp_out);

      for (int l = 0; l < el1.GetDof(); l++)
         for (int k = 0; k < el1.GetDof(); k++)
         {
            mfem::Vector u(dim), v(dim);
            shape.GetRow(k, u);
            shape.GetRow(l, v);
	    
            mfem::Vector curl_u(dim), curl_v(dim);
            curl_shape.GetRow(k, curl_u);
            curl_shape.GetRow(l, curl_v);

            mfem::Vector n_x_curl_u(dim), n_x_curl_v(dim), n_x_u(dim), n_x_v(dim), n_x_u_x_n(dim), n_x_v_x_n(dim);
            normal.cross3D(curl_u,n_x_curl_u);
            normal.cross3D(curl_v,n_x_curl_v);
            normal.cross3D(u,n_x_u);
            normal.cross3D(v,n_x_v);

            elmat.Elem(l,k) += factor_ * weights[i] * area * (n_x_curl_u * v);
            elmat.Elem(l,k) += factor_ * theta_ * weights[i] * area * (u * n_x_curl_v);
            elmat.Elem(l,k) += factor_ * Cw_/h * weights[i] * area * (n_x_u * n_x_v);

         } 
   }
}

void ND_NitscheLFIntegrator::AssembleRHSElementVect(
    const mfem::FiniteElement &el, mfem::ElementTransformation &Tr, mfem::Vector &elvect)
{
   MFEM_ABORT("ND_NitscheIntegrator::AssembleRHSElementVect(): method is not implemented for this class");
}

void ND_NitscheLFIntegrator::AssembleRHSElementVect(
    const mfem::FiniteElement &el, mfem::FaceElementTransformations &Tr, mfem::Vector &elvect)
{
   int dim = el.GetDim();
   mfem::Vector normal(dim);

   mfem::IntegrationPoint ip_face;

   // Build a reasonable quadrature on the actual face geometry
   const mfem::IntegrationRule *ir = IntRule;
   ir = &mfem::IntRules.Get(static_cast<mfem::Geometry::Type>(Tr.FaceGeom),
                            2*el.GetOrder()+12);

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
