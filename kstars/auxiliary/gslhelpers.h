/*
    SPDX-FileCopyrightText: 2022 Sophie Taylor <sophie@spacekitteh.moe>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>
#include <cstdint>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_sort.h>
#include <gsl/gsl_statistics.h>

// gslhelpers is used to provide utility routines to access GNU Science Library (GSL) routines
// iterators to access strided arrays are supported and these routines help to access the correct
// GSL rotines (C based) for the datatype of the input data.
//
// gslheklpers is currently used by robuststatistics.h/cpp to access GSL robust statistics routines but
// could be extended in future to access other GSL functions.

namespace Mathematics
{

namespace GSLHelpers
{

// int64 code is currently deactiovated by GSLHELPERS_INT64. It doesn't compile on Mac becuase it
// won't cast a long long * to a long * even though they are both 64bit pointers.
// On 32bit systems this would be an issue because they are not the same.
// If the int64 versions are required in future then this "problem" will need to
// be resolved.
#undef GSLHELPERS_INT64

/**
 * @short creates a new shared pointer to a GSL vector which gets automatically freed.
 * The vector is initialised to 0.
 *
 * @param n the size of the vector.
 */
std::shared_ptr<gsl_vector> NewGSLVector(size_t n);

template<class Iter_T>
class stride_iter
{
    public:
        // public typedefs
        typedef typename std::iterator_traits<Iter_T>::value_type value_type;
        typedef typename std::iterator_traits<Iter_T>::reference reference;
        typedef typename std::iterator_traits<Iter_T>::difference_type
        difference_type;
        typedef typename std::iterator_traits<Iter_T>::pointer pointer;
        typedef std::random_access_iterator_tag iterator_category;
        typedef stride_iter self;

        // constructors
        constexpr stride_iter( ) : iter(nullptr), step(0) { }
        constexpr stride_iter(const self &x) : iter(x.iter), step(x.step) { }
        constexpr stride_iter(Iter_T x, difference_type n) : iter(x), step(n) { }

        // operators
        constexpr self &operator++( )
        {
            iter += step;
            return *this;
        }
        constexpr self operator++(int)
        {
            self tmp = *this;
            iter += step;
            return tmp;
        }
        constexpr self &operator+=(difference_type x)
        {
            iter += x * step;
            return *this;
        }
        constexpr self &operator--( )
        {
            iter -= step;
            return *this;
        }
        constexpr self operator--(int)
        {
            self tmp = *this;
            iter -= step;
            return tmp;
        }
        constexpr self &operator-=(difference_type x)
        {
            iter -= x * step;
            return *this;
        }
        constexpr reference operator[](difference_type n)
        {
            return iter[n * step];
        }
        reference operator*( ) const
        {
            return *iter;
        }

        // friend operators
        constexpr friend bool operator==(const self &x, const self &y)
        {
            assert(x.step == y.step);
            return x.iter == y.iter;
        }
        constexpr friend bool operator!=(const self &x, const self &y)
        {
            assert(x.step == y.step);
            return x.iter != y.iter;
        }
        constexpr friend bool operator<(const self &x, const self &y)
        {
            assert(x.step == y.step);
            return x.iter < y.iter;
        }
        constexpr friend difference_type operator-(const self &x, const self &y)
        {
            assert(x.step == y.step);
            return (x.iter - y.iter) / x.step;
        }
        constexpr friend self operator+(const self &x, difference_type y)
        {
            assert(x.step == y.step);
            return x += y * x.step;
        }
        constexpr friend self operator+(difference_type x, const self &y)
        {
            assert(x.step == y.step);
            return y += x * x.step;
        }
    private:
        Iter_T iter;
        difference_type step;
};

template< class Iter_T >
constexpr static stride_iter<Iter_T> make_strided_iter(Iter_T i, typename stride_iter<Iter_T>::difference_type stride)
{
    return stride_iter<Iter_T>(i, stride);
}




