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
 * @brief     The file holds the covariance functions that can be used with the GP class.
 */

#ifndef COVARIANCE_FUNCTIONS_H
#define COVARIANCE_FUNCTIONS_H

#include <Eigen/Dense>
#include <vector>
#include <list>
#include <utility>
#include <cstdint>

// HY: Added all the "override" qualifiers below to silence the compiler.

namespace covariance_functions
{
    /*!@brief Base class definition for covariance functions
     */
    class CovFunc
    {
    public:
        CovFunc() {}
        virtual ~CovFunc() {}

        /*!
         * Evaluates the covariance function, caches the quantities that are needed
         * to calculate gradient and Hessian.
         */
        virtual Eigen::MatrixXd evaluate(const Eigen::VectorXd& x1, const Eigen::VectorXd& x2) = 0;

        //! Method to set the hyper-parameters.
        virtual void setParameters(const Eigen::VectorXd& params) = 0;
        virtual void setExtraParameters(const Eigen::VectorXd& params) = 0;

        //! Returns the hyper-parameters.
        virtual const Eigen::VectorXd& getParameters() const = 0;
        virtual const Eigen::VectorXd& getExtraParameters() const = 0;

        //! Returns the number of hyper-parameters.
        virtual int getParameterCount() const = 0;
        virtual int getExtraParameterCount() const = 0;

        //! Produces a clone to be able to copy the object.
        virtual CovFunc* clone() const = 0;
    };

    /*!
     * The function computes a combined covariance function. It is a periodic
     * covariance function with an additional square exponential. This
     * combination makes it possible to learn a signal that consists of both
     * periodic and aperiodic parts.
     *
     * Square Exponential Component:
     * @f[
     * k _{\textsc{se}}(t,t';\theta_\textsc{se},\ell_\textsc{se}) =
     * \theta_\textsc{se} \cdot
     * \exp\left(-\frac{(t-t')^2}{2\ell_\textsc{se}^{2}}\right)
     * @f]
     *
     * Periodic Component:
     * @f[
     * k_\textsc{p}(t,t';\theta_\textsc{p},\ell_\textsc{p},\lambda) =
     * \theta_\textsc{p} \cdot
     * \exp\left(-\frac{2\sin^2\left(\frac{\pi}{\lambda}
     * (t-t')\right)}{\ell_\textsc{p}^2}\right)
     * @f]
     *
     * Kernel Combination:
     * @f[
     * k _\textsc{c}(t,t';\theta_\textsc{se},\ell_\textsc{se},\theta_\textsc{p},
     * \ell_\textsc{p},\lambda) =
     * k_{\textsc{se}}(t,t';\theta_\textsc{se},\ell_\textsc{se})
     * +
     * k_\textsc{p}(t,t';\theta_\textsc{p},\ell_\textsc{p},\lambda)
     * @f]
     */
     class PeriodicSquareExponential : public CovFunc
     {
     private:
         Eigen::VectorXd hyperParameters;
         Eigen::VectorXd extraParameters;

     public:
         PeriodicSquareExponential();
         explicit PeriodicSquareExponential(const Eigen::VectorXd& hyperParameters);

         /*!
          * Evaluates the covariance function, caches the quantities that are needed
          * to calculate gradient and Hessian.
          */
         Eigen::MatrixXd evaluate(const Eigen::VectorXd& x1, const Eigen::VectorXd& x2) override;

         //! Method to set the hyper-parameters.
         void setParameters(const Eigen::VectorXd& params) override;
         void setExtraParameters(const Eigen::VectorXd& params) override;

         //! Returns the hyper-parameters.
         const Eigen::VectorXd& getParameters() const override;
         const Eigen::VectorXd& getExtraParameters() const override;

         //! Returns the number of hyper-parameters.
         int getParameterCount() const override;
         int getExtraParameterCount() const override;

         /**
          * Produces a clone to be able to copy the object.
          */
         virtual CovFunc* clone() const override
         {
             return new PeriodicSquareExponential(*this);
         }
     };


     /*!
      * The function computes a combined covariance function. It is a periodic
      * covariance function with two additional square exponential components.
      * This combination makes it possible to learn a signal that consists of
      * periodic parts, long-range aperiodic parts and small-range deformations.
      *
      * Square Exponential Component:
      * @f[
      * k _{\textsc{se}}(t,t';\theta_\textsc{se},\ell_\textsc{se}) =
      * \theta_\textsc{se} \cdot
      * \exp\left(-\frac{(t-t')^2}{2\ell_\textsc{se}^{2}}\right)
      * @f]
      *
      * Periodic Component:
      * @f[
      * k_\textsc{p}(t,t';\theta_\textsc{p},\ell_\textsc{p},\lambda) =
      * \theta_\textsc{p} \cdot
      * \exp\left(-\frac{2\sin^2\left(\frac{\pi}{\lambda}
      * (t-t')\right)}{\ell_\textsc{p}^2}\right)
      * @f]
      *
      * Kernel Combination:
      * @f[
      * k _\textsc{c}(t,t';\theta_\textsc{se},\ell_\textsc{se},\theta_\textsc{p},
      * \ell_\textsc{p},\lambda) =
      * k_{\textsc{se},1}(t,t';\theta_{\textsc{se},1},\ell_{\textsc{se},1})
      * +
      * k_\textsc{p}(t,t';\theta_\textsc{p},\ell_\textsc{p},\lambda)
      * +
      * k_{\textsc{se},2}(t,t';\theta_{\textsc{se},2},\ell_{\textsc{se},2})
      * @f]
      */
    class PeriodicSquareExponential2 : public CovFunc
    {
    private:
        Eigen::VectorXd hyperParameters;
        Eigen::VectorXd extraParameters;

    public:
        PeriodicSquareExponential2();
        explicit PeriodicSquareExponential2(const Eigen::VectorXd& hyperParameters);

        Eigen::MatrixXd evaluate(const Eigen::VectorXd& x1, const Eigen::VectorXd& x2) override;

        //! Method to set the hyper-parameters.
        void setParameters(const Eigen::VectorXd& params) override;
        void setExtraParameters(const Eigen::VectorXd& params) override;

        //! Returns the hyper-parameters.
        const Eigen::VectorXd& getParameters() const override;
        const Eigen::VectorXd& getExtraParameters() const override;

        //! Returns the number of hyper-parameters.
        int getParameterCount() const override;
        int getExtraParameterCount() const override;

        /**
         * Produces a clone to be able to copy the object.
         */
        virtual CovFunc* clone() const override
        {
            return new PeriodicSquareExponential2(*this);
        }
    };
}  // namespace covariance_functions
#endif  // ifndef COVARIANCE_FUNCTIONS_H
