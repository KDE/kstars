/*  Halton Sequence Generator
    SPDX-FileCopyrightText: 2026 Christian Kemper <ckemper@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

namespace Ekos
{

class HaltonSequence
{
    public:
        explicit HaltonSequence(int base) : m_value(0.0), m_invBase(1.0 / base), m_index(0) {}

        double next()
        {
            m_index++;
            double r = 1.0 - m_value - 1e-10;
            if (m_invBase < r)
            {
                m_value += m_invBase;
            }
            else
            {
                double h = m_invBase;
                double hh;
                do
                {
                    hh = h;
                    h *= m_invBase;
                }
                while (r < h);
                m_value += hh + h - 1.0;
            }
            return m_value;
        }

        int index() const { return m_index; }

    private:
        double m_value;
        double m_invBase;
        int m_index;
};

} // namespace Ekos
