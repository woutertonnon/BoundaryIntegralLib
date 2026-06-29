#include "BoundaryOperators.h"
#include "mfem.hpp"
#include <algorithm> // std::max

namespace
{
// ---------------------------------------------------------------------------
// 2D/3D embedding helpers for the H(curl) Nitsche integrators.
//
// The curl-curl Nitsche boundary form involves the cross products n x u and
// n x curl u.  In 3D both operands are genuine 3-vectors.  In 2D a Nedelec
// field u is a 2-vector, and its curl is the *scalar* omega = d_x u_y - d_y u_x.
//
// Rather than re-derive the 2D weak form (error-prone sign bookkeeping), we
// embed the 2D problem in 3D: the domain lies in the z = 0 plane, vector
// fields have zero z-component, and the scalar curl becomes the z-component
// (0, 0, omega).  Under this embedding the 3D identities — and hence the
// existing, validated cross3D-based formulas — hold verbatim, so the 2D
// operator is the exact planar reduction of the 3D curl-curl Nitsche form.
//
// embedVec3:  (a_x, a_y[, 0])           -> 3-vector
// embedCurl3: 3D curl as-is; 2D scalar  -> (0, 0, omega)
// ---------------------------------------------------------------------------
inline mfem::Vector embedVec3(const mfem::Vector &a)
{
    mfem::Vector e(3);
    e = 0.0;
    for (int d = 0; d < a.Size() && d < 3; ++d) { e[d] = a[d]; }
    return e;
}

inline mfem::Vector embedCurl3(const mfem::Vector &c)
{
    mfem::Vector e(3);
    e = 0.0;
    if (c.Size() == 1) { e[2] = c[0]; }              // 2D: scalar curl -> +z
    else { for (int d = 0; d < c.Size() && d < 3; ++d) { e[d] = c[d]; } }
    return e;
}
} // anonymous namespace


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


// ---------------------------------------------------------------------------
// RT_DivJumpIntegrator
// ---------------------------------------------------------------------------
//
// Assembles the symmetric ghost penalty on a single interior face:
//
//   (gamma * nu * h_F) * int_F [[div u]] [[div v]] dF
//
// where [[div u]] = div(u^+) - div(u^-).  For RT elements div(u) is a
// scalar in L2, so the jump is a scalar quantity.
//
// The combined element matrix has the block structure
//   [  M11  -M12 ]
//   [ -M21   M22 ]
//
void RT_DivJumpIntegrator::AssembleFaceMatrix(
    const mfem::FiniteElement &el1, const mfem::FiniteElement &el2,
    mfem::FaceElementTransformations &Trans, mfem::DenseMatrix &elmat)
{
    MFEM_ASSERT(Trans.Elem2No >= 0,
                "RT_DivJumpIntegrator: expected an interior face");

    const int dof1 = el1.GetDof();
    const int dof2 = el2.GetDof();
    const int ndof = dof1 + dof2;
    const int dim  = el1.GetDim();

    elmat.SetSize(ndof, ndof);
    elmat = 0.;

    const int order = 2 * std::max(el1.GetOrder(), el2.GetOrder()) + 1;
    const mfem::IntegrationRule &ir =
        mfem::IntRules.Get(Trans.FaceGeom, order);

    mfem::Vector normal(dim);
    mfem::Vector div_shape1(dof1), div_shape2(dof2);

    for (int q = 0; q < ir.GetNPoints(); ++q)
    {
        const mfem::IntegrationPoint &ip = ir.IntPoint(q);
        Trans.SetAllIntPoints(&ip);

        Trans.Face->SetIntPoint(&ip);
        mfem::CalcOrtho(Trans.Face->Jacobian(), normal);
        const double jac = normal.Norml2();   // |J_face|
        const double h   = std::sqrt(jac);    // h_F ~ face length
        // Ghost-penalty weight: gamma * nu * h_F * dA
        const double w   = ip.weight * jac * gamma_ * factor_ * h;

        el1.CalcPhysDivShape(*Trans.Elem1, div_shape1);
        el2.CalcPhysDivShape(*Trans.Elem2, div_shape2);

        // ++ block
        for (int l = 0; l < dof1; ++l)
            for (int k = 0; k < dof1; ++k)
                elmat(l, k) += w * div_shape1(k) * div_shape1(l);

        // +- block
        for (int l = 0; l < dof1; ++l)
            for (int k = 0; k < dof2; ++k)
                elmat(l, dof1 + k) -= w * div_shape2(k) * div_shape1(l);

        // -+ block
        for (int l = 0; l < dof2; ++l)
            for (int k = 0; k < dof1; ++k)
                elmat(dof1 + l, k) -= w * div_shape1(k) * div_shape2(l);

        // -- block
        for (int l = 0; l < dof2; ++l)
            for (int k = 0; k < dof2; ++k)
                elmat(dof1 + l, dof1 + k) += w * div_shape2(k) * div_shape2(l);
    }
}

