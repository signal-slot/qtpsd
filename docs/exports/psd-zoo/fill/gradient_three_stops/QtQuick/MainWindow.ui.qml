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
    Rectangle {
        height: 200
        width: 300
        x: 0
        y: 0
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                color: "#ffff0000"
                position: 1
            }
            GradientStop {
                color: "#ff00ff00"
                position: 0.5
            }
            GradientStop {
                color: "#ff0000ff"
                position: 0
            }
        }
    }
}
