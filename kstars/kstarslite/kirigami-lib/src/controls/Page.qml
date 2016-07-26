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

import QtQuick 2.1
import QtQuick.Layouts 1.2
import org.kde.kirigami 1.0
import "private"

/**
 * Page is a container for all the app pages: everything pushed to the
 * ApplicationWindow stackView should be a Page instabnce (or a subclass,
 * such as ScrollablePage)
 * @see ScrollablePage
 */
Item {
    id: root

    /**
     * title: string
     * Title for the page
     */
    property string title

    /**
     * leftPadding: int
     * default contents padding at left
     */
    property int leftPadding: Units.gridUnit

    /**
     * topPadding: int
     * default contents padding at top
     */
    property int topPadding: Units.gridUnit

    /**
     * rightPadding: int
     * default contents padding at right
     */
    property int rightPadding: Units.gridUnit

    /**
     * bottomPadding: int
     * default contents padding at bottom
     */
    property int bottomPadding: Units.gridUnit

    /**
     * contentData: Item
     * The main items contained in this Page
     */
    default property alias contentData: container.data

    /**
     * flickable: Flickable
     * if the central element of the page is a Flickable
     * (ListView and Gridview as well) you can set it there.
     * normally, you wouldn't need to do that, but just use the
     * ScrollablePage element instead
     * @see ScrollablePage
     * Use this if your flickable has some non standard properties, such as not covering the whole Page
     */
    property Flickable flickable

    /**
     * actions.contextualActions: list<QtObject>
     * Defines the contextual actions for the page:
     * an easy way to assign actions in the right sliding panel
     *
     * Example usage:
     * @code
     * import org.kde.kirigami 1.0 as Kirigami
     *
     * Kirigami.ApplicationWindow {
     *  [...]
     *     contextDrawer: Kirigami.ContextDrawer {
     *         id: contextDrawer
     *     }
     *  [...]
     * }
     * @endcode
     *
     * @code
     * import org.kde.kirigami 1.0 as Kirigami
     *
     * Kirigami.Page {
     *   [...]
     *     actions.contextualActions: [
     *         Kirigami.Action {
     *             iconName: "edit"
     *             text: "Action text"
     *             onTriggered: {
     *                 // do stuff
     *             }
     *         },
     *         Kirigami.Action {
     *             iconName: "edit"
     *             text: "Action text"
     *             onTriggered: {
     *                 // do stuff
     *             }
     *         }
     *     ]
     *   [...]
     * }
     * @endcode
     */
    //TODO: remove
    property alias contextualActions: actionsGroup.contextualActions

    /**
     * actions.main: Action
     * An optional single action for the action button.
     * it can be a Kirigami.Action or a QAction
     *
     * Example usage:
     *
     * @code
     * import org.kde.kirigami 1.0 as Kirigami
     * Kirigami.Page {
     *     actions.main: Kirigami.Action {
     *         iconName: "edit"
     *         onTriggered: {
     *             // do stuff
     *         }
     *     }
     * }
     * @endcode
     */
    //TODO: remove
    property alias mainAction: actionsGroup.main

    /**
     * actions.left: Action
     * An optional extra action at the left of the main action button.
     * it can be a Kirigami.Action or a QAction
     *
     * Example usage:
     *
     * @code
     * import org.kde.kirigami 1.0 as Kirigami
     * Kirigami.Page {
     *     actions.left: Kirigami.Action {
     *         iconName: "edit"
     *         onTriggered: {
     *             // do stuff
     *         }
     *     }
     * }
     * @endcode
     */
    //TODO: remove
    property alias leftAction: actionsGroup.left

    /**
     * actions.right: Action
     * An optional extra action at the right of the main action button.
     * it can be a Kirigami.Action or a QAction
     *
     * Example usage:
     *
     * @code
     * import org.kde.kirigami 1.0 as Kirigami
     * Kirigami.Page {
     *     actions.right: Kirigami.Action {
     *         iconName: "edit"
     *         onTriggered: {
     *             // do stuff
     *         }
     *     }
     * }
     * @endcode
     */
    //TODO: remove
    property alias rightAction: actionsGroup.right

    /**
     * Actions properties are grouped.
     *
     * @code
     * import org.kde.kirigami 1.0 as Kirigami
     * Kirigami.Page {
     *     actions {
     *         main: Kirigami.Action {...}
     *         left: Kirigami.Action {...}
     *         right: Kirigami.Action {...}
     *         contextualActions: [
     *             Kirigami.Action {...},
     *             Kirigami.Action {...}
     *         ]
     *     }
     * }
     * @endcode
     */
    readonly property alias actions: actionsGroup

    PageActionPropertyGroup {
        id: actionsGroup
    }

    /**
     * background: Item
     * This property holds the background item.
     * Note: If the background item has no explicit size specified,
     * it automatically follows the control's size.
     * In most cases, there is no need to specify width or
     * height for a background item.
     */
    property Item background

    onBackgroundChanged: {
        background.z = -1;
        background.parent = root;
        background.anchors.fill = root;
    }

    /**
     * emitted When the application requests a Back action
     * For instance a global "back" shortcut or the Android
     * Back button has been pressed.
     * The page can manage the back event by itself,
     * and if it set event.accepted = true, it will stop the main
     * application to manage the back event.
     */
    signal backRequested(var event);

    Item {
        id: container
        onChildrenChanged: {
            //NOTE: make sure OverlaySheets are directly under the root
            //so they are over all the contents and don't have margins
            //TODO: OverlayDrawers as well?
            //search for an OverlaySheet, unfortunately have to blind test properties
            //as there is no way to get the classname from qml objects
            for (var i = children.length -1; i >= 0; --i) {
                var child = children[i];
                if (child.toString().indexOf("OverlaySheet") === 0) {
                    child.parent = root;
                }
            }
        }
        anchors {
            fill: parent
            leftMargin: leftPadding
            topMargin: topPadding + (applicationWindow === undefined || applicationWindow().wideScreen ? 0 : applicationWindow().header.height)
            rightMargin: rightPadding
            bottomMargin: bottomPadding
        }
    }

    Loader {
        z: 9999
        parent: root
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: item ? item.height : 0
        source: applicationWindow().header && applicationWindow().header.toString().indexOf("ToolBarApplicationHeader") !== 0 ? Qt.resolvedUrl("./private/ActionButton.qml") : ""
    }

    Layout.fillWidth: true
}
