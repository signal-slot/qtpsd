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
        height: 87
        source: "images/First paragraph with indent. This is a longer text to test para"
        width: 338
        x: 20
        y: 20
    }
}
