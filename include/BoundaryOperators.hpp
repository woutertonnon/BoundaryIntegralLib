#ifndef SEMILAGRANGE0FORMS_BOUNDARYOPERATORS_H
#define SEMILAGRANGE0FORMS_BOUNDARYOPERATORS_H

#include <mfem.hpp>

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
     *       Integration order will be @a a * basis_order + @a b. */
    ND_NitscheLFIntegrator(double theta, double Cw, mfem::VectorCoefficient &QG, double factor = 1.)
    : factor_(factor), theta_(theta), Cw_(Cw), Q(QG) { }

    /** Given a particular boundary Finite Element and a transformation (Tr)
     *       computes the element boundary vector, elvect. */
    virtual void AssembleRHSElementVect(const mfem::FiniteElement &el,
                                        mfem::ElementTransformation &Tr,
                                        mfem::Vector &elvect);
    virtual void AssembleRHSElementVect(const mfem::FiniteElement &el,
                                        mfem::FaceElementTransformations &Tr,
                                        mfem::Vector &elvect);
};

#endif
