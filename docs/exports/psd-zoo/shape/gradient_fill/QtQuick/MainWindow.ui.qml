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
    Item {
        height: 142
        width: 142
        x: 29
        y: 29
        Shape {
            height: 140
            width: 140
            x: 0.999995
            y: 0.999995
            ShapePath {
                strokeColor: "transparent"
                strokeWidth: 0
                fillGradient: LinearGradient {
                    x1: 20.7954
                    x2: 121.205
                    y1: 121.205
                    y2: 20.7954
                    GradientStop {
                        color: "#ffff0000"
                        position: 0
                    }
                    GradientStop {
                        color: "#ff0000ff"
                        position: 1
                    }
                }
                PathMove {
                    x: 0
                    y: 0
                }
                PathLine {
                    x: 140
                    y: 0
                }
                PathLine {
                    x: 140
                    y: 140
                }
                PathLine {
                    x: 0
                    y: 140
                }
                PathLine {
                    x: 0
                    y: 0
                }
            }
        }
    }
}
