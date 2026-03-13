import QtQuick

Item {
    height: 200
    width: 300
    Rectangle {
        color: "#ff000000"
        height: 200
        width: 300
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 28
        source: "images/white_text.png"
        width: 166
        x: 51
        y: 93
    }
}
