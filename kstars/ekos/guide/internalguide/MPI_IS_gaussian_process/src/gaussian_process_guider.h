/*
    PHD2 Guiding

    @file
    @date      2014-2017
    @copyright Max Planck Society

    @brief     Provides a Gaussian process based guiding algorithm

    SPDX-FileCopyrightText: Edgar D. Klenske <edgar.klenske@tuebingen.mpg.de>
    SPDX-FileCopyrightText: Stephan Wenninger <stephan.wenninger@tuebingen.mpg.de>
    SPDX-FileCopyrightText: Raffi Enficiaud <raffi.enficiaud@tuebingen.mpg.de>
    SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef GAUSSIAN_PROCESS_GUIDER
#define GAUSSIAN_PROCESS_GUIDER

// Removed the dependence on phd2
// #include "circbuf.h"

#include "gaussian_process.h"
#include "covariance_functions.h"
#include "math_tools.h"

#include <chrono>

enum Hyperparameters
{
    SE0KLengthScale,
    SE0KSignalVariance,
    PKLengthScale,
    PKSignalVariance,
    SE1KLengthScale,
    SE1KSignalVariance,
    PKPeriodLength,
    NumParameters // this represents the number of elements in the enum
};

/**
 * This class provides a guiding algorithm for the right ascension axis that
 * learns and predicts the periodic gear error with a Gaussian process. This
 * prediction helps reducing periodic error components in the residual tracking
 * error. Further it is able to perform tracking without measurement to increase
 * robustness of the overall guiding system.
 */
class GaussianProcessGuider
{
        // HY: Added this subclass to remove dependency on phd2.
        // Other than this class, this is the original GPG code.
        // This implements a basix circular buffer that can hold upto size items.
        // Cannot resize, cannot remove items. Can just insert and randomly access
        // the last "size" items in the order inserted, or clear the entire buffer.
        template <typename T>
        class circular_buffer
        {
            public:
                circular_buffer(int size) : size_(size), buff(new T[size]) {}
                ~circular_buffer()
                {
                    delete[] buff;
                }

                T &operator[](int index) const
                {
                    assert(index >= 0 && index < size_ && index < num_items);
                    int location = (start + index ) % size_;
                    return buff[location];
                }

                int size() const
                {
                    return num_items;
                }

                void clear()
                {
                    start = 0;
                    end = 0;
                    num_items = 0;
                }

                void push_front(T item)
                {
                    buff[end] = item;
                    num_items++;
                    if (num_items >= size_) num_items = size_;
                    end++;
                    if (end >= size_) end = 0;
                }

            private:
                int size_ = 0;     // Max length of the queue.
                T *buff;           // Data storage.
                int start = 0;     // Index of oldest entry in the buffer..
                int end = 0;       // Index of the next place to put data into the buffer.
                int num_items = 0; // Number of items currently in the circular buffer.
        };

    public:

        struct data_point
        {
            double timestamp;
            double measurement; // current pointing error
            double variance; // current measurement variance
            double control; // control action
        };

        /**
         * Holds all data that is needed for the GP guiding.
         */
        struct guide_parameters
        {
            double control_gain_;
            double min_move_;
            double prediction_gain_;

            double min_periods_for_inference_;
            double min_periods_for_period_estimation_;

            int points_for_approximation_;

            bool compute_period_;

            double SE0KLengthScale_;
            double SE0KSignalVariance_;
            double PKLengthScale_;
            double PKSignalVariance_;
            double SE1KLengthScale_;
            double SE1KSignalVariance_;
            double PKPeriodLength_;

            guide_parameters() :
                control_gain_(0.0),
                min_move_(0.0),
                prediction_gain_(0.0),
                min_periods_for_inference_(0.0),
                min_periods_for_period_estimation_(0.0),
                points_for_approximation_(0),
                compute_period_(false),
                SE0KLengthScale_(0.0),
                SE0KSignalVariance_(0.0),
                PKLengthScale_(0.0),
                PKSignalVariance_(0.0),
                SE1KLengthScale_(0.0),
                SE1KSignalVariance_(0.0),
                PKPeriodLength_(0.0)
            {
            }

        };

    private:

        std::chrono::system_clock::time_point start_time_; // reference time
        std::chrono::system_clock::time_point last_time_;

        double control_signal_;
        double prediction_;
        double last_prediction_end_;

        int dither_steps_;
        bool dithering_active_;

        //! the dither offset collects the correction in gear time from dithering
        double dither_offset_;

        circular_buffer<data_point> circular_buffer_data_;

        covariance_functions::PeriodicSquareExponential2 covariance_function_; // for inference
        covariance_functions::PeriodicSquareExponential output_covariance_function_; // for prediction
        GP gp_;

        /**
         * Learning rate for smooth parameter adaptation.
         */
        double learning_rate_;

        /**
         * Guiding parameters of this instance.
         */
        guide_parameters parameters;

        /**
         * Stores the current time and creates a timestamp for the GP.
         */
        void SetTimestamp();

        /**
         * Stores the measurement, SNR and resets last_prediction_end_.
         */
        void HandleGuiding(double input, double SNR);

