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
    Image {
        fillMode: Image.PreserveAspectFit
        height: 23
        source: "images/regular.png"
        width: 79
        x: 22
        y: 82
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 19
        source: "images/bold.png"
        width: 47
        x: 146
        y: 82
    }
}
