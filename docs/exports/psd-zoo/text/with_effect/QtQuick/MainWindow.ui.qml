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
    Text {
        color: "#ffc83232"
        font.bold: true
        font.family: "Roboto"
        font.pixelSize: 48
        height: 70
        horizontalAlignment: Text.AlignLeft
        layer.enabled: true
        text: "STYLED"
        verticalAlignment: Text.AlignBottom
        width: 173
        x: 20
        y: 84
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
