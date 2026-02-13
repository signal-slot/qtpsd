import QtQuick

Item {
    height: 200
    width: 200
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 200
        x: 0
        y: 0
    }
    Text {
        anchors.fill: parent
        color: "#000000"
        font.family: "Sans Serif"
        font.pixelSize: 9
        horizontalAlignment: Text.AlignLeft
        text: ""
    }
}
