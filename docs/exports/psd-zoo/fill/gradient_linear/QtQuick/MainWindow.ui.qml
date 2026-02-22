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
    Rectangle {
        height: 200
        width: 200
        x: 0
        y: 0
        gradient: Gradient {
            GradientStop {
                color: "#ff000000"
                position: 1
            }
            GradientStop {
                color: "#ffffffff"
                position: 0
            }
        }
    }
}
