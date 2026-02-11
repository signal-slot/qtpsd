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
    Item {
        anchors.fill: parent
        Image {
            fillMode: Image.PreserveAspectFit
            height: 40
            source: "images/layer_1.png"
            width: 40
            x: 0
            y: 0
        }
    }
    Item {
        anchors.fill: parent
        Image {
            fillMode: Image.PreserveAspectFit
            height: 40
            source: "images/layer_2.png"
            width: 40
            x: 30
            y: 30
        }
    }
    Item {
        anchors.fill: parent
        Image {
            fillMode: Image.PreserveAspectFit
            height: 40
            source: "images/layer_3.png"
            width: 40
            x: 60
            y: 60
        }
    }
    Item {
        anchors.fill: parent
        Image {
            fillMode: Image.PreserveAspectFit
            height: 40
            source: "images/layer_4.png"
            width: 40
            x: 90
            y: 90
        }
    }
    Item {
        anchors.fill: parent
        Image {
            fillMode: Image.PreserveAspectFit
            height: 40
            source: "images/layer_5.png"
            width: 40
            x: 120
            y: 120
        }
    }
}
