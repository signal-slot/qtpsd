import QtQuick
import QtQuick.Shapes

Item {
    height: 200
    width: 300
    Shape {
        height: 200
        width: 300
        x: 0
        y: 0
        ShapePath {
            strokeColor: "transparent"
            strokeWidth: 0
            fillGradient: LinearGradient {
                x1: 300
                x2: 0
                y1: 100
                y2: 100
                GradientStop {
                    color: "#ff0000"
                    position: 0
                }
                GradientStop {
                    color: "#ff0000"
                    position: 1
                }
            }
            PathMove {
                x: 0
                y: 0
            }
            PathLine {
                x: 300
                y: 0
            }
            PathLine {
                x: 300
                y: 200
            }
            PathLine {
                x: 0
                y: 200
            }
            PathLine {
                x: 0
                y: 0
            }
        }
    }
}
