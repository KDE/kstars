/***************************************************************************
                          SkyGuideView.qml  -  K Desktop Planetarium
                             -------------------
    begin                : 2015/07/01
    copyright            : (C) 2015 by Marcos Cardinot
    email                : mcardinot@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

ColumnLayout {
    id: view
    spacing: 0
    property int fontSizeText: 12
    property int frameBorderWidth: 1
    property int frameHMargin: 10
    property int frameVMargin: 10

    property var historyPrev: []
    property var historyNext: []
    property var currentPage: null
    property int maxHistLenght: 16

    signal addSkyGuide()

    function getPageObj(name, model, slide) {
        return {'name': name, 'modelData': model, 'slide': slide};
    }

    function loadPage(page) {
        if (!page.hasOwnProperty('name')
            || !page.hasOwnProperty('modelData')
            || !page.hasOwnProperty('slide')) {
            console.log("SkyGuideQML:loadPage(PAGE): PAGE should hold "
                        + "the properties: name; modelData; slide");
            return false;
        } else if (currentPage
                   && currentPage.name === page.name
                   && currentPage.modelData === page.modelData
                   && currentPage.slide === page.slide) {
            return false;
        }

        var src;
        if (page.name === "HOME") {
            menuHome.visible = true
            menuSlide.visible = false
            src = "SkyGuideHome.qml";
        } else if (page.name === "INFO") {
            btnPrevSlide.enabled = false
            btnNextSlide.enabled = true
            btnContents.enabled = false
            menuHome.visible = false
            menuSlide.visible = true
            src = "SkyGuideInfo.qml";
        } else if (page.name === "SLIDE") {
            btnNextSlide.enabled = page.slide < page.modelData.contents.length - 1;
            btnPrevSlide.enabled = page.slide > 0;
            btnContents.enabled = true
            menuHome.visible = false
            menuSlide.visible = true
            src = "SkyGuideSlide.qml";
        } else {
            console.log("SkyGuideQML: " + page.name + " is not a valid page!");
            return false;
        }

        if (page.modelData) {
            loader.modelData = page.modelData;
            loader.modelData.currentSlide = page.slide;
        }

        loader.source = "";
        loader.source = src;
        currentPage = page;

        return true;
    }

    function updateHistorySettings() {
        if (historyNext.length + historyPrev.length > maxHistLenght) {
            historyPrev.splice(0, 1); // remove the oldest one
        }
        btnPrev.enabled = historyPrev.length > 0;
        btnNext.enabled = historyNext.length > 0;
    }

    function goPrev() {
        var cPage = currentPage;
        var prevPageIdx = historyPrev.length - 1;
        if (prevPageIdx > -1 && loadPage(historyPrev[prevPageIdx])) {
            historyNext.push(cPage);
            historyPrev.splice(prevPageIdx, 1);
            updateHistorySettings();
        }
    }

    function goNext() {
        var cPage = currentPage;
        var nextPageIdx = historyNext.length - 1;
        if (nextPageIdx > -1 && loadPage(historyNext[nextPageIdx])) {
            historyNext.splice(nextPageIdx, 1);
            historyPrev.push(cPage);
            updateHistorySettings();
        }
    }

    function goToPage(newPage) {
        var cPage = currentPage;
        if (loadPage(newPage)) {
            historyNext = [];
            if (cPage) historyPrev.push(cPage);
            updateHistorySettings();
        }
    }

    ToolBar {
        Layout.alignment: Qt.AlignTop
        Layout.preferredWidth: 360
        Layout.fillWidth: true

        RowLayout {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            ToolButton {
                id: btnPrev
                iconSource: "images/prev.png"
                tooltip: "Goes to the previous page."
                onClicked: goPrev()
            }
            ToolButton {
                id: btnNext
                iconSource: "images/next.png"
                tooltip: "Goes to the next page."
                onClicked: goNext()
            }
            ToolButton {
                iconSource: "images/home.png"
                tooltip: "Goes to the homepage."
                onClicked: goToPage(getPageObj('HOME', null, -1))
            }
        }
    }

    Loader {
        id: loader
        Layout.alignment: Qt.AlignCenter
        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.minimumHeight: 360
        focus: true
        property var modelData: null
        Component.onCompleted: goToPage(getPageObj('HOME', null, -1))
    }

    ToolBar {
        id: menu
        Layout.alignment: Qt.AlignBottom
        Layout.preferredWidth: 360
        Layout.fillWidth: true

        RowLayout {
            id: menuHome
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            ToolButton {
                iconSource: "images/add.png"
                tooltip: "Adds a SkyGuide"
                onClicked: addSkyGuide()
            }
            ToolButton {
                iconSource: "images/write.png"
                tooltip: "Makes a SkyGuide"
            }
        }

        RowLayout {
            id: menuSlide
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            ToolButton {
                id: btnContents
                iconSource: "images/info.png"
                tooltip: "Contents"
                onClicked: goToPage(getPageObj('INFO', loader.modelData, -1))
            }
            ToolButton {
                id: btnPrevSlide
                iconSource: "images/prevSlide.png"
                tooltip: "Previous Slide"
                onClicked: {
                    var slide = loader.modelData.currentSlide - 1;
                    if (slide < 0) {
                        goToPage(getPageObj('INFO', loader.modelData, -1));
                    } else {
                        goToPage(getPageObj('SLIDE', loader.modelData, slide));
                    }
                }
            }
            ToolButton {
                id: btnNextSlide
                iconSource: "images/nextSlide.png"
                tooltip: "Next Slide"
                onClicked: {
                    goToPage(getPageObj('SLIDE', loader.modelData, loader.modelData.currentSlide + 1));
                }
            }
        }
    }
}
