import QtQuick

Item {
    height: 300
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 27
        horizontalAlignment: Text.AlignLeft
        text: "Top"
        width: 40
        x: 100
        y: 25
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 27
        horizontalAlignment: Text.AlignLeft
        text: "Middle"
        width: 72
        x: 100
        y: 125
    }
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 27
        horizontalAlignment: Text.AlignLeft
        text: "Bottom"
        width: 79
        x: 100
        y: 225
    }
}
