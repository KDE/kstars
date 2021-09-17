/*
    SPDX-FileCopyrightText: 2014-2017 Max Planck Society.
    All rights reserved.

    SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * @file
 * @date      2014-2017
 * @copyright Max Planck Society
 *
 * @author    Edgar D. Klenske <edgar.klenske@tuebingen.mpg.de>
 * @author    Stephan Wenninger <stephan.wenninger@tuebingen.mpg.de>
 * @author    Raffi Enficiaud <raffi.enficiaud@tuebingen.mpg.de>
 *
 * @brief     The GP class implements the Gaussian Process functionality.
 */

#ifndef GAUSSIAN_PROCESS_H
#define GAUSSIAN_PROCESS_H

#include <Eigen/Dense>
#include <vector>
#include <list>
#include <memory>
#include <utility>
#include <cstdint>
#include <cmath>
#include "covariance_functions.h"

// Constants

// Jitter is the minimal "noise" on the otherwise noiseless kernel matrices to
// make the Cholesky decomposition stable.
#define JITTER 1e-6

class GP
{
private:
    covariance_functions::CovFunc* covFunc_;
    covariance_functions::CovFunc* covFuncProj_;
    Eigen::VectorXd data_loc_;
    Eigen::VectorXd data_out_;
    Eigen::VectorXd data_var_;
    Eigen::MatrixXd gram_matrix_;
    Eigen::VectorXd alpha_;
    Eigen::LDLT<Eigen::MatrixXd> chol_gram_matrix_;
    double log_noise_sd_;
    bool use_explicit_trend_;
    Eigen::MatrixXd feature_vectors_;
    Eigen::MatrixXd feature_matrix_;
    Eigen::LDLT<Eigen::MatrixXd> chol_feature_matrix_;
    Eigen::VectorXd beta_;

public:
    typedef std::pair<Eigen::VectorXd, Eigen::MatrixXd> VectorMatrixPair;

    GP(); // allowing the standard constructor makes the use so much easier!
    GP(const covariance_functions::CovFunc& covFunc);
    GP(const double noise_variance,
       const covariance_functions::CovFunc& covFunc);
    ~GP(); // Need to tidy up

    GP(const GP& that);
    GP& operator=(const GP& that);

    /*! Sets the covariance function
     *
     * This operation is possible only if there is not inference going on in the
     * current instance. This is useful after initialisation.
     */
    bool setCovarianceFunction(const covariance_functions::CovFunc& covFunc);

    /*!
     * Sets the output projection covariance function.
     */
    void enableOutputProjection(const covariance_functions::CovFunc& covFunc);

    /*!
     * Removes the output projection covariance function.
     */
    void disableOutputProjection();

    /*!
     * Returns a GP sample for the given locations.
     *
     * Returns a sample of the prior if the Gram matrix is empty.
     */
    Eigen::VectorXd drawSample(const Eigen::VectorXd& locations) const;

    /*!
     * Returns a sample of the GP based on a given vector of random numbers.
     */
    Eigen::VectorXd drawSample(const Eigen::VectorXd& locations,
                               const Eigen::VectorXd& random_vector) const;

    /*!
     * Builds an inverts the Gram matrix for a given set of datapoints.
     *
     * This function works on the already stored data and doesn't return
     * anything. The work is done here, I/O somewhere else.
     */
    void infer();

    /*!
     * Stores the given datapoints in the form of data location \a data_loc,
     * the output values \a data_out and noise vector \a data_sig.
     * Calls infer() everytime so that the Gram matrix is rebuild and the
     * Cholesky decomposition is computed.
     */
    void infer(const Eigen::VectorXd& data_loc,
               const Eigen::VectorXd& data_out,
               const Eigen::VectorXd& data_var = Eigen::VectorXd());

    /*!
     * Calculates the GP based on a subset of data (SD) approximation. The data
     * vector for the GP consists of a subset of n most important data points,
     * where the importance is defined as covariance to the prediction point. If
     * no prediction point is given, the last data point is used (extrapolation
     * mode).
     */
    void inferSD(const Eigen::VectorXd& data_loc,
                 const Eigen::VectorXd& data_out,
                 const int n,
                 const Eigen::VectorXd& data_var = Eigen::VectorXd(),
                 const double prediction_point = std::numeric_limits<double>::quiet_NaN());

    /*!
     * Sets the GP back to the prior:
     * Removes datapoints, empties the Gram matrix.
     */
    void clearData();

    /*!
     * Predicts the mean and covariance for a vector of locations.
     *
     * This function just builds the prior and mixed covariance matrices and
     * calls the other predict afterwards.
     */
    Eigen::VectorXd predict(const Eigen::VectorXd& locations, Eigen::VectorXd* variances = nullptr) const;

    /*!
     * Predicts the mean and covariance for a vector of locations based on
     * the output projection.
     *
     * This function just builds the prior and mixed covariance matrices and
     * calls the other predict afterwards.
     */
    Eigen::VectorXd predictProjected(const Eigen::VectorXd& locations, Eigen::VectorXd* variances = nullptr) const;

    /*!
     * Does the real work for predict. Solves the Cholesky decomposition for the
     * given matrices. The Gram matrix and measurements need to be cached
     * already.
     */
    Eigen::VectorXd predict(const Eigen::MatrixXd& prior_cov, const Eigen::MatrixXd& mixed_cov,
                             const Eigen::MatrixXd& phi = Eigen::MatrixXd() , Eigen::VectorXd* variances = nullptr) const;

    /*!
     * Sets the hyperparameters to the given vector.
     */
    void setHyperParameters(const Eigen::VectorXd& hyperParameters);

    /*!
     * Returns the hyperparameters to the given vector.
     */
    Eigen::VectorXd getHyperParameters() const;

    /*!
     * Enables the use of a explicit linear basis function.
     */
    void enableExplicitTrend();

    /*!
     * Disables the use of a explicit linear basis function.
     */
    void disableExplicitTrend();


};

#endif  // ifndef GAUSSIAN_PROCESS_H
