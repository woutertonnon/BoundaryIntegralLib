#include "BoundaryOperators.h"
#include "mfem.hpp"
#include <algorithm> // std::max


// ---------------------------------------------------------------------------
// RT_DGPenaltyIntegrator
// ---------------------------------------------------------------------------
//
// Assembles the symmetric interior-penalty bilinear form on a single
// interior face shared by el1 (Elem1) and el2 (Elem2):
//
//   (sigma * nu / h_F) * int_F [[u]] . [[v]] dF
//
// with [[u]] = u^+ - u^-  (the jump, purely tangential for RT elements).
//
// The combined element matrix has the block structure
//   [  M11  -M12 ]   rows/cols 0..dof1-1       -> el1 DOFs
//   [ -M21   M22 ]   rows/cols dof1..dof1+dof2-1 -> el2 DOFs
//
// where M_{ij}(l,k) = w * phi_i_l . phi_j_k  (vector dot product).
//
void RT_DGPenaltyIntegrator::AssembleFaceMatrix(
    const mfem::FiniteElement &el1, const mfem::FiniteElement &el2,
    mfem::FaceElementTransformations &Trans, mfem::DenseMatrix &elmat)
{
    // This integrator is only registered with AddInteriorFaceIntegrator,
    // so Elem2No should always be valid, but guard anyway.
    MFEM_ASSERT(Trans.Elem2No >= 0,
                "RT_DGPenaltyIntegrator: expected an interior face");

    const int dim  = el1.GetDim();
    const int dof1 = el1.GetDof();
    const int dof2 = el2.GetDof();
    const int ndof = dof1 + dof2;

    elmat.SetSize(ndof, ndof);
    elmat = 0.;

    const int order = 2 * std::max(el1.GetOrder(), el2.GetOrder()) + 1;
    const mfem::IntegrationRule &ir =
        mfem::IntRules.Get(Trans.FaceGeom, order);

    mfem::Vector   normal(dim);
    mfem::DenseMatrix shape1(dof1, dim), shape2(dof2, dim);

    for (int q = 0; q < ir.GetNPoints(); ++q)
    {
        const mfem::IntegrationPoint &ip = ir.IntPoint(q);
        Trans.SetAllIntPoints(&ip);

        // Outward (un-normalised) face normal; its norm is the surface
        // Jacobian determinant (physical face area / reference face area).
        Trans.Face->SetIntPoint(&ip);
        mfem::CalcOrtho(Trans.Face->Jacobian(), normal);
        const double jac  = normal.Norml2();   // |J_face|
        const double h    = std::sqrt(jac);    // characteristic face length
        const double w    = ip.weight * jac * sigma_ * factor_ / h;

        // Physical vector shape functions from each side
        el1.CalcVShape(*Trans.Elem1, shape1);
        el2.CalcVShape(*Trans.Elem2, shape2);

        mfem::Vector u1(dim), u2(dim), v1(dim), v2(dim);

        // ++ block  : int_F phi1_l . phi1_k
        for (int l = 0; l < dof1; ++l) {
            shape1.GetRow(l, v1);
            for (int k = 0; k < dof1; ++k) {
                shape1.GetRow(k, u1);
                elmat(l, k) += w * (u1 * v1);
            }
        }
        // +- block  : -int_F phi1_l . phi2_k
        for (int l = 0; l < dof1; ++l) {
            shape1.GetRow(l, v1);
            for (int k = 0; k < dof2; ++k) {
                shape2.GetRow(k, u2);
                elmat(l, dof1 + k) -= w * (u2 * v1);
            }
        }
        // -+ block  : -int_F phi2_l . phi1_k
        for (int l = 0; l < dof2; ++l) {
            shape2.GetRow(l, v2);
            for (int k = 0; k < dof1; ++k) {
                shape1.GetRow(k, u1);
                elmat(dof1 + l, k) -= w * (u1 * v2);
            }
        }
        // -- block  : +int_F phi2_l . phi2_k
        for (int l = 0; l < dof2; ++l) {
            shape2.GetRow(l, v2);
            for (int k = 0; k < dof2; ++k) {
                shape2.GetRow(k, u2);
                elmat(dof1 + l, dof1 + k) += w * (u2 * v2);
            }
        }
    }
}