inline void gslSort(uint8_t data[], const size_t stride, const size_t n)
{
    gsl_sort_uchar(data, stride, n);
}
inline void gslSort(double data[], const size_t stride, const size_t n)
{
    gsl_sort(data, stride, n);
}
inline void gslSort(float data[], const size_t stride, const size_t n)
{
    gsl_sort_float(data, stride, n);
}
inline void gslSort(uint16_t data[], const size_t stride, const size_t n)
{
    gsl_sort_ushort(data, stride, n);
}
inline void gslSort(int16_t data[], const size_t stride, const size_t n)
{
    gsl_sort_short(data, stride, n);
}
inline void gslSort(uint32_t data[], const size_t stride, const size_t n)
{
    gsl_sort_uint(data, stride, n);
}
inline void gslSort(int32_t data[], const size_t stride, const size_t n)
{
    gsl_sort_int(data, stride, n);
}
#ifdef GSLHELPERS_INT64
inline void gslSort(int64_t data[], const size_t stride, const size_t n)
{
    gsl_sort_long(data, stride, n);
}
#endif







inline double gslMean(const uint8_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_uchar_mean(data, stride, n);
}
inline double gslMean(const double data[], const size_t stride, const size_t n)
{
    return gsl_stats_mean(data, stride, n);
}
inline double gslMean(const float data[], const size_t stride, const size_t n)
{
    return gsl_stats_float_mean(data, stride, n);
}
inline double gslMean(const uint16_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_ushort_mean(data, stride, n);
}
inline double gslMean(const int16_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_short_mean(data, stride, n);
}
inline double gslMean(const uint32_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_uint_mean(data, stride, n);
}
inline double gslMean(const int32_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_int_mean(data, stride, n);
}
#ifdef GSLHELPERS_INT64
inline double gslMean(const int64_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_long_mean(data, stride, n);
}
#endif


inline double gslGastwirthMeanFromSortedData(const uint8_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_uchar_gastwirth_from_sorted_data(data, stride, n);
}
inline double gslGastwirthMeanFromSortedData(const double data[], const size_t stride, const size_t n)
{
    return gsl_stats_gastwirth_from_sorted_data(data, stride, n);
}
inline double gslGastwirthMeanFromSortedData(const float data[], const size_t stride, const size_t n)
{
    return gsl_stats_float_gastwirth_from_sorted_data(data, stride, n);
}
inline double gslGastwirthMeanFromSortedData(const uint16_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_ushort_gastwirth_from_sorted_data(data, stride, n);
}
inline double gslGastwirthMeanFromSortedData(const int16_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_short_gastwirth_from_sorted_data(data, stride, n);
}
inline double gslGastwirthMeanFromSortedData(const uint32_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_uint_gastwirth_from_sorted_data(data, stride, n);
}
inline double gslGastwirthMeanFromSortedData(const int32_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_int_gastwirth_from_sorted_data(data, stride, n);
}
#ifdef GSLHELPERS_INT64
inline double gslGastwirthMeanFromSortedData(const int64_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_long_gastwirth_from_sorted_data(data, stride, n);
}
#endif


inline double gslTrimmedMeanFromSortedData(const double trim, const uint8_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_uchar_trmean_from_sorted_data(trim, data, stride, n);
}
inline double gslTrimmedMeanFromSortedData(const double trim, const double data[], const size_t stride, const size_t n)
{
    return gsl_stats_trmean_from_sorted_data(trim, data, stride, n);
}
inline double gslTrimmedMeanFromSortedData(const double trim, const float data[], const size_t stride, const size_t n)
{
    return gsl_stats_float_trmean_from_sorted_data(trim, data, stride, n);
}
inline double gslTrimmedMeanFromSortedData(const double trim, const uint16_t data[], const size_t stride,
        const size_t n)
{
    return gsl_stats_ushort_trmean_from_sorted_data(trim, data, stride, n);
}
inline double gslTrimmedMeanFromSortedData(const double trim, const int16_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_short_trmean_from_sorted_data(trim, data, stride, n);
}
inline double gslTrimmedMeanFromSortedData(const double trim, const uint32_t data[], const size_t stride,
        const size_t n)
{
    return gsl_stats_uint_trmean_from_sorted_data(trim, data, stride, n);
}
inline double gslTrimmedMeanFromSortedData(const double trim, const int32_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_int_trmean_from_sorted_data(trim, data, stride, n);
}
#ifdef GSLHELPERS_INT64
inline double gslTrimmedMeanFromSortedData(const double trim, const int64_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_long_trmean_from_sorted_data(trim, data, stride, n);
}
#endif




