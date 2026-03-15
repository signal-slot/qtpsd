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
        height: 120
        layer.enabled: true
        opacity: 0.501961
        source: "images/knockout_shadow.png"
        width: 120
        x: 40
        y: 40
        layer.effect: MultiEffect {
            shadowBlur: 15
            shadowColor: "#bf000000"
            shadowEnabled: true
            shadowHorizontalOffset: -7.5
            shadowOpacity: 0.749996
            shadowVerticalOffset: 12.9904
        }
    }
}
