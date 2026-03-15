import QtQuick

Item {
    height: 200
    width: 200
    Item {
        id: _blend_bg_2
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
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/base.png"
            width: 200
            x: 0
            y: 0
        }
    }
    Item {
        id: _blend_fg_2
        anchors.fill: parent
        layer.enabled: true
        opacity: 0
        visible: true
        Item {
            anchors.fill: parent
            layer.enabled: true
            Item {
                id: _blend_bg_1
                anchors.fill: parent
                layer.enabled: true
                opacity: 0
                visible: true
            }
            Item {
                id: _blend_fg_1
                anchors.fill: parent
                layer.enabled: true
                opacity: 0
                visible: true
                Item {
                    anchors.fill: parent
                    layer.enabled: true
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
                            height: 200
                            layer.enabled: true
                            source: "images/content.png"
                            width: 200
                            x: 0
                            y: 0
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
            ShaderEffect {
                anchors.fill: parent
                fragmentShader: "blend.frag.qsb"
                property int blendMode: 0
                property var background: _blend_bg_1
                property var source: _blend_fg_1
            }
        }
    }
    ShaderEffect {
        anchors.fill: parent
        fragmentShader: "blend.frag.qsb"
        property int blendMode: 1
        property var background: _blend_bg_2
        property var source: _blend_fg_2
    }
}
