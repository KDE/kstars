/*
    SPDX-FileCopyrightText: 2022 Sophie Taylor <sophie@spacekitteh.moe>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "robuststatistics.h"
#include <cmath>

namespace Mathematics::RobustStatistics
{
using namespace Mathematics::GSLHelpers;

// int64 code is currently deactiovated by GSLHELPERS_INT64. It doesn't compile on Mac becuase it
// won't cast a long long * to a long * even though they are both 64bit pointers.
// On 32bit systems this would be an issue because they are not the same.
// If the int64 versions are required in future then this "problem" will need to
// be resolved.


template<typename Base = double>
double Pn_from_sorted_data(const Base sorted_data[],
                           const size_t stride,
                           const size_t n,
                           Base work[],
                           int work_int[]);

template<typename Base>
double ComputeScaleFromSortedData(const ScaleCalculation scaleMethod,
                                  const std::vector<Base> &data,
                                  const size_t stride)
{
    auto const size = data.size();
    auto const theData = data.data();
    switch (scaleMethod)
    {
        // Compute biweight midvariance
        case SCALE_BWMV:
        {
            auto work = std::make_unique<double[]>(size);
            auto const adjustedMad = 9.0 * gslMAD(theData, stride, size, work.get());
            auto const median = gslMedianFromSortedData(theData, stride, size);

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
        case SCALE_MAD:
        {
            auto work = std::make_unique<double[]>(size);
            return gslMAD(theData, stride, size, work.get());
        }
        case SCALE_SESTIMATOR:
        {
            auto work = std::make_unique<Base[]>(size);
            return gslSnFromSortedData(theData, stride, size, work.get());
        }
        case SCALE_QESTIMATOR:
        {
            auto work = std::make_unique<Base[]>(3 * size);
            auto intWork = std::make_unique<int[]>(5 * size);

            return gslQnFromSortedData(theData, stride, size, work.get(), intWork.get());
        }
        case SCALE_PESTIMATOR:
        {
            auto work = std::make_unique<Base[]>(3 * size);
            auto intWork = std::make_unique<int[]>(5 * size);

            return Pn_from_sorted_data(theData, stride, size, work.get(), intWork.get());
        }
        case SCALE_VARIANCE:
            [[fallthrough]];
        default:
            return gslVariance(theData, stride, size);
    }
}


template double ComputeScaleFromSortedData(const ScaleCalculation scaleMethod,
        const std::vector<double> &data,
        const size_t stride);
template double ComputeScaleFromSortedData(const ScaleCalculation scaleMethod,
        const std::vector<float> &data,
        const size_t stride);
template double ComputeScaleFromSortedData(const ScaleCalculation scaleMethod,
        const std::vector<uint8_t> &data,
        const size_t stride);
template double ComputeScaleFromSortedData(const ScaleCalculation scaleMethod,
        const std::vector<uint16_t> &data,
        const size_t stride);
template double ComputeScaleFromSortedData(const ScaleCalculation scaleMethod,
        const std::vector<int16_t> &data,
        const size_t stride);
template double ComputeScaleFromSortedData(const ScaleCalculation scaleMethod,
        const std::vector<uint32_t> &data,
        const size_t stride);
template double ComputeScaleFromSortedData(const ScaleCalculation scaleMethod,
        const std::vector<int32_t> &data,
        const size_t stride);
#ifdef GSLHELPERS_INT64
template double ComputeScaleFromSortedData(const ScaleCalculation scaleMethod,
        const std::vector<int64_t> &data,
        const size_t stride);
#endif

template<typename Base> double ComputeLocationFromSortedData(
    const LocationCalculation locationMethod,
    const std::vector<Base> &data,
    double trimAmount, const size_t stride)
{
    auto const size = data.size();
    auto const theData = data.data();

    switch (locationMethod)
    {
        case LOCATION_MEDIAN:
        {
            return gslMedianFromSortedData(theData, stride, size);
        }
        case LOCATION_TRIMMEDMEAN:
        {
            return gslTrimmedMeanFromSortedData(trimAmount, theData, stride, size);
        }
        case  LOCATION_GASTWIRTH:
        {
            return gslGastwirthMeanFromSortedData(theData, stride, size);
        }
        // TODO: Parameterise
        case LOCATION_SIGMACLIPPING:
        {
            auto const median = gslMedianFromSortedData(theData, stride, size);
            if (data.size() > 3)
            {
                auto const stddev = gslStandardDeviation(theData, stride, size);
                auto begin = GSLHelpers::make_strided_iter(data.cbegin(), stride);
                auto end = GSLHelpers::make_strided_iter(data.cend(), stride);

                // Remove samples over 2 standard deviations away.
                auto lower = std::lower_bound(begin, end, (median - stddev * trimAmount));
                auto upper = std::upper_bound(lower, end, (median + stddev * trimAmount));

                // New mean
                auto sum = std::reduce(lower, upper);
                const int num_remaining = std::distance(lower, upper);
                if (num_remaining > 0) return sum / num_remaining;
            }
            return median;
        }
        case LOCATION_MEAN:
            [[fallthrough]];
        default:
            return gslMean(theData, stride, size);

    }
}

template double ComputeLocationFromSortedData(const LocationCalculation scaleMethod,
        const std::vector<double> &data,
        double trimAmount, const size_t stride);
template double ComputeLocationFromSortedData(const LocationCalculation scaleMethod,
        const std::vector<float> &data,
        double trimAmount, const size_t stride);
template double ComputeLocationFromSortedData(const LocationCalculation scaleMethod,
        const std::vector<uint8_t> &data,
        double trimAmount, const size_t stride);
template double ComputeLocationFromSortedData(const LocationCalculation scaleMethod,
        const std::vector<uint16_t> &data,
        double trimAmount, const size_t stride);
template double ComputeLocationFromSortedData(const LocationCalculation scaleMethod,
        const std::vector<int16_t> &data,
        double trimAmount, const size_t stride);
template double ComputeLocationFromSortedData(const LocationCalculation scaleMethod,
        const std::vector<uint32_t> &data,
        double trimAmount, const size_t stride);
template double ComputeLocationFromSortedData(const LocationCalculation scaleMethod,
        const std::vector<int32_t> &data,
        double trimAmount, const size_t stride);
#ifdef GSLHELPERS_INT64
template double ComputeLocationFromSortedData(const LocationCalculation scaleMethod,
        const std::vector<int64_t> &data,
        double trimAmount, const size_t stride);
#endif


SampleStatistics ComputeSampleStatistics(std::vector<double> data,
        const RobustStatistics::LocationCalculation locationMethod,
        const RobustStatistics::ScaleCalculation scaleMethod,
        double trimAmount,
        const size_t stride)
{
    std::sort(data.begin(), data.end());
    double location = RobustStatistics::ComputeLocationFromSortedData(locationMethod, data, trimAmount, stride);
    double scale = RobustStatistics::ComputeScaleFromSortedData(scaleMethod, data, stride);
    double weight = RobustStatistics::ConvertScaleToWeight(scaleMethod, scale);
    return RobustStatistics::SampleStatistics{location, scale, weight};
}

/*
  Algorithm to compute the weighted high median in O(n) time.
  The whimed is defined as the smallest a[j] such that the sum
  of the weights of all a[i] <= a[j] is strictly greater than
  half of the total weight.
  Arguments:
  a: double array containing the observations
  n: number of observations
  w: array of (int/double) weights of the observations.
*/

