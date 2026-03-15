import QtQuick
import QtQuick.Effects

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
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        layer.enabled: true
        source: "images/with_disabled_effect.png"
        width: 200
        x: 0
        y: 0
        layer.effect: MultiEffect {
            shadowBlur: 10
            shadowColor: "#bf000000"
            shadowEnabled: true
            shadowHorizontalOffset: -5
            shadowOpacity: 0.749996
            shadowVerticalOffset: 8.66025
        }
    }
}
