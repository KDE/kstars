/*
    SPDX-FileCopyrightText: 2022 Sophie Taylor <sophie@spacekitteh.moe>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The purpose of Robust Statistics is to provide a more robust way of calculating typical statistical measures
// such as averages. More information is available here: https://en.wikipedia.org/wiki/Robust_statistics
// The basic idea is that traditional measures of "location" such as the mean are susceptible to outliers in the
// distribution. More information on location is available here: https://en.wikipedia.org/wiki/Location_parameter
// The same applies to measures of scale such as variance (more information available here:
// https://en.wikipedia.org/wiki/Scale_parameter)
//
// Robust Statistics uses underlying GNU Science Library (GSL) routines and provides a wrapper around these routines.
// See the Statistics section of the GSL documentation: https://www.gnu.org/software/gsl/doc/html/statistics.html
//
// Currently the following are provided:
// Location: LOCATION_MEAN          - calculate the mean of input data
//           LOCATION_MEDIAN        - calculate the median
//           LOCATION_TRIMMEDMEAN   - discard a specified fraction of high/low values before calculating the mean
//           LOCATION_GASTWIRTH     - use the Gastwirth estimator based on combining different quantiles
//                                    see https://www.gnu.org/software/gsl/doc/html/statistics.html#gastwirth-estimator
//           LOCATION_SIGMACLIPPING - single step sigma clipping routine to sigma clip outliers fron the
//                                    input data and calculate the mean from the remaining data
//
// Scale:    SCALE_VARIANCE         - variance
//           SCALE_MAD              - median absolute deviation is the median of absolute differences between each
//                                    datapoint and the median.
//           SCALE_BWMV             - biweight midvariance is a more complex calculate detailed here
//                                    https://en.wikipedia.org/wiki/Robust_measures_of_scale#The_biweight_midvariance
//                                    implemenated internally - not provided by GSL
//           SCALE_SESTIMATOR       - Qn and Sn estimators are pairwise alternative to MAD by Rousseeuw and Croux
//           SCALE_QESTIMATOR         https://www.researchgate.net/publication/221996720_Alternatives_to_Median_Absolute_Deviation
//           SCALE_PESTIMATOR       - Pairwise mean scale estimator (Pn). This is the interquartile range of the
//                                    pairwise means. Implemented internally, not provided by GSL.
//
// Where necessary data is sorted by the routines and functionality to use a user selected array sride is included.
// C++ Templates are used to provide access to the GSL routines based on the datatype of the input data.

#pragma once

#include <limits>
#include <vector>
#include <cmath>
#include <QObject>
#include <QVector>

#include "gslhelpers.h"

namespace Mathematics::RobustStatistics
{

template<typename Base>
double Pn_from_sorted_data(const Base sorted_data[],
                           const size_t stride,
                           const size_t n,
                           Base work[],
                           int work_int[]);

enum ScaleCalculation { SCALE_VARIANCE, SCALE_BWMV, SCALE_SESTIMATOR, SCALE_QESTIMATOR, SCALE_MAD, SCALE_PESTIMATOR };


enum LocationCalculation { LOCATION_MEAN, LOCATION_MEDIAN, LOCATION_TRIMMEDMEAN, LOCATION_GASTWIRTH,
                           LOCATION_SIGMACLIPPING
                         };

struct SampleStatistics
{
    constexpr SampleStatistics() : location(0), scale(0), weight(std::numeric_limits<double>::infinity()) {}
    constexpr SampleStatistics(const double location, const double scale, const double weight) :
        location(location), scale(scale), weight(weight) {}
    double location;
    double scale;
    double weight;
};

//template<typename Base=double>
//struct Estimator
//{
//    virtual double Estimate(std::vector<Base> data, const size_t stride = 1)
//    {
//        std::sort(Mathematics::GSLHelpers::make_strided_iter(data.begin(), stride),
//                  Mathematics::GSLHelpers::make_strided_iter(data.end(), stride));
//        return EstimateFromSortedData(data, stride);
//    }
//    virtual double Estimate(const std::vector<Base> &data, const size_t stride = 1)
//    {
//        auto sorted = data;
//        std::sort(Mathematics::GSLHelpers::make_strided_iter(sorted.begin(), stride),
//                  Mathematics::GSLHelpers::make_strided_iter(sorted.end(), stride));
//        return EstimateFromSortedData(sorted, stride);
//    }

//    virtual double EstimateFromSortedData(const std::vector<Base> &data, const size_t stride = 1) = 0;
//};

template<typename Base> struct LocationEstimator;

template<typename Base = double>
struct ScaleEstimator //: public virtual Estimator<Base>
{
    LocationEstimator<Base> locationEstimator;
    virtual double EstimateScale(std::vector<Base> data, const size_t stride = 1)
    {
        std::sort(Mathematics::GSLHelpers::make_strided_iter(data.begin(), stride),
                  Mathematics::GSLHelpers::make_strided_iter(data.end(), stride));
        return EstimateScaleFromSortedData(data, stride);
    }
    virtual double EstimateScale(const std::vector<Base> &data, const size_t stride = 1)
    {
        auto sorted = data;
        std::sort(Mathematics::GSLHelpers::make_strided_iter(sorted.begin(), stride),
                  Mathematics::GSLHelpers::make_strided_iter(sorted.end(), stride));
        return EstimateScaleFromSortedData(sorted, stride);
    }

    virtual double ConvertScaleToWeight(const double scale)
    {
        return 1 / (scale * scale);
    }

    virtual double EstimateScaleFromSortedData(const std::vector<Base> &data, const size_t stride = 1) = 0;
};

template <typename Base = double>
struct MAD : public virtual ScaleEstimator<Base>
{
    virtual double EstimateScale(const std::vector<Base> &data, const size_t stride = 1) override
    {
        auto const size = data.size();
        auto work = std::make_unique<double[]>(size / stride);
        return Mathematics::GSLHelpers::gslMAD(data.data(), stride, size, work.get());
    }
    virtual double EstimateScale(std::vector<Base> data, const size_t stride = 1) override
    {
        return EstimateScale(data, stride);
    }
    virtual double EstimateScaleFromSortedData(const std::vector<Base> &data, const size_t stride = 1) override
    {
        return EstimateScale(data, stride);
    }
};
template <typename Base = double>
struct Variance : public virtual ScaleEstimator<Base>
{
    virtual double EstimateScaleFromSortedData(const std::vector<Base> &data, const size_t stride = 1) override
    {
        return Mathematics::GSLHelpers::gslVariance(data.data(), stride, data.size());
    }
    virtual double ConvertScaleToWeight(const double variance) override
    {
        return 1 / variance;
    }
};

template <typename Base = double>
struct SnEstimator : public virtual ScaleEstimator<Base>
{
    virtual double EstimateScaleFromSortedData(const std::vector<Base> &data, const size_t stride = 1) override
    {
        auto size = data.size();
        auto work = std::make_unique<Base[]>(size);
        return Mathematics::GSLHelpers::gslSnFromSortedData(data.data(), stride, size, work.get());
    }
};

template <typename Base = double>
struct QnEstimator : public virtual ScaleEstimator<Base>
{
    virtual double EstimateScaleFromSortedData(const std::vector<Base> &data, const size_t stride = 1) override
    {
        auto size = data.size();
        auto work = std::make_unique<Base[]>(3 * size);
        auto intWork = std::make_unique<Base[]>(5 * size);
        return Mathematics::GSLHelpers::gslQnFromSortedData(data.data(), stride, size, work.get(), intWork.get());
    }
};

template <typename Base = double>
struct PnEstimator : public virtual ScaleEstimator<Base>
{
    virtual double EstimateScaleFromSortedData(const std::vector<Base> &data, const size_t stride = 1) override
    {
        auto size = data.size();
        auto work = std::make_unique<Base[]>(3 * size);
        auto intWork = std::make_unique<Base[]>(5 * size);
        return Pn_from_sorted_data(data.data(), stride, size, work.get(), intWork.get());
    }
};


template <typename Base = double>
struct BiweightMidvariance : public virtual ScaleEstimator<Base>
{
    virtual double EstimateScaleFromSortedData(const std::vector<Base> &data, const size_t stride = 1) override
    {
        auto const size = data.size();
        auto const theData = data.data();
        auto work = std::make_unique<double[]>(size / stride);
        auto const adjustedMad = 9.0 * Mathematics::GSLHelpers::gslMAD(theData, stride, size, work.get());
        auto const median = this->locationEstimator.EstimateLocationFromSortedData(theData,
                            stride); //gslMedianFromSortedData(theData, stride, size);

        auto const begin = Mathematics::GSLHelpers::make_strided_iter(data.cbegin(), stride);
        auto const end = Mathematics::GSLHelpers::make_strided_iter(data.cend(), stride);

        auto ys = std::vector<Base>(size / stride);
        std::transform(begin, end, ys.begin(), [ = ](Base x)
        {
            return (x - median) / adjustedMad;
        });
        auto const indicator = [](Base y)
        {
            return std::fabs(y) < 1 ? 1 : 0;
        };
        auto const top = std::transform_reduce(begin, end, ys.begin(), 0, std::plus{},
                                               [ = ](Base x, Base y)
        {
            return indicator(y) * pow(x - median, 2) * pow(1 - pow(y, 2), 4);
        });
        auto const bottomSum = std::transform_reduce(begin, end, 0, std::plus{},
                               [ = ](Base y)
        {
            return indicator(y) * (1.0 - pow(y, 2)) * (1.0 - 5.0 * pow(y, 2));
        });
        // The -1 is for Bessel's correction.
        auto const bottom = bottomSum * (bottomSum - 1);

        return (size / stride) * (top / bottom);
    }

    virtual double ConvertScaleToWeight(const double variance) override
    {
        return 1 / variance;
    }
};

template<typename Base = double>
struct LocationEstimator //: public virtual Estimator<Base>
{
    ScaleEstimator<Base> scaleEstimator;
    virtual double EstimateLocation(std::vector<Base> data, const size_t stride = 1)
    {
        std::sort(Mathematics::GSLHelpers::make_strided_iter(data.begin(), stride),
                  Mathematics::GSLHelpers::make_strided_iter(data.end(), stride));
        return EstimateLocationFromSortedData(data, stride);
    }
    virtual double EstimateLocation(const std::vector<Base> &data, const size_t stride = 1)
    {
        auto sorted = data;
        std::sort(Mathematics::GSLHelpers::make_strided_iter(sorted.begin(), stride),
                  Mathematics::GSLHelpers::make_strided_iter(sorted.end(), stride));
        return EstimateLocationFromSortedData(sorted, stride);
    }
    virtual double EstimateLocationFromSortedData(const std::vector<Base> &data, const size_t stride = 1) = 0;
};

template<typename Base = double>
struct StatisticalCoEstimator : public virtual LocationEstimator<Base>, public virtual ScaleEstimator<Base>
{
    private:
        LocationEstimator<Base> locationEstimator;
        ScaleEstimator<Base> scaleEstimator;
    public:
        StatisticalCoEstimator(LocationEstimator<Base> locationEstimator, ScaleEstimator<Base> scaleEstimator)
            : locationEstimator(locationEstimator), scaleEstimator(scaleEstimator) {}
        virtual double EstimateLocationFromSortedData(const std::vector<Base> &data, const size_t stride = 1) override
        {
            auto const stats = EstimateStatisticsFromSortedData(data, stride);
            return stats.location;
        }
        virtual double EstimateScaleFromSortedData(const std::vector<Base> &data, const size_t stride = 1) override
        {
            auto const stats = EstimateStatisticsFromSortedData(data, stride);
            return stats.scale;
        }
        virtual SampleStatistics EstimateStatisticsFromSortedData(const std::vector<Base> &data, const size_t stride = 1)
        {
            auto const location = EstimateLocationFromSortedData(data, stride);
            auto const scale = EstimateScaleFromSortedData(data, stride);
            auto const weight = ConvertScaleToWeight(scale);

            return SampleStatistics{location, scale, weight};
        }
};

/**
 * @short Computes a estimate of the statistical scale of the input sample.
 *
 * @param scaleMethod The estimator to use.
 * @param data The sample to estimate the scale of.
 * @param stride The stide of the data.
 */
