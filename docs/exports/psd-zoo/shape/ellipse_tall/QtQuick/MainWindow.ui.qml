import QtQuick
import QtQuick.Shapes

Item {
    height: 300
    width: 200
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 200
        x: 0
        y: 0
    }
    Shape {
        height: 262
        width: 102
        x: 49
        y: 19
        ShapePath {
            fillColor: "#ff00c880"
            strokeColor: "transparent"
            strokeWidth: 0
            PathAngleArc {
                centerX: 51
                centerY: 131
                radiusX: 50
                radiusY: 130
                startAngle: 0
                sweepAngle: 360
            }
        }
    }
}