        /**
         * Stores a zero as blind "measurement" with high variance.
         */
        void HandleDarkGuiding();

        /**
         * Stores the control value.
         */
        void HandleControls(double control_input);

        /**
         * Calculates the noise from the reported SNR value according to an
         * empirically justified equation and stores it.
         */
        double CalculateVariance(double SNR);

        /**
         * Estimates the main period length for a given dataset.
         */
        double EstimatePeriodLength(const Eigen::VectorXd &time, const Eigen::VectorXd &data);

        /**
         * Calculates the difference in gear error for the time between the last
         * prediction point and the current prediction point, which lies one
         * exposure length in the future.
         */
        double PredictGearError(double prediction_location);



    public:
        double GetControlGain() const;
        bool SetControlGain(double control_gain);

        double GetMinMove() const;
        bool SetMinMove(double min_move);

        double GetPeriodLengthsInference() const;
        bool SetPeriodLengthsInference(double num_periods);

        double GetPeriodLengthsPeriodEstimation() const;
        bool SetPeriodLengthsPeriodEstimation(double num_periods);

        int GetNumPointsForApproximation() const;
        bool SetNumPointsForApproximation(int num_points);

        bool GetBoolComputePeriod() const;
        bool SetBoolComputePeriod(bool active);

        std::vector<double> GetGPHyperparameters() const;
        bool SetGPHyperparameters(const std::vector<double> &hyperparameters);

        double GetPredictionGain() const;
        bool SetPredictionGain(double);

        GaussianProcessGuider(guide_parameters parameters);
        ~GaussianProcessGuider();

        /**
         * Calculates the control value based on the current input. 1. The input is
         * stored, 2. the GP is updated with the new data point, 3. the prediction
         * is calculated to compensate the gear error and 4. the controller is
         * calculated, consisting of feedback and prediction parts.
         */
        double result(double input, double SNR, double time_step, double prediction_point = -1.0);

        /**
         * This method provides predictive control if no measurement could be made.
         * A zero measurement is stored with high uncertainty, and then the GP
         * prediction is used for control.
         */
        double deduceResult(double time_step, double prediction_point = -1.0);

        /**
         * This method tells the guider that a dither command was issued. The guider
         * will stop collecting measurements and uses predictions instead, to keep
         * the FFT and the GP working.
         */
        void GuidingDithered(double amt, double rate);

        /**
         * This method tells the guider that a direct move was
         * issued. This has the opposite effect of a dither on the dither
         * offset.
         */
        void DirectMoveApplied(double amt, double rate);

        /**
         * This method tells the guider that dithering is finished. The guider
         * will resume normal operation.
         */
        void GuidingDitherSettleDone(bool success);

        /**
         * Clears the data from the circular buffer and clears the GP data.
         */
        void reset();

        /**
         * Runs the inference machinery on the GP. Gets the measurement data from
         * the circular buffer and stores it in Eigen::Vectors. Detrends the data
         * with linear regression. Calculates the main frequency with an FFT.
         * Updates the GP accordingly with new data and parameter.
         */
        void UpdateGP(double prediction_point = std::numeric_limits<double>::quiet_NaN());

        /**
         * Does filtering and sets the period length of the GPGuider.
         */
        void UpdatePeriodLength(double period_length);

        data_point &get_last_point() const
        {
            return circular_buffer_data_[circular_buffer_data_.size() - 1];
        }

        data_point &get_second_last_point() const
        {
            return circular_buffer_data_[circular_buffer_data_.size() - 2];
        }

        size_t get_number_of_measurements() const
        {
            return circular_buffer_data_.size();
        }

        void add_one_point()
        {
            circular_buffer_data_.push_front(data_point());
        }

        /**
         * This method is needed for automated testing. It can inject data points.
         */
        void inject_data_point(double timestamp, double input, double SNR, double control);

        /**
         * Takes timestamps, measurements and SNRs and returns them regularized in a matrix.
         */
        Eigen::MatrixXd regularize_dataset(const Eigen::VectorXd &timestamps, const Eigen::VectorXd &gear_error,
                                           const Eigen::VectorXd &variances);

        /**
         * Saves the GP data to a csv file for external analysis. Expensive!
         */
        void save_gp_data() const;

        /**
         * Sets the learning rate. Useful for disabling it for testing.
         */
        void SetLearningRate(double learning_rate);
};

//
// GPDebug abstract interface to allow logging to the PHD2 Debug Log when
// GaussianProcessGuider is built in the context of the PHD2 application
//
// Add code like this to record debug info in the PHD2 debug log (with newline appended)
//
//   GPDebug->Log("input: %.2f SNR: %.1f time_step: %.1f", input, SNR, time_step);
//
// Outside of PHD2, like in the test framework, these calls will not produce any output
//
class GPDebug
{
    public:
        static void SetGPDebug(GPDebug *logger);
        virtual ~GPDebug();
        virtual void Log(const char *format, ...) = 0;
};

extern class GPDebug *GPDebug;

#endif  // GAUSSIAN_PROCESS_GUIDER
