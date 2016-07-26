/*
 *   Copyright 2016 Marco Martin <mart@kde.org>
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

import QtQuick 2.5
import QtQuick.Layouts 1.2
import org.kde.kirigami 1.0

Item {
    id: root

//BEGIN PROPERTIES
    /**
     * This property holds the number of items currently pushed onto the view
     */
    readonly property alias depth: pagesLogic.count

    /**
     * The last Page in the Row
     */
    readonly property Item lastItem: pagesLogic.count ? pagesLogic.get(pagesLogic.count - 1).page : null

    /**
     * The currently visible Item
     */
    readonly property Item currentItem: mainFlickable.currentItem

    /**
     * the index of the currently visible Item
     */
    property alias currentIndex: mainFlickable.currentIndex

    /**
     * The initial item when this PageRow is created
     */
    property variant initialPage

    /**
     * The main flickable of this Row
     */
    property alias contentItem: mainFlickable

    /**
     * The default width for a column
     * default is wide enough for 30 grid units.
     * Pages can override it with their Layout.fillWidth,
     * implicitWidth Layout.minimumWidth etc.
     */
    property int defaultColumnWidth: Units.gridUnit * 20

    /**
     * interactive: bool
     * If true it will be possible to go back/forward by dragging the
     * content themselves with a gesture.
     * Otherwise the only way to go back will be programmatically
     * default: true
     */
    property alias interactive: mainFlickable.interactive

//END PROPERTIES

//BEGIN FUNCTIONS
    /**
     * Pushes a page on the stack.
     * The page can be defined as a component, item or string.
     * If an item is used then the page will get re-parented.
     * If a string is used then it is interpreted as a url that is used to load a page 
     * component.
     *
     * @param page The page can also be given as an array of pages.
     *     In this case all those pages will
     *     be pushed onto the stack. The items in the stack can be components, items or
     *     strings just like for single pages.
     *     Additionally an object can be used, which specifies a page and an optional
     *     properties property.
     *     This can be used to push multiple pages while still giving each of
     *     them properties.
     *     When an array is used the transition animation will only be to the last page.
     *
     * @param properties The properties argument is optional and allows defining a
     * map of properties to set on the page.
     * @return The new created page
     */
    function push(page, properties) {
        pop(currentItem);

        // figure out if more than one page is being pushed
        var pages;
        if (page instanceof Array) {
            pages = page;
            page = pages.pop();
            if (page.createObject === undefined && page.parent === undefined && typeof page != "string") {
                properties = properties || page.properties;
                page = page.page;
            }
        }

        // push any extra defined pages onto the stack
        if (pages) {
            var i;
            for (i = 0; i < pages.length; i++) {
                var tPage = pages[i];
                var tProps;
                if (tPage.createObject === undefined && tPage.parent === undefined && typeof tPage != "string") {
                    tProps = tPage.properties;
                    tPage = tPage.page;
                }

                var container = pagesLogic.initPage(tPage, tProps);
                pagesLogic.append(container);
            }
        }

        // initialize the page
        var container = pagesLogic.initPage(page, properties);
        pagesLogic.append(container);
        container.visible = container.page.visible = true;

        mainFlickable.currentIndex = container.level;
        return container.page
    }

    /**
     * Pops a page off the stack.
     * @param page If page is specified then the stack is unwound to that page,
     * to unwind to the first page specify
     * page as null.
     * @return The page instance that was popped off the stack.
     */
    function pop(page) {
        if (depth == 0) {
            return;
        }

        var oldPage = pagesLogic.get(pagesLogic.count-1).page;
        if (page !== undefined) {
            // an unwind target has been specified - pop until we find it
            while (page != oldPage && pagesLogic.count > 1) {
                pagesLogic.remove(oldPage.parent.level);

                oldPage = pagesLogic.get(pagesLogic.count-1).page;
            }
        } else {
            pagesLogic.remove(pagesLogic.count-1);
        }
    }

    /**
     * Replaces a page on the stack.
     * @param page The page can also be given as an array of pages.
     *     In this case all those pages will
     *     be pushed onto the stack. The items in the stack can be components, items or
     *     strings just like for single pages.
     *     Additionally an object can be used, which specifies a page and an optional
     *     properties property.
     *     This can be used to push multiple pages while still giving each of
     *     them properties.
     *     When an array is used the transition animation will only be to the last page.
     * @param properties The properties argument is optional and allows defining a
     * map of properties to set on the page.
     * @see push() for details.
     */
    function replace(page, properties) {
        if (currentIndex>=1)
            pop(pagesLogic.get(currentIndex-1).page);
        else if (currentIndex==0)
            pop();
        else
            console.warn("There's no page to replace");
        return push(page, properties);
    }

    /**
     * Clears the page stack.
     * Destroy (or reparent) all the pages contained.
     */
    function clear() {
        return pagesLogic.clear();
    }

    function get(idx) {
        return pagesLogic.get(idx).page;
    }

