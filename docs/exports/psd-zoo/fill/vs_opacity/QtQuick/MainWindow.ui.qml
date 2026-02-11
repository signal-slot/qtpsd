import QtQuick

Item {
    height: 200
    width: 200
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        source: "images/background.png"
        width: 200
        x: 0
        y: 0
    }
    Image {
        fillMode: Image.PreserveAspectFit
        height: 200
        opacity: 0.241569
        source: "images/fill_vs_opacity.png"
        width: 200
        x: 0
        y: 0
    }
}
