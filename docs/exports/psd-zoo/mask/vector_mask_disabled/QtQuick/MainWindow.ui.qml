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
    Item {
        height: 200
        width: 200
        x: 0
        y: 0
        Rectangle {
            color: "#00c800"
            height: 140
            width: 140
            x: 30
            y: 30
        }
    }
}
