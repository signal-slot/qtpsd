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
            height: 114
            opacity: 0.368627
            width: 153
            x: 54
            y: 65
            ShapePath {
                fillColor: "#5a6f0e"
                strokeColor: "#e5fc14"
                strokeWidth: 17
            }
            ShapePath {
                fillColor: "#5a6f0e"
                fillRule: ShapePath.WindingFill
                strokeColor: "#e5fc14"
                strokeWidth: 17
                PathMove {
                    x: 76.7117
                    y: 11.0005
                }
                PathCubic {
                    control1X: 76.7117
                    control1Y: 11.0005
                    control2X: 129.154
                    control2Y: 29.6605
                    x: 129.154
                    y: 29.6605
                }
                PathCubic {
                    control1X: 129.154
                    control1Y: 29.6605
                    control2X: 141.937
                    control2Y: 71.1084
                    x: 141.937
                    y: 71.1084
                }
                PathCubic {
                    control1X: 141.937
                    control1Y: 71.1084
                    control2X: 105.436
                    control2Y: 104.133
                    x: 105.436
                    y: 104.133
                }
                PathCubic {
                    control1X: 105.436
                    control1Y: 104.133
                    control2X: 47.1361
                    control2Y: 103.866
                    x: 47.1361
                    y: 103.866
                }
                PathCubic {
                    control1X: 47.1361
                    control1Y: 103.866
                    control2X: 10.9386
                    control2Y: 70.5087
                    x: 10.9386
                    y: 70.5087
                }
                PathCubic {
                    control1X: 10.9386
                    control1Y: 70.5087
                    control2X: 24.101
                    control2Y: 29.1796
                    x: 24.101
                    y: 29.1796
                }
                PathCubic {
                    control1X: 24.101
                    control1Y: 29.1796
                    control2X: 76.7117
                    control2Y: 11.0005
                    x: 76.7117
                    y: 11.0005
                }
            }
        }
        Shape {
            height: 95
            opacity: 0.368627
            width: 133
            x: 170
            y: 75
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
                    y: 0.999999
                }
                PathCubic {
                    control1X: 66.5
                    control1Y: 0.999999
                    control2X: 119.027
                    control2Y: 19.4198
                    x: 119.027
                    y: 19.4198
                }
                PathCubic {
                    control1X: 119.027
                    control1Y: 19.4198
                    control2X: 132
                    control2Y: 60.8087
                    x: 132
                    y: 60.8087
                }
                PathCubic {
                    control1X: 132
                    control1Y: 60.8087
                    control2X: 95.6502
                    control2Y: 94
                    x: 95.6502
                    y: 94
                }
                PathCubic {
                    control1X: 95.6502
                    control1Y: 94
                    control2X: 37.3498
                    control2Y: 94
                    x: 37.3498
                    y: 94
                }
                PathCubic {
                    control1X: 37.3498
                    control1Y: 94
                    control2X: 1
                    control2Y: 60.8087
                    x: 1
                    y: 60.8087
                }
                PathCubic {
                    control1X: 1
                    control1Y: 60.8087
                    control2X: 13.9731
                    control2Y: 19.4198
                    x: 13.9731
                    y: 19.4198
                }
                PathCubic {
                    control1X: 13.9731
                    control1Y: 19.4198
                    control2X: 66.5
                    control2Y: 0.999999
                    x: 66.5
                    y: 0.999999
                }
            }
        }
    }
}
