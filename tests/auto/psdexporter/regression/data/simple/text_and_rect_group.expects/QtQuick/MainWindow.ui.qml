import QtQuick

Item {
    height: 240
    width: 320
    Item {
        id: 1
        height: 240
        width: 320
        x: 0
        y: 0
        Rectangle {
            color: "#ff5d5206"
            height: 240
            width: 320
            x: 0
            y: 0
        }
        Item {
            anchors.fill: parent
            Item {
                height: 62
                width: 163
                x: 48
                y: 79
                Rectangle {
                    color: "#fff5f5ed"
                    height: 60
                    width: 160
                    x: 2
                    y: 0.999995
                }
            }
            Item {
                height: 34
                width: 157
                x: 52
                y: 80
                Rectangle {
                    color: "#fff7e790"
                    height: 32
                    width: 155
                    x: 0.999992
                    y: 0.999994
                }
            }
            Image {
                fillMode: Image.PreserveAspectFit
                height: 32
                source: "images/example1.png"
                width: 155
                x: 53
                y: 81
            }
        }
    }
}