inline double gslMedianFromSortedData(const uint8_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_uchar_median_from_sorted_data(data, stride, n);
}
inline double gslMedianFromSortedData(const double data[], const size_t stride, const size_t n)
{
    return gsl_stats_median_from_sorted_data(data, stride, n);
}
inline double gslMedianFromSortedData(const float data[], const size_t stride, const size_t n)
{
    return gsl_stats_float_median_from_sorted_data(data, stride, n);
}
inline double gslMedianFromSortedData(const uint16_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_ushort_median_from_sorted_data(data, stride, n);
}
inline double gslMedianFromSortedData(const int16_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_short_median_from_sorted_data(data, stride, n);
}
inline double gslMedianFromSortedData(const uint32_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_uint_median_from_sorted_data(data, stride, n);
}
inline double gslMedianFromSortedData(const int32_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_int_median_from_sorted_data(data, stride, n);
}
#ifdef GSLHELPERS_INT64
inline double gslMedianFromSortedData(const int64_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_long_median_from_sorted_data(data, stride, n);
}
#endif


inline double gslVariance(const uint8_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_uchar_variance(data, stride, n);
}
inline double gslVariance(const double data[], const size_t stride, const size_t n)
{
    return gsl_stats_variance(data, stride, n);
}
inline double gslVariance(const float data[], const size_t stride, const size_t n)
{
    return gsl_stats_float_variance(data, stride, n);
}
inline double gslVariance(const uint16_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_ushort_variance(data, stride, n);
}
inline double gslVariance(const int16_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_short_variance(data, stride, n);
}
inline double gslVariance(const uint32_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_uint_variance(data, stride, n);
}
inline double gslVariance(const int32_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_int_variance(data, stride, n);
}
#ifdef GSLHELPERS_INT64
inline double gslVariance(const int64_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_long_variance(data, stride, n);
}
#endif



inline double gslStandardDeviation(const uint8_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_uchar_sd(data, stride, n);
}
inline double gslStandardDeviation(const double data[], const size_t stride, const size_t n)
{
    return gsl_stats_sd(data, stride, n);
}
inline double gslStandardDeviation(const float data[], const size_t stride, const size_t n)
{
    return gsl_stats_float_sd(data, stride, n);
}
inline double gslStandardDeviation(const uint16_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_ushort_sd(data, stride, n);
}
inline double gslStandardDeviation(const int16_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_short_sd(data, stride, n);
}
inline double gslStandardDeviation(const uint32_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_uint_sd(data, stride, n);
}
inline double gslStandardDeviation(const int32_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_int_sd(data, stride, n);
}
#ifdef GSLHELPERS_INT64
inline double gslStandardDeviation(const int64_t data[], const size_t stride, const size_t n)
{
    return gsl_stats_long_sd(data, stride, n);
}
#endif



inline double gslMAD(const uint8_t data[], const size_t stride, const size_t n, double work[])
{
    return gsl_stats_uchar_mad(data, stride, n, work);
}
inline double gslMAD(const double data[], const size_t stride, const size_t n, double work[])
{
    return gsl_stats_mad(data, stride, n, work);
}
inline double gslMAD(const float data[], const size_t stride, const size_t n, double work[])
{
    return gsl_stats_float_mad(data, stride, n, work);
}
inline double gslMAD(const uint16_t data[], const size_t stride, const size_t n, double work[])
{
    return gsl_stats_ushort_mad(data, stride, n, work);
}
inline double gslMAD(const int16_t data[], const size_t stride, const size_t n, double work[])
{
    return gsl_stats_short_mad(data, stride, n, work);
}
inline double gslMAD(const uint32_t data[], const size_t stride, const size_t n, double work[])
{
    return gsl_stats_uint_mad(data, stride, n, work);
}
inline double gslMAD(const int32_t data[], const size_t stride, const size_t n, double work[])
{
    return gsl_stats_int_mad(data, stride, n, work);
}
#ifdef GSLHELPERS_INT64
inline double gslMAD(const int64_t data[], const size_t stride, const size_t n, double work[])
{
    return gsl_stats_long_mad(data, stride, n, work);
}
#endif


