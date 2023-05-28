/*
    SPDX-FileCopyrightText: 2022 Sophie Taylor <sophie@spacekitteh.moe>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gslhelpers.h"

namespace Mathematics
{

std::shared_ptr<gsl_vector> GSLHelpers::NewGSLVector(size_t n)
{
    auto vec = gsl_vector_calloc(n);
    return std::shared_ptr<gsl_vector>(vec, gsl_vector_free);
}

} // namespace Mathematics
