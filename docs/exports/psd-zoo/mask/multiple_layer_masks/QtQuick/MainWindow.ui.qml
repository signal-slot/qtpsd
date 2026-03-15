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
        height: 200
        layer.enabled: true
        source: "images/red_masked.png"
        width: 200
        x: 0
        y: 0
        Image {
            id: _rmask_0
            fillMode: Image.PreserveAspectFit
            height: 200
            layer.enabled: true
            source: "images/red_masked_mask.png"
            visible: false
            width: 200
        }
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: _rmask_0
        }
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        layer.enabled: true
        source: "images/blue_masked.png"
        width: 200
        x: 0
        y: 0
        Image {
            id: _rmask_1
            fillMode: Image.PreserveAspectFit
            height: 200
            layer.enabled: true
            source: "images/blue_masked_mask.png"
            visible: false
            width: 200
        }
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: _rmask_1
        }
    }
}
