import QtQuick

Item {
    height: 200
    width: 300
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
                color: "#00ff0000"
                position: 0
            }
        }
    }
}
