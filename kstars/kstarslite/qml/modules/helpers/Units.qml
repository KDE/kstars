/*
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

import QtQuick 2.4
import QtQuick.Controls.Private 1.0
import QtQuick.Window 2.2

QtObject {
    id: units

    /**
     * The fundamental unit of space that should be used for sizes, expressed in pixels.
     * Given the screen has an accurate DPI settings, it corresponds to a width of
     * the capital letter M
     */
    property int gridUnit: fontMetrics.height

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
    /*property QtObject iconSizes: QtObject {
        property int small: 16 * devicePixelRatio * (Settings.isMobile ? 1.5 : 1)
        property int smallMedium: 22 * devicePixelRatio * (Settings.isMobile ? 1.5 : 1)
        property int medium: 32 * devicePixelRatio * (Settings.isMobile ? 1.5 : 1)
        property int large: 48 * devicePixelRatio * (Settings.isMobile ? 1.5 : 1)
        property int huge: 64 * devicePixelRatio * (Settings.isMobile ? 1.5 : 1)
        property int enormous: 128 * devicePixelRatio * (Settings.isMobile ? 1.5 : 1)
    }*/

    /**
     * units.smallSpacing is the amount of spacing that should be used around smaller UI elements,
     * for example as spacing in Columns. Internally, this size depends on the size of
     * the default font as rendered on the screen, so it takes user-configured font size and DPI
     * into account.
     */
    property int smallSpacing: gridUnit/4

    /**
     * units.largeSpacing is the amount of spacing that should be used inside bigger UI elements,
     * for example between an icon and the corresponding text. Internally, this size depends on
     * the size of the default font as rendered on the screen, so it takes user-configured font
     * size and DPI into account.
     */
    property int largeSpacing: gridUnit

    /**
     * The ratio between physical and device-independent pixels. This value does not depend on the
     * size of the configured font. If you want to take font sizes into account when scaling elements,
     * use theme.mSize(theme.defaultFont), units.smallSpacing and units.largeSpacing.
     * The devicePixelRatio follows the definition of "device independent pixel" by Microsoft.
     */
    property real devicePixelRatio: fontMetrics.font.pixelSize / (fontMetrics.font.pointSize * 1.33)

    /**
     * units.longDuration should be used for longer, screen-covering animations, for opening and
     * closing of dialogs and other "not too small" animations
     */
    property int longDuration: 250

    /**
     * units.shortDuration should be used for short animations, such as accentuating a UI event,
     * hover events, etc.
     */
    property int shortDuration: 150

    /**
     * metrics used by the default font
     */
    property variant fontMetrics: TextMetrics {
        text: "M"
    }
}
