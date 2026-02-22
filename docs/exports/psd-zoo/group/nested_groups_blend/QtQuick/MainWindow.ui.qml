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
        source: "images/base.png"
        width: 200
        x: 0
        y: 0
    }
    Item {
        anchors.fill: parent
        layer.enabled: true
        property string blendMode: "screen"
        Item {
            anchors.fill: parent
            layer.enabled: true
            property string blendMode: "multiply"
            Image {
                fillMode: Image.PreserveAspectFit
                height: 200
                property string blendMode: "multiply"
                source: "images/content.png"
                width: 200
                x: 0
                y: 0
            }
        }
    }
}
