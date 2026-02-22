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
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 32
        horizontalAlignment: Text.AlignLeft
        text: "Top"
        verticalAlignment: Text.AlignBottom
        width: 40
        x: 100
        y: 25
    }
    Text {
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 32
        horizontalAlignment: Text.AlignLeft
        text: "Middle"
        verticalAlignment: Text.AlignBottom
        width: 72
        x: 100
        y: 125
    }
    Text {
        color: "#ff000000"
        font.family: "Roboto"
        font.pixelSize: 24
        height: 32
        horizontalAlignment: Text.AlignLeft
        text: "Bottom"
        verticalAlignment: Text.AlignBottom
        width: 79
        x: 100
        y: 225
    }
}
