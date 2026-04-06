import QtQuick

Item {
    height: 300
    width: 400
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 400
        x: 0
        y: 0
    }
    Item {
        anchors.fill: parent
        Image {
            fillMode: Image.PreserveAspectFit
            height: 300
            source: "images/bg_color.png"
            width: 400
            x: 0
            y: 0
        }
    }
    Item {
        anchors.fill: parent
        Item {
            height: 242
            width: 302
            x: 49
            y: 29
            Rectangle {
                color: "#ffffffff"
                height: 240
                width: 300
                x: 1
                y: 1.00001
            }
        }
        Text {
            clip: true
            color: "#ff1e1e1e"
            font.bold: true
            font.family: "Roboto"
            font.pixelSize: 20
            height: 26
            horizontalAlignment: Text.AlignLeft
            text: "Card Title"
            verticalAlignment: Text.AlignBottom
            width: 87
            x: 80
            y: 59
        }
        Text {
            clip: true
            color: "#ff505050"
            font.family: "Roboto"
            font.pixelSize: 12
            height: 121
            horizontalAlignment: Text.AlignLeft
            text: "This is body text in a card layout with multiple layers and groups."
            verticalAlignment: Text.AlignBottom
            width: 250
            wrapMode: Text.Wrap
            x: 80
            y: 109
        }
    }
}
