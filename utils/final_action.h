/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/**
 * This header contains a cherry-picked version of the GSL `final_action`
 * as it is quite useful. You can see it in action over at
 * `kstars/datahandlers/catalogsdb.h`
 *
 * Further reading: http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Re-finally
 * Source: https://github.com/Microsoft/GSL
 */

#pragma once

namespace gsl
{
// final_action allows you to ensure something gets run at the end of a scope
template <class F>
class final_action
{
  public:
    static_assert(!std::is_reference<F>::value && !std::is_const<F>::value &&
                      !std::is_volatile<F>::value,
                  "Final_action should store its callable by value");

    explicit final_action(F f) noexcept : f_(std::move(f)) {}

    final_action(final_action &&other) noexcept
        : f_(std::move(other.f_)), invoke_(std::exchange(other.invoke_, false))
    {
    }

    final_action(const final_action &) = delete;
    final_action &operator=(const final_action &) = delete;
    final_action &operator=(final_action &&) = delete;

    ~final_action() noexcept
    {
        if (invoke_)
            f_();
    }

  private:
    F f_;
    bool invoke_{ true };
};

template <class F>
final_action<typename std::remove_cv<typename std::remove_reference<F>::type>::type>
finally(F &&f) noexcept
{
    return final_action<
        typename std::remove_cv<typename std::remove_reference<F>::type>::type>(
        std::forward<F>(f));
}
} // namespace gsl