//END FUNCTIONS

    QtObject {
        id: pagesLogic

        readonly property int count: mainLayout.children.length
        property var componentCache

        property int roundedDefaultColumnWidth: Math.floor(root.width/root.defaultColumnWidth) > 0 ? root.width/Math.floor(root.width/root.defaultColumnWidth) : root.width

        //NOTE:seems to only work if the array is defined in a declarative way,
        //the Object in an imperative way, espacially on Android
        Component.onCompleted: {
            componentCache = {};
        }

        //TODO: remove?
        function get(id) {
            return mainLayout.children[id];
        }

        function append(item) {
            //FIXME: seems that for one loop the x of the item would continue to be 0
            item.x = item.level * roundedDefaultColumnWidth;
            item.parent = mainLayout;
        }

        function clear () {
            while (mainLayout.children.length > 0) {
                remove(0);
            }
        }

        function remove(id) {
            if (id < 0 || id >= count) {
                print("Tried to remove an invalid page index:" + id);
                return;
            }

            var item = mainLayout.children[id];
            if (item.owner) {
                item.page.parent = item.owner;
            }
            //FIXME: why reparent ing is necessary?
            //is destroy just an async deleteLater() that isn't executed immediately or it actually leaks?
            item.parent = root;
            item.destroy();
        }

        function initPage(page, properties) {
            var container = containerComponent.createObject(mainLayout, {
                "level": pagesLogic.count,
                "page": page
            });

            var pageComp;
            if (page.createObject) {
                // page defined as component
                pageComp = page;
            } else if (typeof page == "string") {
                // page defined as string (a url)
                pageComp = pagesLogic.componentCache[page];
                if (!pageComp) {
                    pageComp = pagesLogic.componentCache[page] = Qt.createComponent(page);
                }
            }
            if (pageComp) {
                if (pageComp.status == Component.Error) {
                    throw new Error("Error while loading page: " + pageComp.errorString());
                } else {
                    // instantiate page from component
                    page = pageComp.createObject(container.pageParent, properties || {});
                }
            } else {
                // copy properties to the page
                for (var prop in properties) {
                    if (properties.hasOwnProperty(prop)) {
                        page[prop] = properties[prop];
                    }
                }
            }

            container.page = page;
            if (page.parent == null || page.parent == container.pageParent) {
                container.owner = null;
            } else {
                container.owner = page.parent;
            }

            // the page has to be reparented
            if (page.parent != container) {
                page.parent = container;
            }

            return container;
        }
    }

    NumberAnimation {
        id: scrollAnim
        target: mainFlickable
        property: "contentX"
        duration: Units.longDuration
        easing.type: Easing.InOutQuad
    }

    Flickable {
        id: mainFlickable
        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: mainLayout.childrenRect.width
        contentHeight: height
        readonly property Item currentItem: {
            var idx = Math.min(currentIndex, pagesLogic.count-1)
            return idx>=0 ? pagesLogic.get(idx).page : null
        }
        //clip only when the app has a sidebar
        clip: root.x > 0

        property int currentIndex: 0
        property int firstVisibleLevel: Math.round (contentX / pagesLogic.roundedDefaultColumnWidth)

        flickDeceleration: Units.gridUnit * 50
        onCurrentItemChanged: {
            var itemX = pagesLogic.roundedDefaultColumnWidth * currentIndex;

            if (itemX >= contentX && mainFlickable.currentItem && itemX + mainFlickable.currentItem.width <= contentX + mainFlickable.width) {
                return;
            }

            //this catches 0 and NaN (sometimes at startup width can oddly be nan
            if (!mainFlickable.width) {
                return;
            }
            scrollAnim.running = false;
            scrollAnim.from = contentX;
            if (itemX < contentX || !mainFlickable.currentItem) {
                scrollAnim.to = Math.max(0, Math.min(itemX, mainFlickable.contentWidth - mainFlickable.width));
            } else {
                scrollAnim.to = Math.max(0, Math.min(itemX - mainFlickable.width + mainFlickable.currentItem.width, mainFlickable.contentWidth - mainFlickable.width));
            }
            scrollAnim.running = true;
        }
        onMovementEnded: {
            if (mainLayout.childrenRect.width == 0) {
                return;
            }

            scrollAnim.running = false;
            scrollAnim.from = contentX;
            scrollAnim.to = pagesLogic.roundedDefaultColumnWidth * firstVisibleLevel
            scrollAnim.running = true;

            var mappedCurrentItemPos = currentItem.mapToItem(mainFlickable, 0, 0);

            //is the current item out of view?
            if (mappedCurrentItemPos.x < 0) {
                currentIndex = firstVisibleLevel;
            } else if (mappedCurrentItemPos.x + currentItem.width > mainFlickable.width) {
                currentIndex = Math.min(root.depth-1, firstVisibleLevel + Math.floor(mainFlickable.width/pagesLogic.roundedDefaultColumnWidth)-1);
            }
        }
        onFlickEnded: movementEnded();

        Row {
            id: mainLayout
            add: Transition {
                NumberAnimation {
                    property: "y"
                    from: mainFlickable.height
                    to: 0
                    duration: Units.shortDuration
                    easing.type: Easing.InOutQuad
                }
            }
            onWidthChanged: {
                //current item in view
                if (children[mainFlickable.currentIndex].x >= mainFlickable.contentX &&
                    children[mainFlickable.currentIndex].x + children[mainFlickable.currentIndex].width <= mainFlickable.contentX + mainFlickable.width) {
                    mainFlickable.contentX = pagesLogic.roundedDefaultColumnWidth * mainFlickable.firstVisibleLevel;
                } else {
                    mainFlickable.contentX = Math.max(0, Math.min(width - mainFlickable.width, mainFlickable.currentIndex * pagesLogic.roundedDefaultColumnWidth));
                }
                
            }
            //onChildrenChanged: mainFlickable.contentX = pagesLogic.roundedDefaultColumnWidth * mainFlickable.firstVisibleLevel
        }
    }

    Rectangle {
        height: Units.smallSpacing
        width: root.width * root.width/mainLayout.width
        anchors.bottom: parent.bottom
        color: Theme.textColor
        opacity: 0
        x: root.width * mainFlickable.visibleArea.xPosition
        onXChanged: {
            opacity = 0.3
            scrollIndicatorTimer.restart();
        }
        Behavior on opacity {
            OpacityAnimator {
                duration: Units.longDuration
                easing.type: Easing.InOutQuad
            }
        }
        Timer {
            id: scrollIndicatorTimer
            interval: Units.longDuration * 4
            onTriggered: parent.opacity = 0;
        }
    }

    Component {
        id: containerComponent

        MouseArea {
            id: container
            height: mainFlickable.height
            width: root.width
            state: pendingState
            property string pendingState: root.width < root.defaultColumnWidth*2 ? "vertical" : (container.level >= pagesLogic.count - 1 ? "last" : "middle");

            //HACK
            onPendingStateChanged: {
                stateTimer.restart();
            }
            Timer {
                id: stateTimer
                interval: 150
                onTriggered: container.state = container.pendingState
            }

            property int level

            property int hint: page && page.implicitWidth ? page.implicitWidth : root.defaultColumnWidth
            property int roundedHint: Math.floor(root.width/hint) > 0 ? root.width/Math.floor(root.width/hint) : root.width

            property Item page
            property Item owner
            onPageChanged: {
                page.parent = container;
                page.anchors.fill = container;
            }
            drag.filterChildren: true
            onClicked: root.currentIndex = level;
            onFocusChanged: {
                if (focus) {
                    root.currentIndex = level;
                }
            }

            Rectangle {
                z: 999
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    right: parent.right
                }
                width: Math.ceil(Units.smallSpacing / 5)
                color: Theme.textColor
                opacity: 0.3
                visible: container.level < root.depth-1
            }
            states: [
                State {
                    name: "vertical"
                    PropertyChanges {
                        target: container
                        width: root.width
                    }
                },
                State {
                    name: "last"
                    PropertyChanges {
                        target: container
                        width: Math.max(roundedHint, root.width - (container.level == 0 ? 0 : pagesLogic.get(container.level-1).width))
                    }
                },
                State {
                    name: "middle"
                    PropertyChanges {
                        target: container
                        width: pagesLogic.roundedDefaultColumnWidth
                    }
                }
            ]
            transitions: [
                Transition {
                    from: "last,middle"
                    to: "middle,last"
                    SequentialAnimation {
                        NumberAnimation {
                            property: "width"
                            duration: Units.longDuration
                            easing.type: Easing.InOutQuad
                        }
                        ScriptAction {
                           script: mainFlickable.currentItemChanged();
                        }
                    }
                }
            ]
        }
    }

    Component.onCompleted: {
        if (initialPage) {
            push(initialPage, null)
        }
    }
}