template<typename Base = double>
double ComputeScale(const ScaleCalculation scaleMethod, std::vector<Base> data,
                    const size_t stride = 1)
{
    if (scaleMethod != SCALE_VARIANCE)
        std::sort(data.begin(), data.end());
    return ComputeScaleFromSortedData(scaleMethod, data, stride);
}
//[[using gnu : pure]]
template<typename Base = double>
double ComputeScaleFromSortedData(const ScaleCalculation scaleMethod,
                                  const std::vector<Base> &data,
                                  const size_t stride = 1);
/**
 * @short Computes a estimate of the statistical location of the input sample.
 *
 * @param scaleMethod The estimator to use.
 * @param data The sample to estimate the location of.
 * @param sortedData The sorted data.
 * @param trimAmount When using the trimmed mean estimate, the percentage quartile to remove either side.
 *                   When using sigma clipping, the number of standard deviations a sample has to be from the mean
 *                   to be considered an outlier.
 * @param stride The stide of the data. Note that in cases other than the simple mean, the stride will only apply
 *               AFTER sorting.
 */
template<typename Base = double>
double ComputeLocation(const LocationCalculation locationMethod, std::vector<Base> data,
                       const double trimAmount = 0.25, const size_t stride = 1)
{
    if (locationMethod != LOCATION_MEAN)
        std::sort(data.begin(), data.end());
    return ComputeLocationFromSortedData(locationMethod, data, trimAmount, stride);
}

