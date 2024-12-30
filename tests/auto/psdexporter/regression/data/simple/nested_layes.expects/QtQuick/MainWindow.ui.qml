import QtQuick

Item {
    height: 240
    width: 320
    Item {
        anchors.fill: parent
        Rectangle {
            color: "#f7e790"
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
                    color: "#d4f5c9"
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
                        color: "#217903"
                        height: -10
                        width: 137
                        x: 0.999992
                        y: 11
                    }
                }
                Item {
                    height: 14
                    width: 25
                    x: 79
                    y: 81
                    Rectangle {
                        color: "#5e88e7"
                        height: 12
                        width: 23
                        x: 1
                        y: 0.999993
                    }
                }
            }
            Text {
                color: "#000000"
                font.family: "SourceHanSans-Medium"
                font.pointSize: 20
                height: 58
                horizontalAlignment: Text.AlignHCenter
                text: "Example1\n "
                verticalAlignment: Text.AlignVCenter
                width: 160
                x: 50
                y: 63
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
                    color: "#d4f5c9"
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
                        color: "#217903"
                        height: -9.99999
                        width: 137
                        x: 0.999992
                        y: 11
                    }
                }
                Item {
                    height: 14
                    width: 25
                    x: 179
                    y: 166
                    Rectangle {
                        color: "#5e88e7"
                        height: 12
                        width: 23
                        x: 1
                        y: 0.999998
                    }
                }
            }
            Text {
                color: "#000000"
                font.family: "SourceHanSans-Medium"
                font.pointSize: 20
                height: 58
                horizontalAlignment: Text.AlignHCenter
                text: "Example1\n "
                verticalAlignment: Text.AlignVCenter
                width: 160
                x: 50
                y: 143
            }
        }
    }
}
