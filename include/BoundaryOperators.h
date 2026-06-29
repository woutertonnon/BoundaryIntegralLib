#ifndef SEMILAGRANGE0FORMS_BOUNDARYOPERATORS_H
#define SEMILAGRANGE0FORMS_BOUNDARYOPERATORS_H

#include <mfem.hpp>

// Interior-face DG penalty for H(div) / RT spaces.
//
// Adds the symmetric interior penalty:
//
//   s_h(u,v) = (sigma * factor / h_F) * sum_{F interior} int_F [[u]] . [[v]] dF
//
// where [[u]] = u^+ - u^- is the jump across the face.  For RT elements the
// normal component is single-valued, so [[u]] is purely tangential and this
// term controls the otherwise-free tangential DOF oscillations that arise
// when viscosity enters only through the vorticity coupling BT*D^{-1}*B.
//
// Usage: blf.AddInteriorFaceIntegrator(new RT_DGPenaltyIntegrator(sigma, nu));
class RT_DGPenaltyIntegrator : public mfem::BilinearFormIntegrator
{
protected:
    double sigma_, factor_;

public:
    RT_DGPenaltyIntegrator(double sigma, double factor = 1.)
        : sigma_(sigma), factor_(factor) {}

    virtual void AssembleElementMatrix(const mfem::FiniteElement &el,
                                       mfem::ElementTransformation &Trans,
                                       mfem::DenseMatrix &elmat)
    {
        MFEM_ABORT("RT_DGPenaltyIntegrator: only interior-face assembly is supported");
    }

    void AssembleFaceMatrix(const mfem::FiniteElement &el1,
                            const mfem::FiniteElement &el2,
                            mfem::FaceElementTransformations &Trans,
                            mfem::DenseMatrix &elmat);
};

// Interior-face DG penalty for H(curl) / ND spaces.
//
// Adds the symmetric interior penalty:
//
//   s_h(u,v) = (sigma * factor / h_F) * sum_{F interior} int_F [[u]] . [[v]] dF
//
// where [[u]] = u^+ - u^- is the jump across the face.  For conforming ND
// elements the tangential component is single-valued, so [[u]] is purely
// normal and this term controls normal-component oscillations across faces.
//
// Usage: blf.AddInteriorFaceIntegrator(new ND_DGPenaltyIntegrator(sigma, nu));
class ND_DGPenaltyIntegrator : public mfem::BilinearFormIntegrator
{
protected:
    double sigma_, factor_;

public:
    ND_DGPenaltyIntegrator(double sigma, double factor = 1.)
        : sigma_(sigma), factor_(factor) {}

    virtual void AssembleElementMatrix(const mfem::FiniteElement &el,
                                       mfem::ElementTransformation &Trans,
                                       mfem::DenseMatrix &elmat)
    {
        MFEM_ABORT("ND_DGPenaltyIntegrator: only interior-face assembly is supported");
    }

    void AssembleFaceMatrix(const mfem::FiniteElement &el1,
                            const mfem::FiniteElement &el2,
                            mfem::FaceElementTransformations &Trans,
                            mfem::DenseMatrix &elmat);
};

// Interior-face curl-jump ghost penalty for H(curl) / ND spaces.
//
// Adds the symmetric ghost penalty:
//
//   g_h(u,v) = (gamma * factor * h_F) * sum_{F interior} int_F [[curl u]] . [[curl v]] dF
//
// where [[curl u]] = curl(u^+) - curl(u^-) is the vorticity jump across the face.
// For ND elements curl(u) is discontinuous at faces; these jumps feed directly into
// the lagged convection term (curl(u^n) x u, v) and seed spurious oscillations.
//
// Scaling is h_F (not 1/h_F): the ghost-penalty scaling.  Dimensionally,
// [[curl u]] ~ u/h, so (h_F) * ||[[curl u]]||^2 * h^2 ~ u^2 * h matches the
// volume curl-curl term nu*(curl u, curl u) ~ nu*u^2*h.  The penalty therefore
// vanishes as h->0 and does not degrade the optimal convergence rate.
//
// Usage: blf.AddInteriorFaceIntegrator(new ND_CurlJumpIntegrator(gamma, nu));
class ND_CurlJumpIntegrator : public mfem::BilinearFormIntegrator
{
protected:
    double gamma_, factor_;

public:
    ND_CurlJumpIntegrator(double gamma, double factor = 1.)
        : gamma_(gamma), factor_(factor) {}

