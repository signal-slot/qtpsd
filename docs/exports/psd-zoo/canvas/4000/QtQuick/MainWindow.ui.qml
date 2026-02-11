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
    Image {
        fillMode: Image.PreserveAspectFit
        height: 3000
        source: "images/big_content.png"
        width: 3000
        x: 500
        y: 500
    }
}
