import QtQuick

Item {
    height: 300
    width: 300
    Image {
        fillMode: Image.PreserveAspectFit
        height: 300
        source: "images/background.png"
        width: 300
        x: 0
        y: 0
    }
    Text {
        color: "#000000"
        height: 22
        text: ""
        width: 40
        x: 100
        y: 33
    }
    Text {
        color: "#000000"
        height: 19
        text: ""
        width: 70
        x: 102
        y: 132
    }
    Text {
        color: "#000000"
        height: 18
        text: ""
        width: 76
        x: 102
        y: 233
    }
}