    virtual void AssembleElementMatrix(const mfem::FiniteElement &el,
                                       mfem::ElementTransformation &Trans,
                                       mfem::DenseMatrix &elmat)
    {
        MFEM_ABORT("ND_CurlJumpIntegrator: only interior-face assembly is supported");
    }

    void AssembleFaceMatrix(const mfem::FiniteElement &el1,
                            const mfem::FiniteElement &el2,
                            mfem::FaceElementTransformations &Trans,
                            mfem::DenseMatrix &elmat);
};

// Interior-face upwind stabilization for H(curl) / ND spaces, following
// Heumann, Hiptmair, Pagliantini (2016) SIAM J. Sci. Comput.
// "Stabilized Galerkin for transient advection of differential forms."
//
// For a 1-form u advected by wind w, Heumann's scheme adds on each interior face F:
//
//   a_upwind(u,v) = factor * sum_{F interior} int_F |w.n_F| [[u]] . [[v]] dF
//
// where [[u]] = u^+ - u^- is the jump across the face.
// For conforming ND elements the tangential component is single-valued
// ([[n x u]] = 0), so [[u]] is purely normal: [[u]] = [[u.n]] n_hat.
// The term therefore penalises normal-component oscillations with a
// wind-dependent coefficient |w.n_F| (wind-adaptive, O(1) in h).
//
// Key properties:
//   - Symmetric positive semi-definite (numerical dissipation, not anti-dissipation)
//   - Coefficient |w.n_F| is O(1), matching the convective scaling of the operator
//   - Must be re-assembled each timestep as the wind w evolves
//   - Energy conservation: (w x u, u) = 0 still holds in the volume term;
//     the upwind term adds a controlled amount of dissipation at faces
//
// Usage: blf.AddInteriorFaceIntegrator(new ND_UpwindIntegrator(w_coef));
class ND_UpwindIntegrator : public mfem::BilinearFormIntegrator
{
protected:
    mfem::VectorCoefficient &w_coef_;
    double factor_;

public:
    ND_UpwindIntegrator(mfem::VectorCoefficient &w_coef, double factor = 1.)
        : w_coef_(w_coef), factor_(factor) {}

    virtual void AssembleElementMatrix(const mfem::FiniteElement &el,
                                       mfem::ElementTransformation &Trans,
                                       mfem::DenseMatrix &elmat)
    {
        MFEM_ABORT("ND_UpwindIntegrator: only interior-face assembly is supported");
    }

    void AssembleFaceMatrix(const mfem::FiniteElement &el1,
                            const mfem::FiniteElement &el2,
                            mfem::FaceElementTransformations &Trans,
                            mfem::DenseMatrix &elmat);
};

// Interior-face div-jump ghost penalty for H(div) / RT spaces.
//
// Adds the symmetric ghost penalty:
//
//   g_h(u,v) = (gamma * factor * h_F) * sum_{F interior} int_F [[div u]] [[div v]] dF
//
// where [[div u]] = div(u^+) - div(u^-) is the divergence jump across the face.
// For RT elements div(u) is discontinuous at faces (it lives in L2); these
// jumps feed into the pressure coupling and can seed spurious oscillations.
//
// Scaling is h_F (not 1/h_F): the ghost-penalty scaling.  Dimensionally,
// [[div u]] ~ u/h, so (h_F) * [[div u]]^2 * h^2 ~ u^2/h matches the
// DG penalty term.  The penalty therefore vanishes as h->0 and does not
// degrade the optimal convergence rate.
//
// Usage: blf.AddInteriorFaceIntegrator(new RT_DivJumpIntegrator(gamma, nu));
class RT_DivJumpIntegrator : public mfem::BilinearFormIntegrator
{
protected:
    double gamma_, factor_;

public:
    RT_DivJumpIntegrator(double gamma, double factor = 1.)
        : gamma_(gamma), factor_(factor) {}

