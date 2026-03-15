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
        source: "images/bg.png"
        width: 200
        x: 0
        y: 0
    }
    Item {
        anchors.fill: parent
        Item {
            id: _blend_bg_0
            anchors.fill: parent
            layer.enabled: true
            opacity: 0
            visible: true
        }
        Item {
            id: _blend_fg_0
            anchors.fill: parent
            layer.enabled: true
            opacity: 0
            visible: true
            Image {
                fillMode: Image.PreserveAspectFit
                height: 140
                layer.enabled: true
                source: "images/content.png"
                width: 140
                x: 30
                y: 30
            }
        }
        ShaderEffect {
            anchors.fill: parent
            fragmentShader: "blend.frag.qsb"
            property int blendMode: 0
            property var background: _blend_bg_0
            property var source: _blend_fg_0
        }
    }
}