// ---------------------------------------------------------------------------
// RT_BdrTangentPenaltyIntegrator
// ---------------------------------------------------------------------------
//
// Assembles the boundary penalty on each boundary face F:
//
//   (Cw / h_F) * int_F (n x u) . (n x v) dF
//
void RT_BdrTangentPenaltyIntegrator::AssembleFaceMatrix(
    const mfem::FiniteElement &el1, const mfem::FiniteElement &el2,
    mfem::FaceElementTransformations &Trans, mfem::DenseMatrix &elmat)
{
    MFEM_ASSERT(Trans.Elem2No < 0,
                "RT_BdrTangentPenaltyIntegrator: expected a boundary face");

    const int dof = el1.GetDof();
    const int dim = el1.GetDim();

    elmat.SetSize(dof, dof);
    elmat = 0.0;

    const mfem::IntegrationRule &ir = mfem::IntRules.Get(
        static_cast<mfem::Geometry::Type>(Trans.FaceGeom),
        2 * el1.GetOrder() + 1);

    mfem::Vector normal(dim);
    mfem::DenseMatrix shape(dof, dim);

    for (int q = 0; q < ir.GetNPoints(); ++q)
    {
        const mfem::IntegrationPoint &ip_face = ir.IntPoint(q);
        Trans.SetAllIntPoints(&ip_face);

        Trans.Face->SetIntPoint(&ip_face);
        mfem::CalcOrtho(Trans.Face->Jacobian(), normal);

        double area = normal.Norml2();
        double h = sqrt(area);
        normal *= 1.0 / area; // unit normal

        el1.CalcVShape(*Trans.Elem1, shape);

        double w = ip_face.weight * area * Cw_ / h;

        for (int l = 0; l < dof; ++l)
        {
            mfem::Vector v(dim), n_x_v(dim);
            shape.GetRow(l, v);
            normal.cross3D(v, n_x_v);

            for (int k = 0; k < dof; ++k)
            {
                mfem::Vector u(dim), n_x_u(dim);
                shape.GetRow(k, u);
                normal.cross3D(u, n_x_u);

                elmat(l, k) += w * (n_x_u * n_x_v);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// RT_ND_BdrCrossProductIntegrator
// ---------------------------------------------------------------------------
//
// Assembles the boundary face matrix for the mixed bilinear form:
//
//   b_bdr(u, eta) = int_F (n x u_RT) . eta_ND dS
//
// trial_fe1 is the RT element (trial), test_fe1 is the ND element (test).
// The output elmat has size (test_dof x trial_dof).
//
void RT_ND_BdrCrossProductIntegrator::AssembleFaceMatrix(
    const mfem::FiniteElement &trial_fe1,
    const mfem::FiniteElement &test_fe1,
    const mfem::FiniteElement &trial_fe2,
    const mfem::FiniteElement &test_fe2,
    mfem::FaceElementTransformations &Trans,
    mfem::DenseMatrix &elmat)
{
    MFEM_ASSERT(Trans.Elem2No < 0,
                "RT_ND_BdrCrossProductIntegrator: expected a boundary face");

    const int trial_dof = trial_fe1.GetDof(); // RT DOFs
    const int test_dof  = test_fe1.GetDof();  // ND DOFs
    const int dim       = trial_fe1.GetDim();

    elmat.SetSize(test_dof, trial_dof);
    elmat = 0.0;

    const mfem::IntegrationRule &ir = mfem::IntRules.Get(
        static_cast<mfem::Geometry::Type>(Trans.FaceGeom),
        2 * std::max(trial_fe1.GetOrder(), test_fe1.GetOrder()) + 1);

    mfem::Vector normal(dim);
    mfem::DenseMatrix trial_shape(trial_dof, dim); // RT shape
    mfem::DenseMatrix test_shape(test_dof, dim);   // ND shape

    for (int q = 0; q < ir.GetNPoints(); ++q)
    {
        const mfem::IntegrationPoint &ip_face = ir.IntPoint(q);
        Trans.SetAllIntPoints(&ip_face);

        // Outward normal (unnormalised, |normal| = face area element)
        Trans.Face->SetIntPoint(&ip_face);
        mfem::CalcOrtho(Trans.Face->Jacobian(), normal);

        // Evaluate shapes on the volume element (Elem1 side)
        trial_fe1.CalcVShape(*Trans.Elem1, trial_shape);
        test_fe1.CalcVShape(*Trans.Elem1, test_shape);

        double w = ip_face.weight; // face quadrature weight
        // Note: CalcOrtho already includes the face Jacobian determinant,
        // so w * normal gives the full measure-weighted normal.

        for (int j = 0; j < test_dof; ++j)
        {
            mfem::Vector eta(dim);
            test_shape.GetRow(j, eta);

            for (int i = 0; i < trial_dof; ++i)
            {
                mfem::Vector u(dim), n_cross_u(dim);
                trial_shape.GetRow(i, u);
                normal.cross3D(u, n_cross_u); // n x u

                elmat(j, i) += w * (n_cross_u * eta);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// RT_BdrTangentPenaltyLFIntegrator
// ---------------------------------------------------------------------------
//
// Assembles the RHS consistency term on each boundary face F:
//
//   (Cw / h_F) * int_F (n x u_D) . (n x v) dF
//
void RT_BdrTangentPenaltyLFIntegrator::AssembleRHSElementVect(
    const mfem::FiniteElement &el, mfem::ElementTransformation &Tr,
    mfem::Vector &elvect)
{
    MFEM_ABORT("RT_BdrTangentPenaltyLFIntegrator: element assembly not supported; "
               "use AddBdrFaceIntegrator");
}

void RT_BdrTangentPenaltyLFIntegrator::AssembleRHSElementVect(
    const mfem::FiniteElement &el, mfem::FaceElementTransformations &Tr,
    mfem::Vector &elvect)
{
    MFEM_ASSERT(Tr.Elem2No < 0,
                "RT_BdrTangentPenaltyLFIntegrator: expected a boundary face");

    const int dof = el.GetDof();
    const int dim = el.GetDim();

    elvect.SetSize(dof);
    elvect = 0.0;

    const mfem::IntegrationRule &ir = mfem::IntRules.Get(
        static_cast<mfem::Geometry::Type>(Tr.FaceGeom),
        2 * el.GetOrder() + 1);

    mfem::Vector normal(dim), uD_val(dim);
    mfem::DenseMatrix shape(dof, dim);

    for (int q = 0; q < ir.GetNPoints(); ++q)
    {
        const mfem::IntegrationPoint &ip_face = ir.IntPoint(q);
        Tr.SetAllIntPoints(&ip_face);

        Tr.Face->SetIntPoint(&ip_face);
        mfem::CalcOrtho(Tr.Face->Jacobian(), normal);

        double area = normal.Norml2();
        double h = sqrt(area);
        normal *= 1.0 / area; // unit normal

        el.CalcVShape(*Tr.Elem1, shape);

        // Evaluate boundary data u_D at this point
        mfem::Vector phys_pt;
        Tr.Transform(ip_face, phys_pt);
        uD_.Eval(uD_val, *Tr.Elem1, Tr.Elem1->GetIntPoint());

        mfem::Vector n_x_uD(dim);
        normal.cross3D(uD_val, n_x_uD);

        double w = ip_face.weight * area * Cw_ / h;

        for (int j = 0; j < dof; ++j)
        {
            mfem::Vector v(dim), n_x_v(dim);
            shape.GetRow(j, v);
            normal.cross3D(v, n_x_v);

            elvect(j) += w * (n_x_uD * n_x_v);
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

      const int sdim = Trans.GetSpaceDim();
      const int cdim = (dim == 3) ? 3 : 1;   // curl: 3-vector (3D) or scalar (2D)
      mfem::DenseMatrix shape(el1.GetDof(), sdim);
      mfem::DenseMatrix curl_shape(el1.GetDof(), cdim);

      mfem::ElementTransformation *tr1 = Trans.Elem1;
      //tr1->SetIntPoint(&ip_face);
      el1.CalcVShape(*tr1, shape);
      el1.CalcPhysCurlShape(*tr1, curl_shape);

      mfem::Vector temp_out(sdim);
      Trans.Transform(ip_face,temp_out);

      // Embed the (normalized) face normal into 3D once per quad point.
      mfem::Vector n3 = embedVec3(normal);

      for (int l = 0; l < el1.GetDof(); l++)
         for (int k = 0; k < el1.GetDof(); k++)
         {
            mfem::Vector u(sdim), v(sdim);
            shape.GetRow(k, u);
            shape.GetRow(l, v);

            mfem::Vector curl_u(cdim), curl_v(cdim);
            curl_shape.GetRow(k, curl_u);
            curl_shape.GetRow(l, curl_v);

            // Embed into 3D so the 3D curl-curl Nitsche identities apply
            // verbatim (planar reduction in 2D, identity in 3D).
            mfem::Vector u3 = embedVec3(u),  v3 = embedVec3(v);
            mfem::Vector cu3 = embedCurl3(curl_u), cv3 = embedCurl3(curl_v);

            mfem::Vector n_x_curl_u(3), n_x_curl_v(3), n_x_u(3), n_x_v(3);
            n3.cross3D(cu3, n_x_curl_u);
            n3.cross3D(cv3, n_x_curl_v);
            n3.cross3D(u3,  n_x_u);
            n3.cross3D(v3,  n_x_v);

            elmat.Elem(l,k) += factor_ * weights[i] * area * (n_x_curl_u * v3);
            elmat.Elem(l,k) += factor_ * theta_ * weights[i] * area * (u3 * n_x_curl_v);
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

      const int sdim = Tr.GetSpaceDim();
      const int cdim = (dim == 3) ? 3 : 1;   // curl: 3-vector (3D) or scalar (2D)
      mfem::DenseMatrix shape(el.GetDof(), sdim);
      mfem::DenseMatrix curl_shape(el.GetDof(), cdim);

      mfem::ElementTransformation *tr1 = Tr.Elem1;
      el.CalcVShape(*tr1, shape);
      el.CalcPhysCurlShape(*tr1, curl_shape);

      mfem::Vector temp_out(sdim);
      Tr.Transform(ip_face,temp_out);

      // Prescribed boundary velocity u_D (vector field).
      mfem::Vector u(Q.GetVDim());
      Q.Eval(u,Tr,ip_face);

      // Embed normal and boundary data into 3D once per quad point.
      mfem::Vector n3 = embedVec3(normal);
      mfem::Vector u3 = embedVec3(u);
      mfem::Vector n_x_u(3);
      n3.cross3D(u3, n_x_u);

      for (int k = 0; k < el.GetDof(); k++)
      {
         // Test function v and its curl (scalar in 2D, 3-vector in 3D).
         mfem::Vector v(sdim);
         shape.GetRow(k, v);

         mfem::Vector curl_v(cdim);
         curl_shape.GetRow(k, curl_v);

         mfem::Vector v3 = embedVec3(v), cv3 = embedCurl3(curl_v);

         mfem::Vector n_x_curl_v(3), n_x_v(3);
         n3.cross3D(cv3, n_x_curl_v);
         n3.cross3D(v3,  n_x_v);

         elvect.Elem(k) += factor_ * theta_ * weights[i] * area * (u3 * n_x_curl_v);
         elvect.Elem(k) += factor_ * Cw_/h * weights[i] * area * (n_x_u * n_x_v);

      }
   }


}
