/*
 *   Copyright 2015 Marco Martin <mart@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

pragma Singleton

import QtQuick 2.4
import org.kde.plasma.core 2.0 as PlasmaCore


QtObject {
    /**
     * The fundamental unit of space that should be used for sizes, expressed in pixels.
     * Given the screen has an accurate DPI settings, it corresponds to a width of
     * the capital letter M
     */
    property int gridUnit: units.gridUnit

    /**
     * units.iconSizes provides access to platform-dependent icon sizing
     *
     * The icon sizes provided are normalized for different DPI, so icons
     * will scale depending on the DPI.
     *
     * Icon sizes from KIconLoader, adjusted to devicePixelRatio:
     * * small
     * * smallMedium
     * * medium
     * * large
     * * huge
     * * enormous
     *
     * Not devicePixelRation-adjusted::
     * * desktop
     */
    property QtObject iconSizes: QtObject {
        property int small: units.iconSizes.small
        property int smallMedium: units.iconSizes.smallMedium
        property int medium: units.iconSizes.medium
        property int large: units.iconSizes.large
        property int huge: units.iconSizes.huge
        property int enormous: units.iconSizes.enormous
    }

    /**
     * units.smallSpacing is the amount of spacing that should be used around smaller UI elements,
     * for example as spacing in Columns. Internally, this size depends on the size of
     * the default font as rendered on the screen, so it takes user-configured font size and DPI
     * into account.
     */
    property int smallSpacing: units.smallSpacing

    /**
     * units.largeSpacing is the amount of spacing that should be used inside bigger UI elements,
     * for example between an icon and the corresponding text. Internally, this size depends on
     * the size of the default font as rendered on the screen, so it takes user-configured font
     * size and DPI into account.
     */
    property int largeSpacing: units.largeSpacing

    /**
     * The ratio between physical and device-independent pixels. This value does not depend on the \
     * size of the configured font. If you want to take font sizes into account when scaling elements,
     * use theme.mSize(theme.defaultFont), units.smallSpacing and units.largeSpacing.
     * The devicePixelRatio follows the definition of "device independent pixel" by Microsoft.
     */
    property real devicePixelRatio: units.devicePixelRatio

    /**
     * units.longDuration should be used for longer, screen-covering animations, for opening and
     * closing of dialogs and other "not too small" animations
     */
    property int longDuration: units.longDuration

    /**
     * units.shortDuration should be used for short animations, such as accentuating a UI event,
     * hover events, etc..
     */
    property int shortDuration: units.shortDuration

    /**
     * metrics used by the default font
     */
    property variant fontMetrics: TextMetrics {
        text: "M"
    }
}
