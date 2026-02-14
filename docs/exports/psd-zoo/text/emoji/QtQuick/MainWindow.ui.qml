import QtQuick
import QtQuick.Layouts

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
        height: 72
        width: 203
        x: 30
        y: 72
        Column {
            anchors.centerIn: parent
            RowLayout {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 0
                Text {
                    Layout.fillHeight: true
                    color: "#000000"
                    font.family: "Noto Color Emoji"
                    font.pixelSize: 36
                    text: "😀"
                }
                Text {
                    Layout.fillHeight: true
                    color: "#000000"
                    font.family: "Noto Sans"
                    font.pixelSize: 36
                    text: ""
                }
                Text {
                    Layout.fillHeight: true
                    color: "#000000"
                    font.family: "Noto Color Emoji"
                    font.pixelSize: 36
                    text: "🚀"
                }
                Text {
                    Layout.fillHeight: true
                    color: "#000000"
                    font.family: "Noto Sans"
                    font.pixelSize: 36
                    text: ""
                }
                Text {
                    Layout.fillHeight: true
                    color: "#000000"
                    font.family: "Noto Color Emoji"
                    font.pixelSize: 36
                    text: "❤"
                }
                Text {
                    Layout.fillHeight: true
                    color: "#000000"
                    font.family: "Noto Sans"
                    font.pixelSize: 36
                    text: ""
                }
                Text {
                    Layout.fillHeight: true
                    color: "#000000"
                    font.family: "Noto Color Emoji"
                    font.pixelSize: 36
                    text: "🌟"
                }
            }
        }
    }
}
