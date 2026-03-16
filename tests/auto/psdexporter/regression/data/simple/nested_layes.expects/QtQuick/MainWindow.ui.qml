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
            color: "#fff7e790"
            height: 240
            width: 320
            x: 0
            y: 0
        }
        Item {
            anchors.fill: parent
            Item {
                height: 52
                width: 182
                x: 39
                y: 49
                Rectangle {
                    color: "#ffd4f5c9"
                    height: 50
                    radius: 20
                    width: 180
                    x: 1
                    y: 0.999995
                }
            }
            Item {
                anchors.fill: parent
                Item {
                    height: 13
                    width: 139
                    x: 72
                    y: 89
                    Rectangle {
                        color: "#ff217903"
                        height: 10
                        width: 137
                        x: 0.999992
                        y: 1
                    }
                }
                Item {
                    height: 14
                    width: 25
                    x: 79
                    y: 81
                    Rectangle {
                        color: "#ff5e88e7"
                        height: 12
                        width: 23
                        x: 1
                        y: 0.999993
                    }
                }
            }
            Text {
                clip: true
                color: "#ff000000"
                font.family: "Source Han Sans"
                font.pixelSize: 30
                font.weight: 500
                height: 44
                horizontalAlignment: Text.AlignHCenter
                text: "Example1"
                verticalAlignment: Text.AlignVCenter
                width: 160
                x: 50
                y: 54
            }
        }
        Item {
            anchors.fill: parent
            Item {
                height: 52
                width: 182
                x: 39
                y: 134
                Rectangle {
                    color: "#ffd4f5c9"
                    height: 50
                    radius: 20
                    width: 180
                    x: 1
                    y: 1
                }
            }
            Item {
                anchors.fill: parent
                Item {
                    height: 12
                    width: 139
                    x: 72
                    y: 174
                    Rectangle {
                        color: "#ff217903"
                        height: 9.99999
                        width: 137
                        x: 0.999992
                        y: 1
                    }
                }
                Item {
                    height: 14
                    width: 25
                    x: 179
                    y: 166
                    Rectangle {
                        color: "#ff5e88e7"
                        height: 12
                        width: 23
                        x: 1
                        y: 0.999998
                    }
                }
            }
            Text {
                clip: true
                color: "#ff000000"
                font.family: "Source Han Sans"
                font.pixelSize: 30
                font.weight: 500
                height: 44
                horizontalAlignment: Text.AlignHCenter
                text: "Example1"
                verticalAlignment: Text.AlignVCenter
                width: 160
                x: 50
                y: 134
            }
        }
    }
}
