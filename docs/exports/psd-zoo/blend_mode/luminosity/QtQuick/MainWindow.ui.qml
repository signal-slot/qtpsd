import QtQuick

Item {
    height: 200
    width: 200
    Item {
        id: _blend_bg_0
        anchors.fill: parent
        layer.enabled: true
        opacity: 0
        visible: true
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/background.png"
            width: 200
            x: 0
            y: 0
        }
    }
    Item {
        id: _blend_fg_0
        anchors.fill: parent
        layer.enabled: true
        opacity: 0
        visible: true
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            layer.enabled: true
            source: "images/luminosity_layer.png"
            width: 200
            x: 0
            y: 0
        }
    }
    ShaderEffect {
        anchors.fill: parent
        fragmentShader: "blend.frag.qsb"
        property int blendMode: 24
        property var background: _blend_bg_0
        property var source: _blend_fg_0
    }
}
