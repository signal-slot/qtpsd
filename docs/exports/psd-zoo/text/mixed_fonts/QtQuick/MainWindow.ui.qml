import QtQuick

Item {
    height: 200
    width: 400
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 400
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        height: 23
        text: ""
        width: 79
        x: 22
        y: 82
    }
    Text {
        color: "#000000"
        height: 19
        text: ""
        width: 47
        x: 146
        y: 82
    }
}