inline double gslSnFromSortedData(const uint8_t data[], const size_t stride, const size_t n, uint8_t work[])
{
    return gsl_stats_uchar_Sn_from_sorted_data(data, stride, n, work);
}
inline double gslSnFromSortedData(const double data[], const size_t stride, const size_t n, double work[])
{
    return gsl_stats_Sn_from_sorted_data(data, stride, n, work);
}
inline double gslSnFromSortedData(const float data[], const size_t stride, const size_t n, float work[])
{
    return gsl_stats_float_Sn_from_sorted_data(data, stride, n, work);
}
inline double gslSnFromSortedData(const uint16_t data[], const size_t stride, const size_t n, uint16_t work[])
{
    return gsl_stats_ushort_Sn_from_sorted_data(data, stride, n, work);
}
inline double gslSnFromSortedData(const int16_t data[], const size_t stride, const size_t n, int16_t work[])
{
    return gsl_stats_short_Sn_from_sorted_data(data, stride, n, work);
}
inline double gslSnFromSortedData(const uint32_t data[], const size_t stride, const size_t n, uint32_t work[])
{
    return gsl_stats_uint_Sn_from_sorted_data(data, stride, n, work);
}
inline double gslSnFromSortedData(const int32_t data[], const size_t stride, const size_t n, int32_t work[])
{
    return gsl_stats_int_Sn_from_sorted_data(data, stride, n, work);
}
#ifdef GSLHELPERS_INT64
inline double gslSnFromSortedData(const int64_t data[], const size_t stride, const size_t n, int64_t work[])
{
    return gsl_stats_long_Sn_from_sorted_data(data, stride, n, work);
}
#endif

inline double gslQnFromSortedData(const uint8_t data[], const size_t stride, const size_t n, uint8_t work[],
                                  int work_int[])
{
    return gsl_stats_uchar_Qn_from_sorted_data(data, stride, n, work, work_int);
}
inline double gslQnFromSortedData(const double data[], const size_t stride, const size_t n, double work[],
                                  int work_int[])
{
    return gsl_stats_Qn_from_sorted_data(data, stride, n, work, work_int);
}
inline double gslQnFromSortedData(const float data[], const size_t stride, const size_t n, float work[], int work_int[])
{
    return gsl_stats_float_Qn_from_sorted_data(data, stride, n, work, work_int);
}
inline double gslQnFromSortedData(const uint16_t data[], const size_t stride, const size_t n, uint16_t work[],
                                  int work_int[])
{
    return gsl_stats_ushort_Qn_from_sorted_data(data, stride, n, work, work_int);
}
inline double gslQnFromSortedData(const int16_t data[], const size_t stride, const size_t n, int16_t work[],
                                  int work_int[])
{
    return gsl_stats_short_Qn_from_sorted_data(data, stride, n, work, work_int);
}
inline double gslQnFromSortedData(const uint32_t data[], const size_t stride, const size_t n, uint32_t work[],
                                  int work_int[])
{
    return gsl_stats_uint_Qn_from_sorted_data(data, stride, n, work, work_int);
}
inline double gslQnFromSortedData(const int32_t data[], const size_t stride, const size_t n, int32_t work[],
                                  int work_int[])
{
    return gsl_stats_int_Qn_from_sorted_data(data, stride, n, work, work_int);
}
#ifdef GSLHELPERS_INT64
inline double gslQnFromSortedData(const int64_t data[], const size_t stride, const size_t n, int64_t work[],
                                  int work_int[])
{
    return gsl_stats_long_Qn_from_sorted_data(data, stride, n, work, work_int);
}
#endif

}
} // namespace Mathematics
