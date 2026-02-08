import QtQuick
import QtQuick.Shapes

Item {
    height: 240
    width: 320
    Item {
        anchors.fill: parent
        Rectangle {
            color: "#8fc5f3"
            height: 240
            width: 320
            x: 0
            y: 0
        }
        Shape {
            height: 96
            opacity: 0.368627
            width: 134
            x: 64
            y: 74
            ShapePath {
                fillColor: "#5a6f0e"
                strokeColor: "transparent"
                strokeWidth: 1
            }
            ShapePath {
                fillColor: "#5a6f0e"
                fillRule: ShapePath.WindingFill
                strokeColor: "transparent"
                strokeWidth: 1
                PathMove {
                    x: 66.5
                    y: 2
                }
                PathCubic {
                    control1X: 66.5
                    control1Y: 2
                    control2X: 119.027
                    control2Y: 20.4198
                    x: 119.027
                    y: 20.4198
                }
                PathCubic {
                    control1X: 119.027
                    control1Y: 20.4198
                    control2X: 132
                    control2Y: 61.8087
                    x: 132
                    y: 61.8087
                }
                PathCubic {
                    control1X: 132
                    control1Y: 61.8087
                    control2X: 95.6502
                    control2Y: 95
                    x: 95.6502
                    y: 95
                }
                PathCubic {
                    control1X: 95.6502
                    control1Y: 95
                    control2X: 37.3498
                    control2Y: 95
                    x: 37.3498
                    y: 95
                }
                PathCubic {
                    control1X: 37.3498
                    control1Y: 95
                    control2X: 1
                    control2Y: 61.8087
                    x: 1
                    y: 61.8087
                }
                PathCubic {
                    control1X: 1
                    control1Y: 61.8087
                    control2X: 13.9731
                    control2Y: 20.4198
                    x: 13.9731
                    y: 20.4198
                }
                PathCubic {
                    control1X: 13.9731
                    control1Y: 20.4198
                    control2X: 66.5
                    control2Y: 2
                    x: 66.5
                    y: 2
                }
            }
        }
    }
}
