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
        height: 140
        layer.enabled: true
        source: "images/trans_shapes.png"
        width: 140
        x: 30
        y: 30
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
