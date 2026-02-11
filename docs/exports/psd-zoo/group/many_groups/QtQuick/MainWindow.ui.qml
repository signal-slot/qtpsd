import QtQuick

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
    Item {
        anchors.fill: parent
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/layer_1.png"
            width: 60
            x: 0
            y: 0
        }
    }
    Item {
        anchors.fill: parent
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/layer_2.png"
            width: 60
            x: 60
            y: 0
        }
    }
    Item {
        anchors.fill: parent
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/layer_3.png"
            width: 60
            x: 120
            y: 0
        }
    }
    Item {
        anchors.fill: parent
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/layer_4.png"
            width: 60
            x: 180
            y: 0
        }
    }
    Item {
        anchors.fill: parent
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/layer_5.png"
            width: 60
            x: 240
            y: 0
        }
    }
}
