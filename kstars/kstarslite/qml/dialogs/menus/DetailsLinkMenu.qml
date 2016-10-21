// Copyright (C) 2016 Artem Fedoskin <afedoskin3@gmail.com>
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

import QtQuick.Controls 2.0
import QtQuick 2.7
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../../modules/"
import "../../modules/helpers"

Menu {
    modal: true
    transformOrigin: Menu.Center
    padding: 5
    property int itemIndex: -1
    property bool isImage: false
    background: Rectangle {
        implicitWidth: 200
        color: num.sysPalette.base
        radius: 5
    }

    function openForImage(index) {
        isImage = true
        itemIndex = index
        open();
    }

    function openForInfo(index) {
        isImage = false
        itemIndex = index
        open();
    }

    KSMenuItem {
        text: xi18n("View resource")
        onTriggered: {
            if(isImage) {
                Qt.openUrlExternally(DetailDialogLite.getImageURL(itemIndex));
            } else {
                Qt.openUrlExternally(DetailDialogLite.getInfoURL(itemIndex));
            }
        }
    }

    KSMenuItem {
        text: xi18n("Edit")
        onTriggered: {
            detailsAddLink.openEdit(itemIndex, isImage)
        }
    }

    KSMenuItem {
        text: xi18n("Delete")
        onTriggered: {
            DetailDialogLite.removeLink(itemIndex, isImage)
        }
    }
}