template<typename Base = double> Base whimed(Base* a, int * w, int n, Base* a_cand, Base* a_srt, int * w_cand)
{
    int n2, i, kcand;
    /* sum of weights: `int' do overflow when  n ~>= 1e5 */
    int64_t wleft, wmid, wright, w_tot, wrest;

    Base trial;

    w_tot = 0;
    for (i = 0; i < n; ++i)
        w_tot += w[i];

    wrest = 0;

    /* REPEAT : */
    do
    {
        for (i = 0; i < n; ++i)
            a_srt[i] = a[i];

        n2 = n / 2; /* =^= n/2 +1 with 0-indexing */

        gslSort(a_srt, 1, n);

        trial = a_srt[n2];

        wleft = 0;
        wmid = 0;
        wright = 0;

        for (i = 0; i < n; ++i)
        {
            if (a[i] < trial)
                wleft += w[i];
            else if (a[i] > trial)
                wright += w[i];
            else
                wmid += w[i];
        }

        /* wleft = sum_{i; a[i]	 < trial}  w[i]
         * wmid	 = sum_{i; a[i] == trial}  w[i] at least one 'i' since trial is one a[]!
         * wright= sum_{i; a[i]	 > trial}  w[i]
         */
        kcand = 0;
        if (2 * (wrest + wleft) > w_tot)
        {
            for (i = 0; i < n; ++i)
            {
                if (a[i] < trial)
                {
                    a_cand[kcand] = a[i];
                    w_cand[kcand] = w[i];
                    ++kcand;
                }
            }
        }
        else if (2 * (wrest + wleft + wmid) <= w_tot)
        {
            for (i = 0; i < n; ++i)
            {
                if (a[i] > trial)
                {
                    a_cand[kcand] = a[i];
                    w_cand[kcand] = w[i];
                    ++kcand;
                }
            }

            wrest += wleft + wmid;
        }
        else
        {
            return trial;
        }

        n = kcand;
        for (i = 0; i < n; ++i)
        {
            a[i] = a_cand[i];
            w[i] = w_cand[i];
        }
    }
    while(1);
}


/*
gsl_stats_Qn0_from_sorted_data()
  Efficient algorithm for the scale estimator:
    Q_n0 = { |x_i - x_j|; i<j }_(k) [ = Qn without scaling ]
i.e. the k-th order statistic of the |x_i - x_j|, where:
k = (floor(n/2) + 1 choose 2)
Inputs: sorted_data - sorted array containing the observations
        stride      - stride
        n           - length of 'sorted_data'
        work        - workspace of length 3n of type BASE
        work_int    - workspace of length 5n of type int
Return: Q_n statistic (without scale/correction factor); same type as input data
*/

