import QtQuick

Item {
    height: 200
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/dark_bg.png"
        width: 300
        x: 0
        y: 0
    }
    Text {
        color: "#ffffff"
        font.family: "Roboto"
        font.pixelSize: 36
        height: 41
        horizontalAlignment: Text.AlignLeft
        text: "Shadow"
        width: 129
        x: 30
        y: 82
    }
}