// ---------------------------------------------------------------------------
// ND_DGPenaltyIntegrator
// ---------------------------------------------------------------------------
//
// Assembles the symmetric interior-penalty bilinear form on a single
// interior face shared by el1 and el2:
//
//   (sigma * nu / h_F) * int_F [[u]] . [[v]] dF
//
// with [[u]] = u^+ - u^-  (the jump, purely normal for conforming ND elements).
//
void ND_DGPenaltyIntegrator::AssembleFaceMatrix(
    const mfem::FiniteElement &el1, const mfem::FiniteElement &el2,
    mfem::FaceElementTransformations &Trans, mfem::DenseMatrix &elmat)
{
    MFEM_ASSERT(Trans.Elem2No >= 0,
                "ND_DGPenaltyIntegrator: expected an interior face");

    const int dim  = el1.GetDim();
    const int dof1 = el1.GetDof();
    const int dof2 = el2.GetDof();
    const int ndof = dof1 + dof2;

    elmat.SetSize(ndof, ndof);
    elmat = 0.;

    const int order = 2 * std::max(el1.GetOrder(), el2.GetOrder()) + 1;
    const mfem::IntegrationRule &ir =
        mfem::IntRules.Get(Trans.FaceGeom, order);

    mfem::Vector   normal(dim);
    mfem::DenseMatrix shape1(dof1, dim), shape2(dof2, dim);

    for (int q = 0; q < ir.GetNPoints(); ++q)
    {
        const mfem::IntegrationPoint &ip = ir.IntPoint(q);
        Trans.SetAllIntPoints(&ip);

        Trans.Face->SetIntPoint(&ip);
        mfem::CalcOrtho(Trans.Face->Jacobian(), normal);
        const double jac  = normal.Norml2();
        const double h    = std::sqrt(jac);
        const double w    = ip.weight * jac * sigma_ * factor_ / h;

        el1.CalcVShape(*Trans.Elem1, shape1);
        el2.CalcVShape(*Trans.Elem2, shape2);

        mfem::Vector u1(dim), u2(dim), v1(dim), v2(dim);

        for (int l = 0; l < dof1; ++l) {
            shape1.GetRow(l, v1);
            for (int k = 0; k < dof1; ++k) {
                shape1.GetRow(k, u1);
                elmat(l, k) += w * (u1 * v1);
            }
        }
        for (int l = 0; l < dof1; ++l) {
            shape1.GetRow(l, v1);
            for (int k = 0; k < dof2; ++k) {
                shape2.GetRow(k, u2);
                elmat(l, dof1 + k) -= w * (u2 * v1);
            }
        }
        for (int l = 0; l < dof2; ++l) {
            shape2.GetRow(l, v2);
            for (int k = 0; k < dof1; ++k) {
                shape1.GetRow(k, u1);
                elmat(dof1 + l, k) -= w * (u1 * v2);
            }
        }
        for (int l = 0; l < dof2; ++l) {
            shape2.GetRow(l, v2);
            for (int k = 0; k < dof2; ++k) {
                shape2.GetRow(k, u2);
                elmat(dof1 + l, dof1 + k) += w * (u2 * v2);
            }
        }
    }
}


// ---------------------------------------------------------------------------
// ND_CurlJumpIntegrator
// ---------------------------------------------------------------------------
//
// Assembles the ghost-penalty bilinear form on a single interior face:
//
//   (gamma * nu * h_F) * int_F [[curl u]] . [[curl v]] dF
//
// with [[curl u]] = curl(u^+) - curl(u^-)  (the vorticity jump across the face).
// h_F = sqrt(|J_face|) is the characteristic face length.
//
void ND_CurlJumpIntegrator::AssembleFaceMatrix(
    const mfem::FiniteElement &el1, const mfem::FiniteElement &el2,
    mfem::FaceElementTransformations &Trans, mfem::DenseMatrix &elmat)
{
    MFEM_ASSERT(Trans.Elem2No >= 0,
                "ND_CurlJumpIntegrator: expected an interior face");

    const int dim  = el1.GetDim();
    const int cdim = (dim == 3) ? 3 : 1;   // curl lives in R^3 (3D) or R^1 (2D)
    const int dof1 = el1.GetDof();
    const int dof2 = el2.GetDof();
    const int ndof = dof1 + dof2;

    elmat.SetSize(ndof, ndof);
    elmat = 0.;

    const int order = 2 * std::max(el1.GetOrder(), el2.GetOrder()) + 1;
    const mfem::IntegrationRule &ir =
        mfem::IntRules.Get(Trans.FaceGeom, order);

    mfem::Vector normal(dim);
    mfem::DenseMatrix curl_shape1(dof1, cdim), curl_shape2(dof2, cdim);

    for (int q = 0; q < ir.GetNPoints(); ++q)
    {
        const mfem::IntegrationPoint &ip = ir.IntPoint(q);
        Trans.SetAllIntPoints(&ip);

        Trans.Face->SetIntPoint(&ip);
        mfem::CalcOrtho(Trans.Face->Jacobian(), normal);
        const double jac = normal.Norml2();   // |J_face|, dimension length^2
        const double h   = std::sqrt(jac);    // h_F ~ face length
        // Ghost-penalty weight: gamma * nu * h_F * dA  (h on top, not under)
        const double w   = ip.weight * jac * gamma_ * factor_ * h;

        el1.CalcPhysCurlShape(*Trans.Elem1, curl_shape1);
        el2.CalcPhysCurlShape(*Trans.Elem2, curl_shape2);

        mfem::Vector cu1(cdim), cu2(cdim), cv1(cdim), cv2(cdim);

        // ++ block
        for (int l = 0; l < dof1; ++l) {
            curl_shape1.GetRow(l, cv1);
            for (int k = 0; k < dof1; ++k) {
                curl_shape1.GetRow(k, cu1);
                elmat(l, k) += w * (cu1 * cv1);
            }
        }
        // +- block
        for (int l = 0; l < dof1; ++l) {
            curl_shape1.GetRow(l, cv1);
            for (int k = 0; k < dof2; ++k) {
                curl_shape2.GetRow(k, cu2);
                elmat(l, dof1 + k) -= w * (cu2 * cv1);
            }
        }
        // -+ block
        for (int l = 0; l < dof2; ++l) {
            curl_shape2.GetRow(l, cv2);
            for (int k = 0; k < dof1; ++k) {
                curl_shape1.GetRow(k, cu1);
                elmat(dof1 + l, k) -= w * (cu1 * cv2);
            }
        }
        // -- block
        for (int l = 0; l < dof2; ++l) {
            curl_shape2.GetRow(l, cv2);
            for (int k = 0; k < dof2; ++k) {
                curl_shape2.GetRow(k, cu2);
                elmat(dof1 + l, dof1 + k) += w * (cu2 * cv2);
            }
        }
    }
}


