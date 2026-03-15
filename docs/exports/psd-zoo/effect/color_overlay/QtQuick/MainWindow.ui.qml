import QtQuick
import QtQuick.Effects

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
    Image {
        fillMode: Image.PreserveAspectFit
        height: 100
        layer.enabled: true
        source: "images/color_overlay_layer.png"
        width: 200
        x: 50
        y: 50
        layer.effect: MultiEffect {
            colorization: 1
            colorizationColor: "#ffff0080"
            opacity: 1
        }
    }
}
