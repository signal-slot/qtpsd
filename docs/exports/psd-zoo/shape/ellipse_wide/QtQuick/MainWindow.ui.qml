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
        height: 102
        width: 262
        x: 19
        y: 49
        ShapePath {
            fillColor: "#ffff8000"
            strokeColor: "transparent"
            strokeWidth: 0
            PathAngleArc {
                centerX: 131
                centerY: 51
                radiusX: 130
                radiusY: 50
                startAngle: 0
                sweepAngle: 360
            }
        }
    }
}
