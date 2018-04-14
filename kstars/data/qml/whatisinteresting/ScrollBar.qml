/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/** This qml code implements a vertical scrollbar which shall be displayed in listview of sky-objects.
  * This piece of code is based on this example code:
  * https://projects.forum.nokia.com/qmluiexamples/browser/qml/qmluiexamples/Scrollable/ScrollBar.qml
  */

import QtQuick 1.0

Rectangle {
    // The flickable to which the scrollbar is attached to, must be set
    property variant flickable

    // If set to false, scrollbar is visible even when not scrolling
    property bool hideScrollBarsWhenStopped: true

    // Thickness of the scrollbar, in pixels
    property int scrollbarWidth: 10

    color: "gray"
    radius: width/2

    function sbOpacity()
    {
        if (!hideScrollBarsWhenStopped || height < parent.height)
        {
            return 0.5;
        }

        return (flickable.flicking || flickable.moving) ? (height >= parent.height ? 0 : 0.5) : 0;
    }

    // Scrollbar appears automatically when content is bigger than the Flickable
    opacity: sbOpacity()

    width: scrollbarWidth
    height: flickable.visibleArea.heightRatio * parent.height
    x: parent.width - width
    y: flickable.visibleArea.yPosition * parent.height

    // Animate scrollbar appearing/disappearing
    Behavior on opacity { NumberAnimation { duration: 200 }}
}
