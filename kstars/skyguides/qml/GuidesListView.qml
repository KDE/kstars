import QtQuick 1.0

Rectangle
{
	width: 210
	height: 400

	ListModel
	{
		id: guidesModel

		ListElement
		{
			name: "Jupiter Tempests"
			path: "file:///home/gioacchino/Development/kstars/kstars/skyguides/example_guides/jupiter_the_big/"
		}
		
		ListElement {
			name: "Jupiter the Big"
			path: "file:///home/gioacchino/Development/kstars/kstars/skyguides/example_guides/jupiter_documentary/"
		}
		
		ListElement
		{
			name: "Jupiter Eye"
			path: "file:///home/gioacchino/Development/kstars/kstars/skyguides/example_guides/jupiter_the_big/"
		}
	}

	ListView
	{
		height: parent.height - anchors.margins * 2
		width: parent.width - anchors.margins * 2
		anchors.margins: 8
		anchors.top: parent.top
		anchors.left: parent.left

		spacing: 10
		model: guidesModel
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
					text: name
					font.bold: true
				}
				
				Image
				{
					width: parent.width - 6
					anchors.margins: 3
					anchors.top: titleText.bottom
					anchors.left: parent.left
					source: path + "thumbnail.png"
				}
				
				MouseArea
				{
					
					onClicked: console.log("button clicked")
				}
			}
		}
	}
}