    virtual void AssembleElementMatrix(const mfem::FiniteElement &el,
                                       mfem::ElementTransformation &Trans,
                                       mfem::DenseMatrix &elmat)
    {
        MFEM_ABORT("RT_DivJumpIntegrator: only interior-face assembly is supported");
    }

    void AssembleFaceMatrix(const mfem::FiniteElement &el1,
                            const mfem::FiniteElement &el2,
                            mfem::FaceElementTransformations &Trans,
                            mfem::DenseMatrix &elmat);
};

// Boundary face integrator for a MixedBilinearForm with trial space RT
// and test space ND.  Computes the boundary term arising from integration
// by parts of the vorticity equation:
//
//   (curl u, eta) = (u, curl eta) + int_{partial Omega} (u x n) . eta dS
//
// This integrator assembles the surface integral
//
//   b_bdr(u, eta) = int_F (n x u_RT) . eta_ND dS
//
// on each boundary face F.  It must be added via
//   mixed_blf.AddBdrFaceIntegrator(new RT_ND_BdrCrossProductIntegrator());
//
// The 4-argument AssembleFaceMatrix overload is used by MixedBilinearForm;
// the 2-argument version is not supported.
class RT_ND_BdrCrossProductIntegrator : public mfem::BilinearFormIntegrator
{
public:
    RT_ND_BdrCrossProductIntegrator() {}

    virtual void AssembleElementMatrix(const mfem::FiniteElement &el,
                                       mfem::ElementTransformation &Trans,
                                       mfem::DenseMatrix &elmat)
    {
        MFEM_ABORT("RT_ND_BdrCrossProductIntegrator: element assembly not supported");
    }

    void AssembleFaceMatrix(const mfem::FiniteElement &el1,
                            const mfem::FiniteElement &el2,
                            mfem::FaceElementTransformations &Trans,
                            mfem::DenseMatrix &elmat)
    {
        MFEM_ABORT("RT_ND_BdrCrossProductIntegrator: 2-arg face assembly not supported; "
                    "use MixedBilinearForm::AddBdrFaceIntegrator");
    }

    void AssembleFaceMatrix(const mfem::FiniteElement &trial_fe1,
                            const mfem::FiniteElement &test_fe1,
                            const mfem::FiniteElement &trial_fe2,
                            const mfem::FiniteElement &test_fe2,
                            mfem::FaceElementTransformations &Trans,
                            mfem::DenseMatrix &elmat);
};

// Mixed boundary face integrator for the normal-flux pairing
//
//   m_bdr(u, q) = int_F (u . n) q dF      over boundary faces F
//
// trial_fe1 is the H(curl)/ND velocity (vector proxy u); test_fe1 is the H1
// scalar (pressure proxy q).  The output elmat has size (test_dof x trial_dof),
// so on a MixedBilinearForm(trial = ND, test = H1) the assembled matrix maps a
// velocity to (<u.n, q_j>)_j.  Used to build the consistent Nitsche outflow
// coupling (its transpose supplies the symmetric <p, v.n> momentum term).
//
// Usage:
//   mixed_blf.AddBdrFaceIntegrator(new ND_NormalScalarBdrIntegrator(), marker);
class ND_NormalScalarBdrIntegrator : public mfem::BilinearFormIntegrator
{
public:
    ND_NormalScalarBdrIntegrator() {}

    virtual void AssembleElementMatrix(const mfem::FiniteElement &el,
                                       mfem::ElementTransformation &Trans,
                                       mfem::DenseMatrix &elmat)
    {
        MFEM_ABORT("ND_NormalScalarBdrIntegrator: element assembly not supported");
    }

    void AssembleFaceMatrix(const mfem::FiniteElement &el1,
                            const mfem::FiniteElement &el2,
                            mfem::FaceElementTransformations &Trans,
                            mfem::DenseMatrix &elmat)
    {
        MFEM_ABORT("ND_NormalScalarBdrIntegrator: 2-arg face assembly not supported; "
                   "use MixedBilinearForm::AddBdrFaceIntegrator");
    }

    void AssembleFaceMatrix(const mfem::FiniteElement &trial_fe1,
                            const mfem::FiniteElement &test_fe1,
                            const mfem::FiniteElement &trial_fe2,
                            const mfem::FiniteElement &test_fe2,
                            mfem::FaceElementTransformations &Trans,
                            mfem::DenseMatrix &elmat);
};

