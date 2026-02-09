import QtQuick

Item {
    height: 240
    width: 320
    Item {
        anchors.fill: parent
        Rectangle {
            color: "#f9cff4"
            height: 240
            width: 320
            x: 0
            y: 0
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 46
            source: "images/qtquick.png"
            width: 64
            x: 40
            y: 80
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 48
            source: "images/qtquick_1.png"
            width: 36
            x: 58
            y: 16
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 46
            source: "images/7c7c45584dc3695d.png"
            width: 64
            x: 40
            y: 150
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 48
            source: "images/slint.png"
            width: 36
            x: 150
            y: 79
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 48
            source: "images/55f66e81a5e64db8.png"
            width: 36
            x: 150
            y: 149
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 40
            source: "images/flutter.png"
            width: 32
            x: 240
            y: 82
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 40
            source: "images/flutter_pixeled.png"
            width: 32
            x: 240
            y: 152
        }
    }
}
