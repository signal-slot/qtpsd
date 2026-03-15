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
        height: 140
        layer.enabled: true
        source: "images/styled_1.png"
        width: 80
        x: 10
        y: 30
        layer.effect: MultiEffect {
            shadowBlur: 5
            shadowColor: "#bf000000"
            shadowEnabled: true
            shadowHorizontalOffset: -1.5
            shadowOpacity: 0.749996
            shadowVerticalOffset: 2.59808
        }
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 140
        layer.enabled: true
        source: "images/styled_2.png"
        width: 80
        x: 100
        y: 30
        layer.effect: MultiEffect {
            shadowBlur: 5
            shadowColor: "#bf000000"
            shadowEnabled: true
            shadowHorizontalOffset: -1.5
            shadowOpacity: 0.749996
            shadowVerticalOffset: 2.59808
        }
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 140
        layer.enabled: true
        source: "images/styled_3.png"
        width: 80
        x: 190
        y: 30
        layer.effect: MultiEffect {
            shadowBlur: 5
            shadowColor: "#bf000000"
            shadowEnabled: true
            shadowHorizontalOffset: -1.5
            shadowOpacity: 0.749996
            shadowVerticalOffset: 2.59808
        }
    }
}