// ---------------------------------------------------------------------------
// ND_UpwindIntegrator
// ---------------------------------------------------------------------------
//
// Assembles Heumann's upwind bilinear form on a single interior face:
//
//   a_upwind(u,v) = factor * int_F |w.n_F| [[u]] . [[v]] dF
//
// For conforming ND elements [[u]] is purely normal, so this penalises
// normal-component oscillations with a wind-adaptive coefficient |w.n_F|.
//
// The weight |w.n_F| * dA = |w . n_J| where n_J is the un-normalised face
// normal (from CalcOrtho), so the area Jacobian cancels naturally.
//
void ND_UpwindIntegrator::AssembleFaceMatrix(
    const mfem::FiniteElement &el1, const mfem::FiniteElement &el2,
    mfem::FaceElementTransformations &Trans, mfem::DenseMatrix &elmat)
{
    MFEM_ASSERT(Trans.Elem2No >= 0,
                "ND_UpwindIntegrator: expected an interior face");

    const int dim  = el1.GetDim();
    const int dof1 = el1.GetDof();
    const int dof2 = el2.GetDof();
    const int ndof = dof1 + dof2;

    elmat.SetSize(ndof, ndof);
    elmat = 0.;

    const int order = 2 * std::max(el1.GetOrder(), el2.GetOrder()) + 1;
    const mfem::IntegrationRule &ir =
        mfem::IntRules.Get(Trans.FaceGeom, order);

    mfem::Vector   normal(dim);
    mfem::DenseMatrix shape1(dof1, dim), shape2(dof2, dim);
    mfem::Vector   w_eval(dim);

    for (int q = 0; q < ir.GetNPoints(); ++q)
    {
        const mfem::IntegrationPoint &ip = ir.IntPoint(q);
        Trans.SetAllIntPoints(&ip);

        Trans.Face->SetIntPoint(&ip);
        mfem::CalcOrtho(Trans.Face->Jacobian(), normal);  // un-normalised; |normal| = dA/d_xi

        // Evaluate wind at the element-1 reference point.
        // CurlGridFunctionCoefficient requires an ElementTransformation (not face).
        w_coef_.Eval(w_eval, *Trans.Elem1, Trans.Elem1->GetIntPoint());

        // |w . n_hat| * dA = |w . n_J|  (un-normalised dot, Jacobian cancels)
        const double abs_w_dot_n = std::abs(w_eval * normal);
        const double w = ip.weight * abs_w_dot_n * factor_;

        el1.CalcVShape(*Trans.Elem1, shape1);
        el2.CalcVShape(*Trans.Elem2, shape2);

        mfem::Vector u1(dim), u2(dim), v1(dim), v2(dim);

        // ++ block
        for (int l = 0; l < dof1; ++l) {
            shape1.GetRow(l, v1);
            for (int k = 0; k < dof1; ++k) {
                shape1.GetRow(k, u1);
                elmat(l, k) += w * (u1 * v1);
            }
        }
        // +- block
        for (int l = 0; l < dof1; ++l) {
            shape1.GetRow(l, v1);
            for (int k = 0; k < dof2; ++k) {
                shape2.GetRow(k, u2);
                elmat(l, dof1 + k) -= w * (u2 * v1);
            }
        }
        // -+ block
        for (int l = 0; l < dof2; ++l) {
            shape2.GetRow(l, v2);
            for (int k = 0; k < dof1; ++k) {
                shape1.GetRow(k, u1);
                elmat(dof1 + l, k) -= w * (u1 * v2);
            }
        }
        // -- block
        for (int l = 0; l < dof2; ++l) {
            shape2.GetRow(l, v2);
            for (int k = 0; k < dof2; ++k) {
                shape2.GetRow(k, u2);
                elmat(dof1 + l, dof1 + k) += w * (u2 * v2);
            }
        }
    }
}


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
      double area = normal.Norml2();
      double h = sqrt(area);
      normal *= 1./area;

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

         elvect.Elem(k) += factor_ * theta_ * weights[i] * area * (u * n_x_curl_v);
         elvect.Elem(k) += factor_ * Cw_/h * weights[i] * area * (n_x_u * n_x_v);

      } 
   }


}
