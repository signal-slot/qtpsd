import QtQuick

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
        opacity: 0.501961
        property string blendMode: "dissolve"
        source: "images/dissolve_layer.png"
        width: 200
        x: 0
        y: 0
    }
}
