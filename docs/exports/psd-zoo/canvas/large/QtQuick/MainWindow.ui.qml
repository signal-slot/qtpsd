import QtQuick

Item {
    height: 4000
    width: 4000
    Image {
        fillMode: Image.PreserveAspectFit
        height: 4000
        source: "images/background.png"
        width: 4000
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        font.family: "Sans Serif"
        font.pixelSize: 9
        height: 67
        horizontalAlignment: Text.AlignLeft
        text: ""
        width: 425
        x: 1806
        y: 1948
    }
}
