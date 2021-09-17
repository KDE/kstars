/*
    SPDX-FileCopyrightText: 2017 Max Planck Society.
    All rights reserved.

    SPDX-License-Identifier: BSD-3-Clause
*/

/* Created by Edgar Klenske <edgar.klenske@tuebingen.mpg.de>
 */

#include "gaussian_process_guider.h"
#include "guide_performance_tools.h"

int main(int argc, char** argv)
{
    if (argc < 15)
        return -1;

    GaussianProcessGuider* GPG;

    GaussianProcessGuider::guide_parameters parameters;

    // use parameters from command line
    parameters.control_gain_ = std::stod(argv[2]);
    parameters.min_periods_for_inference_ = std::stod(argv[3]);
    parameters.min_move_ = std::stod(argv[4]);
    parameters.SE0KLengthScale_ = std::stod(argv[5]);
    parameters.SE0KSignalVariance_ = std::stod(argv[6]);
    parameters.PKLengthScale_ = std::stod(argv[7]);
    parameters.PKPeriodLength_ = std::stod(argv[8]);
    parameters.PKSignalVariance_ = std::stod(argv[9]);
    parameters.SE1KLengthScale_ = std::stod(argv[10]);
    parameters.SE1KSignalVariance_ = std::stod(argv[11]);
    parameters.min_periods_for_period_estimation_ = std::stod(argv[12]);
    parameters.points_for_approximation_ = static_cast<int>(std::floor(std::stod(argv[13])));
    parameters.prediction_gain_ = std::stod(argv[14]);
    parameters.compute_period_ = true;

    GPG = new GaussianProcessGuider(parameters);

    GAHysteresis GAH;

    std::string filename;

    filename = argv[1];
    double improvement = calculate_improvement(filename, GAH, GPG);

    std::cout << improvement << std::endl;

    return 0;
}
