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
    Image {
        fillMode: Image.PreserveAspectFit
        height: 100
        source: "images/raster_circle.png"
        width: 100
        x: 50
        y: 50
    }
    Text {
        clip: true
        color: "#ff000000"
        font.bold: true
        font.family: "Roboto"
        font.pixelSize: 28
        height: 37
        horizontalAlignment: Text.AlignLeft
        text: "Mixed Content"
        verticalAlignment: Text.AlignBottom
        width: 183
        x: 50
        y: 221
    }
    Item {
        height: 152
        width: 172
        x: 199
        y: 49
        Rectangle {
            color: "#ff0064c8"
            height: 150
            width: 170
            x: 1
            y: 1.00001
        }
    }
}
