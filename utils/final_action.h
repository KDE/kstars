/***************************************************************************
                  final_action.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2021-06-03
    copyright            : (C) 2021 by Valentin Boettcher
    email                : hiro at protagon.space; @hiro98:tchncs.de
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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