//[[using gnu : pure]]
template<typename Base = double>
double ComputeLocationFromSortedData(const LocationCalculation locationMethod,
                                     const std::vector<Base> &data, const double trimAmount = 0.25, const size_t stride = 1);

/**
 * @short Computes a weight for use in regression.
 *
 * @param scaleMethod The estimator to use.
 * @param data The sample to estimate the scale of.
 * @param sortedData The sorted data.
 * @param stride The stide of the data. Note that in cases other than the simple variance, the stride will only
 *               apply AFTER sorting.
 */
template<typename Base = double>
double ComputeWeight(const ScaleCalculation scaleMethod, std::vector<Base> data, const size_t stride = 1)
{
    if (scaleMethod != SCALE_VARIANCE)
        std::sort(data.begin(), data.end());
    return ComputeWeightFromSortedData(scaleMethod, data, stride);
}
//[[using gnu : pure]]
template<typename Base = double>
double ComputeWeightFromSortedData(const ScaleCalculation scaleMethod, const std::vector<Base> &data,
                                   const size_t stride = 1)
{
    auto const scale = ComputeScaleFromSortedData(scaleMethod, data, stride);
    return ConvertScaleToWeight(scaleMethod, scale);
}

SampleStatistics ComputeSampleStatistics(std::vector<double> data,
        const LocationCalculation locationMethod = LOCATION_TRIMMEDMEAN,
        const ScaleCalculation scaleMethod = SCALE_QESTIMATOR,
        double trimAmount = 0.25,
        const size_t stride = 1);

[[using gnu : pure]]
constexpr double ConvertScaleToWeight(const ScaleCalculation scaleMethod, double scale)
{
    // If the passed in scale is zero or near zero return a very small weight rather than infinity.
    // This fixes the situation where, e.g. there is a single datapoint so scale is zero and
    // weighting this datapoint with infinite weight is incorrect (and breaks the LM solver)
    switch (scaleMethod)
    {
        // Variance, biweight midvariance are variances.
        case SCALE_VARIANCE:
            [[fallthrough]];
        case SCALE_BWMV:
            return (scale < 1e-10) ? 1e-10 : 1 / scale;
        // MAD, Sn, Qn and Pn estimators are all calibrated to estimate the standard deviation of Gaussians.
        case SCALE_MAD:
            [[fallthrough]];
        case SCALE_SESTIMATOR:
            [[fallthrough]];
        case SCALE_QESTIMATOR:
            [[fallthrough]];
        case SCALE_PESTIMATOR:
            return (scale < 1e-10) ? 1e-10 : 1 / (scale * scale);
    }
    return -1;
}


} // namespace Mathematics
