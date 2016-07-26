/*
 *   Copyright 2015 Marco Martin <mart@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
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

import QtQuick 2.5
import QtQuick.Controls 1.3 as Controls
import "templates/private"
import org.kde.kirigami 1.0
import QtGraphicalEffects 1.0

/**
 * A window that provides some basic features needed for all apps
 * Use this class only if you need a custom content for your application,
 * different from the Page Row behavior recomended by the HIG and provided
 * by ApplicationWindow.
 * It is recomended to use ApplicationWindow instead
 * @see ApplicationWindow
 *
 * It's usually used as a root QML component for the application.
 * It provides support for a central page stack, side drawers and
 * a top ApplicationHeader, as well as basic support for the
 * Android back button
 *
 * Example usage:
 * @code
 * import org.kde.kirigami 1.0 as Kirigami
 *
 * Kirigami.ApplicationWindow {
 *  [...]
 *     globalDrawer: Kirigami.GlobalDrawer {
 *         actions: [
 *            Kirigami.Action {
 *                text: "View"
 *                iconName: "view-list-icons"
 *                Kirigami.Action {
 *                        text: "action 1"
 *                }
 *                Kirigami.Action {
 *                        text: "action 2"
 *                }
 *                Kirigami.Action {
 *                        text: "action 3"
 *                }
 *            },
 *            Kirigami.Action {
 *                text: "Sync"
 *                iconName: "folder-sync"
 *            }
 *         ]
 *     }
 *
 *     contextDrawer: Kirigami.ContextDrawer {
 *         id: contextDrawer
 *     }
 * 
 *     pageStack: PageStack {
 *         ...
 *     }
 *  [...]
 * }
 * @endcode
 *
 * @inherit QtQuick.Controls.ApplicationWindow
 */
Controls.ApplicationWindow {
    id: root

    /**
     * pageStack: StackView
     * Readonly.
     * The stack used to allocate the pages and to manage the transitions
     * between them.
     * Put a container here, such as QQuickControls PageStack
     */
    property Item pageStack

    /**
     * Shows a little passive notification at the bottom of the app window
     * lasting for few seconds, with an optional action button.
     *
     * @param message The text message to be shown to the user.
     * @param timeout How long to show the message:
     *            possible values: "short", "long" or the number of milliseconds
     * @param actionText Text in the action button, if any.
     * @param callBack A JavaScript function that will be executed when the
     *            user clicks the button.
     */
    function showPassiveNotification(message, timeout, actionText, callBack) {
        if (!internal.__passiveNotification) {
            var component = Qt.createComponent("templates/private/PassiveNotification.qml");
            internal.__passiveNotification = component.createObject(contentItem.parent);
        }

        internal.__passiveNotification.showNotification(message, timeout, actionText, callBack);
    }

   /**
    * Hide the passive notification, if any is shown
    */
    function hidePassiveNotification() {
        if(internal.__passiveNotification) {
           internal.__passiveNotification.hideNotification();
        }
    }


    /**
     * @returns a pointer to this application window
     * can be used anywhere in the application.
     */
    function applicationWindow() {
        return root;
    }

   /**
    * header: ApplicationHeader
    * An item that can be used as a title for the application.
    * Scrolling the main page will make it taller or shorter (trough the point of going away)
    * It's a behavior similar to the typical mobile web browser adressbar
    * the minimum, preferred and maximum heights of the item can be controlled with
    * * Layout.minimumHeight: default is 0, i.e. hidden
    * * Layout.preferredHeight: default is Units.gridUnit * 1.6
    * * Layout.maximumHeight: default is Units.gridUnit * 3
    *
    * To achieve a titlebar that stays completely fixed just set the 3 sizes as the same
    * //FIXME: this should become an actual ApplicationHeader
    */
    property var header: undefined

    /**
     * controlsVisible: bool
     * This property controls wether the standard chrome of the app, such
     * as the Action button, the drawer handles and the application
     * header should be visible or not.
     */
    property bool controlsVisible: true

    /**
     * globalDrawer: AbstractDrawer
     * The drawer for global actions, that will be opened by sliding from the
     * left screen edge or by dragging the ActionButton to the right.
     * It is recommended to use the GlobalDrawer class here
     */
    property AbstractDrawer globalDrawer

    /**
     * wideScreen: bool
     * If true the application is considered to be in "widescreen" mode, such as on desktops or horizontal tablets.
     * Different styles can have an own logic for deciding this
     */
    property bool wideScreen: width >= Units.gridUnit * 60

    /**
     * contextDrawer: AbstractDrawer
     * The drawer for context-dependednt actions, that will be opened by sliding from the
     * right screen edge or by dragging the ActionButton to the left.
     * It is recommended to use the ContextDrawer class here.
     * The contents of the context drawer should depend from what page is
     * loaded in the main pageStack
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
     *     contextualActions: [
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
     *
     * When this page will be the current one, the context drawer will visualize
     * contextualActions defined as property in that page.
     */
    property AbstractDrawer contextDrawer

    /**
     * reachableMode: bool
     * When true the application is in reachable mode for single hand use.
     * the whole content of the application is moved down the screen to be
     * reachable with the thumb. if wideScreen is true, tis property has
     * no effect.
     */
    property bool reachableMode: false

    MouseArea {
        parent: contentItem.parent
        z: -1
        anchors.fill: parent
        onClicked: root.reachableMode = false;
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.3)
            opacity: 0.15
        }
    }

    contentItem.anchors.topMargin: root.wideScreen && header && controlsVisible ? header.height : 0
    contentItem.anchors.leftMargin: root.globalDrawer && (root.globalDrawer.modal === false) ? root.globalDrawer.contentItem.width * root.globalDrawer.position : 0
    contentItem.anchors.rightMargin: root.contextDrawer && root.contextDrawer.modal === false ? root.contextDrawer.contentItem.width * root.contextDrawer.position : 0
    contentItem.transform: Translate {
        Behavior on y {
            NumberAnimation {
                duration: Units.longDuration
                easing.type: Easing.InOutQuad
            }
        }
        y: root.reachableMode && !root.wideScreen ? root.height/2 : 0
        x: root.globalDrawer && root.globalDrawer.modal === true && root.globalDrawer.toString().indexOf("SplitDrawer") === 0 ? root.globalDrawer.contentItem.width * root.globalDrawer.position : 0
    }
    //Don't want overscroll in landscape mode
    onWidthChanged: {
        if (width > height) {
            root.reachableMode = false;
        }
    }

    Binding {
        when: globalDrawer !== undefined
        target: globalDrawer
        property: "parent"
        value: contentItem.parent
    }
    Binding {
        when: contextDrawer !== undefined
        target: contextDrawer
        property: "parent"
        value: contentItem.parent
    }
    onPageStackChanged: pageStack.parent = contentItem;

    width: Units.gridUnit * 30
    height: Units.gridUnit * 45
    visible: true


    QtObject {
        id: internal
        property Item __passiveNotification
    }
}
