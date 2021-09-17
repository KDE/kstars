// SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick.Controls 2.0
import QtQuick 2.6
import QtQuick.Layouts 1.1
import "../../constants" 1.0
import "../../modules"
import KStarsLiteEnums 1.0

Popup {
    id: projPopup
    focus: true
    modal: true
    width: projList.implicitWidth
    height: parent.height > projList.implicitHeight ? projList.implicitHeight : parent.height

    background: Rectangle {
        anchors.fill: parent
        color: Num.sysPalette.base
    }

    KSListView {
        id: projList
        anchors {
            fill: parent
            centerIn: parent
        }
        checkCurrent: true

        model: ListModel {
            id: projModel
            Component.onCompleted: {
                projModel.append({ name: xi18n("Lambert (Default)"), proj: Projection.Lambert });
                projModel.append({ name: xi18n("Azimuthal Equidistant"), proj: Projection.AzimuthalEquidistant });
                projModel.append({ name: xi18n("Orthographic"), proj: Projection.Orthographic });
                projModel.append({ name: xi18n("Equirectangular"), proj: Projection.Equirectangular });
                projModel.append({ name: xi18n("Stereographic"), proj: Projection.Stereographic });
                projModel.append({ name: xi18n("Gnomonic"), proj: Projection.Gnomonic });

                //Initialize projector
                for(var i = 0; i < projList.model.count; ++i) {
                    if(projList.model.get(i).proj == SkyMapLite.projType()) {
                       projList.currentIndex = i
                    }
                }
            }
        }

        onClicked: {
            var item = projModel.get(projList.currentIndex)
            KStarsLite.setProjection(item.proj)
            skyMapLite.notification.showNotification("Set projection system to "
                                          + item.name)
            close()
        }

        textRole: "name"
    }
}
