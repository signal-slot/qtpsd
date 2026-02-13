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
        height: 140
        source: "images/styled_1.png"
        width: 80
        x: 10
        y: 30
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 140
        source: "images/styled_2.png"
        width: 80
        x: 100
        y: 30
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 140
        source: "images/styled_3.png"
        width: 80
        x: 190
        y: 30
    }
}
