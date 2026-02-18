#include <gtest/gtest.h>
#include <mfem.hpp>
#include "SpectraErrorOp.hpp"
#include <vector>
#include <complex>
#include <algorithm>
#include <cmath>
#include <Eigen/Dense>

#define SPECTRA_TOL 1e-8

// Robust comparator for complex numbers to ensure stable sorting
// Primary key: Real part (Descending)
// Secondary key: Imaginary part (Descending)
bool ComplexCompare(const std::complex<double>& a, const std::complex<double>& b) {
    if (std::abs(a.real() - b.real()) > 1e-12)
        return a.real() > b.real();
    return a.imag() > b.imag();
}

// Helper to filter top-k by magnitude, then sort by value for direct comparison
void PrepareEigenvalues(const Eigen::VectorXcd& input,
                        std::vector<std::complex<double>>& output,
                        const int k)
{
    std::vector<std::complex<double>> temp;
    for(int i = 0; i < input.size(); ++i)
      temp.push_back(input(i));

    std::sort(temp.begin(), temp.end(), [](const std::complex<double>& a, const std::complex<double>& b) {
        return std::abs(a) > std::abs(b);
    });

    if (temp.size() > k)
      temp.resize(k);

    std::sort(temp.begin(), temp.end(), ComplexCompare);

    output = temp;
}

// Tests the solver against a diagonal system where eigenvalues are analytically known.
// Constructs a diagonal matrix A and sets P = I.
// The error operator is E = I - A, so eigenvalues should be exactly (1.0 - A_ii).
TEST(SpectraTest, DiagonalSystem)
{
    const int n = 50;
    const int numEigs = 5;

    mfem::SparseMatrix A(n, n);
    Eigen::VectorXcd expected_vec(n);

    for (int i = 0; i < n; i++)
    {
        double val = 0.5 + 0.4 * ((double)i / (n - 1));
        A.Set(i, i, val);
        expected_vec(i) = std::complex<double>(1.0 - val, 0.0);
    }
    A.Finalize();

    mfem::IdentityOperator P(n);

    // Compute Spectra results passing the tolerance
    Eigen::VectorXcd spectra_res =
      computeErrorOperatorEigenvalues(A, P, numEigs, SPECTRA_TOL, false);

    // Prepare both sets for comparison
    std::vector<std::complex<double>> expected_sorted;
    std::vector<std::complex<double>> actual_sorted;

    PrepareEigenvalues(expected_vec, expected_sorted, numEigs);
    PrepareEigenvalues(spectra_res, actual_sorted, numEigs);

    ASSERT_EQ(actual_sorted.size(), numEigs);

    for (int i = 0; i < numEigs; i++)
    {
        EXPECT_NEAR(actual_sorted[i].real(), expected_sorted[i].real(), SPECTRA_TOL);
        EXPECT_NEAR(actual_sorted[i].imag(), expected_sorted[i].imag(), SPECTRA_TOL);
    }
}

// Tests the solver against a dense random system.
// Uses Eigen::EigenSolver to compute the ground truth eigenvalues of (I - A).
// Verifies that Spectra's top-k eigenvalues match the Eigen results exactly.
TEST(SpectraTest, DenseRandomSystem)
{
    const int n = 20;
    const int numEigs = 6;

    mfem::DenseMatrix A(n);
    Eigen::MatrixXd E_mat(n, n);

    srand(42);

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            double val = ((double)rand() / RAND_MAX) * 0.5;
            A(i, j) = val;
            E_mat(i, j) = (i == j ? 1.0 : 0.0) - val;
        }
    }

    // Ground Truth: Compute all eigenvalues using Eigen
    Eigen::EigenSolver<Eigen::MatrixXd> es(E_mat);
    Eigen::VectorXcd all_evals = es.eigenvalues();

    mfem::IdentityOperator P(n);

    // Test Subject: Compute top k using Spectra, passing the tolerance
    Eigen::VectorXcd spectra_res =
      computeErrorOperatorEigenvalues(A, P, numEigs, SPECTRA_TOL, false);

    // Prepare for comparison
    std::vector<std::complex<double>> expected_sorted;
    std::vector<std::complex<double>> actual_sorted;

    PrepareEigenvalues(all_evals, expected_sorted, numEigs);
    PrepareEigenvalues(spectra_res, actual_sorted, numEigs);

    ASSERT_EQ(actual_sorted.size(), numEigs);

    for (int i = 0; i < numEigs; i++)
    {
        EXPECT_NEAR(actual_sorted[i].real(), expected_sorted[i].real(), SPECTRA_TOL)
            << "Real part mismatch at index " << i;
        EXPECT_NEAR(actual_sorted[i].imag(), expected_sorted[i].imag(), SPECTRA_TOL)
            << "Imag part mismatch at index " << i;
    }
}
