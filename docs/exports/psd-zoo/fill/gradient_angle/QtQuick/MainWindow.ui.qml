import QtQuick
import QtQuick.Shapes

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
    Shape {
        height: 200
        width: 200
        x: 0
        y: 0
        ShapePath {
            strokeColor: "transparent"
            strokeWidth: 0
            fillGradient: ConicalGradient {
                angle: 0
                centerX: 100
                centerY: 100
                GradientStop {
                    color: "#ff008000"
                    position: 0
                }
                GradientStop {
                    color: "#ffffff00"
                    position: 1
                }
            }
            PathMove {
                x: 0
                y: 0
            }
            PathLine {
                x: 200
                y: 0
            }
            PathLine {
                x: 200
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
