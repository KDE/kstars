import QtQuick 1.0
import QtWebKit 1.0

Rectangle
{
	id: rootRect
	width: 210
	height: 400

	WebView
	{
		settings.developerExtrasEnabled: true;
		id: webView
		visible: false
		preferredWidth: 640
		preferredHeight: 480
		z: 2
		onLoadFinished:
		{
			rootRect.width = width;
			rootRect.height = height;
		}
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
		text: webView.progress*100 + "%"
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.verticalCenter: parent.verticalCenter
		height: 30
		width: 100
	}

	ListView
	{
		height: parent.height - anchors.margins * 2
		width: parent.width - anchors.margins * 2
		visible: !webView.visible
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
				width: parent.parent.parent.width
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
					width: parent.width - 6
					anchors.margins: 3
					anchors.top: titleText.bottom
					anchors.left: parent.left
					source: path + "/thumbnail.png"
				}
				
				MouseArea
				{
					anchors.fill: parent

					onClicked:
					{
						webView.visible = true;
						webView.url = path + "/index.html"
					}
				}
			}
		}
	}
}
