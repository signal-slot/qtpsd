import QtQuick
import QtQuick.Shapes

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
    Shape {
        height: 142
        width: 262
        x: 19
        y: 29
        ShapePath {
            fillColor: "#000000"
            strokeColor: "transparent"
            strokeWidth: 1
        }
        ShapePath {
            fillColor: "#000000"
            fillRule: ShapePath.OddEvenFill
            strokeColor: "transparent"
            strokeWidth: 1
            PathMove {
                x: 0.999999
                y: 0.999995
            }
            PathCubic {
                control1X: 0.999999
                control1Y: 0.999995
                control2X: 121
                control2Y: 0.999995
                x: 121
                y: 0.999995
            }
            PathCubic {
                control1X: 121
                control1Y: 0.999995
                control2X: 121
                control2Y: 141
                x: 121
                y: 141
            }
            PathCubic {
                control1X: 121
                control1Y: 141
                control2X: 0.999999
                control2Y: 141
                x: 0.999999
                y: 141
            }
            PathCubic {
                control1X: 0.999999
                control1Y: 141
                control2X: 0.999999
                control2Y: 0.999995
                x: 0.999999
                y: 0.999995
            }
            PathMove {
                x: 141
                y: 21
            }
            PathCubic {
                control1X: 141
                control1Y: 21
                control2X: 261
                control2Y: 21
                x: 261
                y: 21
            }
            PathCubic {
                control1X: 261
                control1Y: 21
                control2X: 261
                control2Y: 121
                x: 261
                y: 121
            }
            PathCubic {
                control1X: 261
                control1Y: 121
                control2X: 141
                control2Y: 121
                x: 141
                y: 121
            }
            PathCubic {
                control1X: 141
                control1Y: 121
                control2X: 141
                control2Y: 21
                x: 141
                y: 21
            }
        }
    }
}
