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
    Item {
        height: 162
        layer.enabled: true
        width: 162
        x: 19
        y: 19
        Image {
            id: _rmask_0
            fillMode: Image.PreserveAspectFit
            height: 162
            layer.enabled: true
            source: "images/dual_mask_mask.png"
            visible: false
            width: 162
        }
        Rectangle {
            color: "#ffff0000"
            height: 160
            width: 160
            x: 1
            y: 1
        }
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: _rmask_0
        }
    }
}
