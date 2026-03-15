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
        source: "images/scaled_fx.png"
        width: 200
        x: 50
        y: 50
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
