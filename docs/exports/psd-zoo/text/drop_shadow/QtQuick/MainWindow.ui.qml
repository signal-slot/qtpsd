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
        height: 200
        source: "images/dark_bg.png"
        width: 300
        x: 0
        y: 0
    }
    Text {
        clip: true
        color: "#ffffffff"
        font.family: "Roboto"
        font.pixelSize: 36
        height: 48
        horizontalAlignment: Text.AlignLeft
        layer.enabled: true
        text: "Shadow"
        verticalAlignment: Text.AlignBottom
        width: 129
        x: 30
        y: 82
        layer.effect: MultiEffect {
            shadowBlur: 8
            shadowColor: "#e6000000"
            shadowEnabled: true
            shadowHorizontalOffset: -2.82843
            shadowOpacity: 0.900008
            shadowVerticalOffset: 2.82843
        }
    }
}
