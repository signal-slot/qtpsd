import QtQuick
import QtQuick.Layouts

Item {
    height: 240
    width: 320
    Item {
        anchors.fill: parent
        Rectangle {
            color: "#ff012401"
            height: 240
            width: 320
            x: 0
            y: 0
        }
        Text {
            anchors.fill: parent
            clip: true
            color: "#ffeac3c3"
            font.family: "Source Han Sans"
            font.pixelSize: 50
            horizontalAlignment: Text.AlignHCenter
            text: ""
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            clip: true
            color: "#ffeac3c3"
            font.family: "Source Han Sans"
            font.pixelSize: 30
            height: 44
            horizontalAlignment: Text.AlignHCenter
            text: "文字列中に\n改行"
            verticalAlignment: Text.AlignVCenter
            width: 160
            x: 2
            y: 1
        }
        Item {
            clip: true
            height: 44
            width: 144
            x: 169
            y: 1
            Column {
                anchors.centerIn: parent
                RowLayout {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 0
                    Text {
                        Layout.fillHeight: true
                        color: "#ffeac3c3"
                        font.family: "Koz Go Pr6N"
                        font.pixelSize: 30
                        horizontalAlignment: Text.AlignHCenter
                        text: "文字列"
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        Layout.fillHeight: true
                        color: "#ffeac3c3"
                        font.family: "Koz Go Pr6N"
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        text: "中"
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        Layout.fillHeight: true
                        color: "#ffeac3c3"
                        font.family: "Koz Go Pr6N"
                        font.pixelSize: 30
                        horizontalAlignment: Text.AlignHCenter
                        text: "に"
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                RowLayout {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 0
                    Text {
                        Layout.fillHeight: true
                        color: "#ffeac3c3"
                        font.family: "Koz Go Pr6N"
                        font.pixelSize: 30
                        horizontalAlignment: Text.AlignHCenter
                        text: "別"
                        verticalAlignment: Text.AlignVCenter
                    }
                    Text {
                        Layout.fillHeight: true
                        color: "#ffeac3c3"
                        font.family: "Source Han Sans"
                        font.pixelSize: 18
                        horizontalAlignment: Text.AlignHCenter
                        text: "フォント"
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        Text {
            clip: true
            color: "#ffeac3c3"
            font.family: "Koz Go Pr6N"
            font.pixelSize: 30
            height: 44
            horizontalAlignment: Text.AlignHCenter
            text: "Shift\n+改行"
            verticalAlignment: Text.AlignVCenter
            width: 86
            x: 25
            y: 101
        }
        Text {
            clip: true
            color: "#ffeac3c3"
            font.family: "Koz Go Pr6N"
            font.pixelSize: 24
            height: 116
            horizontalAlignment: Text.AlignHCenter
            text: "段落テキストは折り返される"
            verticalAlignment: Text.AlignVCenter
            width: 158
            wrapMode: Text.Wrap
            x: 160
            y: 110
        }
    }
}