template<typename Base = double> double Pn0k_from_sorted_data(const Base sorted_data[],
        const double k,
        const size_t stride,
        const size_t n,
        Base work[],
        int work_int[])
{
    const int ni = (int) n;
    Base * a_srt = &work[n];
    Base * a_cand = &work[2 * n];

    int *left = &work_int[0];
    int *right = &work_int[n];
    int *p = &work_int[2 * n];
    int *q = &work_int[3 * n];
    int *weight = &work_int[4 * n];

    Base trial = (Base)0.0;
    int found = 0;

    int h, i, j, jh;

    /* following should be `long long int' : they can be of order n^2 */
    int64_t knew, nl, nr, sump, sumq;

    /* check for quick return */
    if (n < 2)
        return (Base)0.0;

    h = n / 2 + 1;

    for (i = 0; i < ni; ++i)
    {
        left[i] = ni - i + 1;
        right[i] = (i <= h) ? ni : ni - (i - h);

        /* the n - (i-h) is from the paper; original code had `n' */
    }

    nl = (int64_t)n * (n + 1) / 2;
    nr = (int64_t)n * n;
    knew = k + nl;/* = k + (n+1 \over 2) */

    /* L200: */
    while (!found && nr - nl > ni)
    {
        j = 0;
        /* Truncation to float : try to make sure that the same values are got later (guard bits !) */
        for (i = 1; i < ni; ++i)
        {
            if (left[i] <= right[i])
            {
                weight[j] = right[i] - left[i] + 1;
                jh = left[i] + weight[j] / 2;
                work[j] = (sorted_data[i * stride] + sorted_data[(ni - jh) * stride]) / 2 ;
                ++j;
            }
        }

        trial = whimed(work, weight, j, a_cand, a_srt, /*iw_cand*/ p);

        j = 0;
        for (i = ni - 1; i >= 0; --i)
        {
            while (j < ni && ((double)(sorted_data[i * stride] + sorted_data[(ni - j - 1) * stride])) / 2 < trial)
                ++j;

            p[i] = j;
        }

        j = ni + 1;
        for (i = 0; i < ni; ++i)
        {
            while ((double)(sorted_data[i * stride] + sorted_data[(ni - j + 1) * stride]) / 2 > trial)
                --j;

            q[i] = j;
        }

        sump = 0;
        sumq = 0;

        for (i = 0; i < ni; ++i)
        {
            sump += p[i];
            sumq += q[i] - 1;
        }

        if (knew <= sump)
        {
            for (i = 0; i < ni; ++i)
                right[i] = p[i];

            nr = sump;
        }
        else if (knew > sumq)
        {
            for (i = 0; i < ni; ++i)
                left[i] = q[i];

            nl = sumq;
        }
        else /* sump < knew <= sumq */
        {
            found = 1;
        }
    } /* while */

    if (found)
    {
        return trial;
    }
    else
    {
        j = 0;
        for (i = 1; i < ni; ++i)
        {
            int jj;

            for (jj = left[i]; jj <= right[i]; ++jj)
            {
                work[j] = (sorted_data[i * stride] + sorted_data[(ni - jj) * stride]) / 2;
                j++;
            }/* j will be = sum_{i=2}^n ((right[i] + left[i] + 1)_{+})/2  */
        }

        /* return pull(work, j - 1, knew - nl)	: */
        knew -= (nl + 1); /* -1: 0-indexing */

        /* sort work array */
        gslSort(work, 1, j);

        return (work[knew]);
    }
}


template<typename Base>
double Pn_from_sorted_data(const Base sorted_data[],
                           const size_t stride,
                           const size_t n,
                           Base work[],
                           int work_int[])
{
    if (n <= 2)
        return sqrt(gslVariance(sorted_data, stride, n));

    double upper = Pn0k_from_sorted_data(sorted_data, (3.0 / 4.0), stride, n, work, work_int);
    double lower = Pn0k_from_sorted_data(sorted_data, (1.0 / 4.0), stride, n, work, work_int);

    const double asymptoticCorrection = 1 / 0.9539;

    auto const asymptoticEstimate = asymptoticCorrection * (upper - lower);

    static double correctionFactors[] = {1.128, 1.303, 1.109, 1.064, 1.166, 1.103, 1.087, 1.105, 1.047, 1.063, 1.057,
                                         1.040, 1.061, 1.047, 1.043, 1.048, 1.031, 1.037, 1.035, 1.028, 1.036, 1.030,
                                         1.029, 1.032, 1.023, 1.025, 1.024, 1.021, 1.026, 1.022, 1.021, 1.023, 1.018,
                                         1.020, 1.019, 1.017, 1.020, 1.018, 1.017, 1.018, 1.015, 1.016, 1.016, 1.014,
                                         1.016, 1.015, 1.014, 1.015
                                        };

    if (n <= 42)
    {
        return  asymptoticEstimate * correctionFactors[n - 2];
    }
    else
    {
        return asymptoticEstimate * (n / (n - 0.7));
    }

}


} // namespace Mathematics
