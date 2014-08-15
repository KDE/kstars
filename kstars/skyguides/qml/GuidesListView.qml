import QtQuick 1.0
import QtWebKit 1.0

Rectangle
{
	id: rootRect
	state: "listing"
	states:
	[
		State
		{
			name: "listing"
			PropertyChanges
			{
				target: presentationList
				visible: true
			}
			StateChangeScript
			{
				name: "resizeWidgetListing"
				script: widget.resizeScene( 210, 400 )
			}
			PropertyChanges
			{
				target: fullscreenButton
				state: "invisible"
			}
		},
		State
		{
			name: "normalPresentation"
			PropertyChanges
			{
				target: presentationList
				visible: false
			}
// QML WebView is buggy and go crazy when going fullscreen from inside QML
// 			StateChangeScript
// 			{
// 				name: "resizeWidgetNormal"
// 				script: widget.showNormal()
// 			}
			PropertyChanges
			{
				target: webView
				visible: true
			}
			PropertyChanges
			{
				target: fullscreenButton
// 				state: "ghost" // QML WebView is buggy and go crazy when going fullscreen from inside QML
				state: "invisible"
				source: "view-fullscreen.png"
			}
		},
		State
		{
			name: "fullscreenPresentation"
			PropertyChanges
			{
				target: presentationList
				visible: false
			}
			PropertyChanges
			{
				target: fullscreenButton
// 				state: "ghost" // QML WebView is buggy and go crazy when going fullscreen from inside QML
				state: "invisible"
				source: "view-restore.png"
			}
			StateChangeScript
			{
				name: "fullScreenDebug"
				script:
				{
					widget.resizeScene(widget.getScreenWidth(), widget.getScreenHeight())
					widget.showFullScreen()
				}
			}
		}
	]

	Image
	{
		id: fullscreenButton
		z: 3
		anchors.right: rootRect.right
		anchors.rightMargin: 5
		anchors.top: rootRect.top
		anchors.topMargin: 5

		state: "invisible"
		states:
		[
			State
			{
				name: "visible"
				PropertyChanges
				{
					target: fullscreenButton
					opacity: 1
					visible: true
				}
			},
			State
			{
				name: "ghost"
				PropertyChanges
				{
					target: fullscreenButton
					opacity: 0.01
					visible: true
				}
			},
			State
			{
				name: "invisible"
				PropertyChanges
				{
					target: fullscreenButton
					visible: false
				}
			}
		]

		MouseArea
		{
			anchors.fill: parent
			hoverEnabled : true
			onClicked: widget.fullScreen ? rootRect.state = "normalPresentation" : rootRect.state = "fullscreenPresentation"
			onEntered: parent.state = "visible"
			onExited: parent.state = "ghost"
		}

		Behavior on opacity { PropertyAnimation {} }
	}

	WebView
	{
		id: webView
		settings.developerExtrasEnabled: true
		settings.pluginsEnabled: true
		settings.localContentCanAccessRemoteUrls: true
		focus: webView.visible
		visible: false
		anchors.fill: parent
		preferredWidth: rootRect.width
		preferredHeight: rootRect.height
		javaScriptWindowObjects:
		[
			QtObject
			{
				WebView.windowObjectName: "skymap"
				function setDestinationBySkyObjectName(name) { skymap.setDestinationBySkyObjectName(name) }
				function setDestinationByCoordinates( ra, dec ) { skymap.setDestinationByCoordinates( ra, dec ) }
				function setFocusBySkyObjectName(name) { skymap.setFocusBySkyObjectName(name) }
				function setFocusByCoordinates( ra, dec ) { skymap.setFocusByCoordinates( ra, dec ) }
				function setZoomFactor(factor) { skymap.setZoomFactor(factor) }
				function zoomFactor() { skyMap.zoomFactor() }
				function zoomIn() { skymap.zoomIn() }
				function zoomOut() { skymap.zoomOut() }
				function zoomDefault() { skyMap.zoomDefault() }
				function forceUpdate() { skymap.forceUpdate() }
			},
			QtObject
			{
				WebView.windowObjectName: "skydata"
				function getSkyObjectRightAscention(name) { return skydata.getSkyObjectRightAscention(name) }
				function getSkyObjectDeclination(name) { return skydata.getSkyObjectDeclination(name) }
				function getSkyObjectMagnitude(name) { return skydata.getSkyObjectMagnitude(name) }
			}
		]
	}

	Text
	{
		id: progressText
		visible: ((webView.progress > 0.01) && (webView.progress < 1) )
		z:3
		color: "white"
		text: "Loading " + webView.progress*100 + "%"
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.verticalCenter: parent.verticalCenter
		height: 30
		width: 100
	}

	ListView
	{
		id: presentationList
		height: parent.height - anchors.margins * 2
		width: parent.width - anchors.margins * 2
		z: 1
		anchors.margins: 8
		anchors.top: parent.top
		anchors.left: parent.left

		spacing: 10
		model: guidesmodel
		delegate: Column
		{
			Rectangle
			{
				width: presentationList.width
				height: 150
				color: "grey"
				border.color: "black"
				border.width: 5
				radius: 10
				
				Text
				{
					anchors.top: parent.top
					anchors.left: parent.left
					anchors.margins: 6
					id: titleText
					text: title
					font.bold: true
				}
				
				Image
				{
					width: parent.width - 9
					height: parent.height - 40
					fillMode: Image.Stretch
					anchors.top: titleText.bottom
					anchors.topMargin: 3
					anchors.left: parent.left
					anchors.leftMargin: 5
					source: path + "/thumbnail.png"
				}
				
				MouseArea
				{
					anchors.fill: parent
					onClicked:
					{
						rootRect.state = "normalPresentation"
						widget.resizeScene(pWidth, pHeight)
						webView.url = path + "/index.html"
					}
				}
			}
		}
	}
}
