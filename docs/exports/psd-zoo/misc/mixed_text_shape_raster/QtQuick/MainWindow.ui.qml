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
        color: "#000000"
        height: 22
        text: ""
        width: 182
        x: 51
        y: 229
    }
    Item {
        height: 152
        width: 172
        x: 199
        y: 49
        Rectangle {
            color: "#000000"
            height: 150
            width: 170
            x: 1
            y: 1.00001
        }
    }
}
