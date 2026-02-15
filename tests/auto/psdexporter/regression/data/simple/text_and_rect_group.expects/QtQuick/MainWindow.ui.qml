import QtQuick

Item {
    height: 240
    width: 320
    Item {
        anchors.fill: parent
        Rectangle {
            color: "#5d5206"
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
                    color: "#f5f5ed"
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
                    color: "#f7e790"
                    height: 32
                    width: 155
                    x: 0.999992
                    y: 0.999994
                }
            }
            Text {
                color: "#000000"
                font.family: "Source Han Sans"
                font.pixelSize: 30
                height: 33
                horizontalAlignment: Text.AlignHCenter
                text: "Example1"
                width: 160
                x: 50
                y: 79
            }
        }
    }
}
