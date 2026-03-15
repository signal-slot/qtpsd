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
    Rectangle {
        border.color: "#ffffff00"
        border.width: 2
        color: "transparent"
        height: 104
        layer.enabled: true
        width: 204
        x: 48
        y: 48
        Image {
            fillMode: Image.PreserveAspectFit
            height: 100
            layer.enabled: true
            source: "images/multi_effect.png"
            width: 200
            x: 2
            y: 2
            layer.effect: MultiEffect {
                colorization: 1
                colorizationColor: "#ffff8000"
                layer.enabled: true
                opacity: 0.501961
                layer.effect: MultiEffect {
                    shadowBlur: 5
                    shadowColor: "#bf000000"
                    shadowEnabled: true
                    shadowHorizontalOffset: -2.5
                    shadowOpacity: 0.749996
                    shadowVerticalOffset: 4.33013
                }
            }
        }
        layer.effect: MultiEffect {
            colorization: 1
            colorizationColor: "#ffff8000"
            layer.enabled: true
            opacity: 0.501961
            layer.effect: MultiEffect {
                shadowBlur: 5
                shadowColor: "#bf000000"
                shadowEnabled: true
                shadowHorizontalOffset: -2.5
                shadowOpacity: 0.749996
                shadowVerticalOffset: 4.33013
            }
        }
    }
}
