import QtQuick
import QtQuick.Effects

Item {
    height: 300
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        layer.enabled: true
        source: "images/multi_effects.png"
        width: 200
        x: 50
        y: 50
        layer.effect: MultiEffect {
            shadowBlur: 10
            shadowColor: "#bf000000"
            shadowEnabled: true
            shadowHorizontalOffset: -2.5
            shadowOpacity: 0.749996
            shadowVerticalOffset: 4.33013
        }
    }
}