// Boundary face penalty for H(div) / RT spaces.
//
// Adds the Nitsche-style tangential penalty on boundary faces:
//
//   (Cw / h_F) * int_{partial Omega} (n x u) . (n x v) dS
//
// This penalises the tangential component of u on the boundary.
// For RT elements the normal component is strongly enforced via essential
// BCs, so this term controls the tangential trace.
//
// Usage: blf.AddBdrFaceIntegrator(new RT_BdrTangentPenaltyIntegrator(Cw));
class RT_BdrTangentPenaltyIntegrator : public mfem::BilinearFormIntegrator
{
protected:
    double Cw_;

public:
    RT_BdrTangentPenaltyIntegrator(double Cw) : Cw_(Cw) {}

    virtual void AssembleElementMatrix(const mfem::FiniteElement &el,
                                       mfem::ElementTransformation &Trans,
                                       mfem::DenseMatrix &elmat)
    {
        MFEM_ABORT("RT_BdrTangentPenaltyIntegrator: only boundary-face assembly is supported");
    }

    void AssembleFaceMatrix(const mfem::FiniteElement &el1,
                            const mfem::FiniteElement &el2,
                            mfem::FaceElementTransformations &Trans,
                            mfem::DenseMatrix &elmat);
};

// Boundary face RHS integrator for H(div) / RT spaces (consistency term).
//
// Computes the linear form contribution:
//
//   (Cw / h_F) * int_{partial Omega} (n x u_D) . (n x v) dS
//
// where u_D is the prescribed boundary data coefficient.
// This is the RHS counterpart of RT_BdrTangentPenaltyIntegrator.
//
// Usage: lf.AddBdrFaceIntegrator(new RT_BdrTangentPenaltyLFIntegrator(Cw, u_D));
class RT_BdrTangentPenaltyLFIntegrator : public mfem::LinearFormIntegrator
{
protected:
    double Cw_;
    mfem::VectorCoefficient &uD_;

public:
    RT_BdrTangentPenaltyLFIntegrator(double Cw, mfem::VectorCoefficient &uD)
        : Cw_(Cw), uD_(uD) {}

    virtual void AssembleRHSElementVect(const mfem::FiniteElement &el,
                                        mfem::ElementTransformation &Tr,
                                        mfem::Vector &elvect);

    virtual void AssembleRHSElementVect(const mfem::FiniteElement &el,
                                        mfem::FaceElementTransformations &Tr,
                                        mfem::Vector &elvect);
};

class ND_NitscheIntegrator : public mfem::BilinearFormIntegrator
{
protected:
    double factor_, theta_, Cw_;

public:
    ND_NitscheIntegrator(double theta, double Cw, double factor = 1.) : factor_(factor), theta_(theta), Cw_(Cw){};

    virtual void AssembleElementMatrix(const mfem::FiniteElement &el,
                                       mfem::ElementTransformation &Trans,
                                       mfem::DenseMatrix &elmat);

    void AssembleFaceMatrix(const mfem::FiniteElement &el1, 
                            const mfem::FiniteElement &el2,
                            mfem::FaceElementTransformations &Trans, 
                            mfem::DenseMatrix &elmat);

};

class ND_NitscheLFIntegrator : public mfem::LinearFormIntegrator
{
protected:
   mfem::VectorCoefficient &Q;
   double factor_, theta_, Cw_;
public:
   /** @brief Constructs a boundary integrator with a given Coefficient @a QG.
       Integration order will be @a a * basis_order + @a b. */
   ND_NitscheLFIntegrator(double theta, double Cw, mfem::VectorCoefficient &QG, double factor = 1.)
      : factor_(factor), theta_(theta), Cw_(Cw), Q(QG) { }
      
   /** Given a particular boundary Finite Element and a transformation (Tr)
       computes the element boundary vector, elvect. */
   virtual void AssembleRHSElementVect(const mfem::FiniteElement &el,
                                       mfem::ElementTransformation &Tr,
                                       mfem::Vector &elvect);
   virtual void AssembleRHSElementVect(const mfem::FiniteElement &el,
                                       mfem::FaceElementTransformations &Tr,
                                       mfem::Vector &elvect);
};

#endif
