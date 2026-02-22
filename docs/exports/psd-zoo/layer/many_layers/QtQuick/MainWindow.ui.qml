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
        clip: true
        color: "#ffff0000"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 19
        horizontalAlignment: Text.AlignLeft
        text: "Layer 1"
        verticalAlignment: Text.AlignBottom
        width: 45
        x: 10
        y: 5
    }
    Text {
        clip: true
        color: "#ff00ff00"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 19
        horizontalAlignment: Text.AlignLeft
        text: "Layer 2"
        verticalAlignment: Text.AlignBottom
        width: 45
        x: 10
        y: 30
    }
    Text {
        clip: true
        color: "#ff0000ff"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 19
        horizontalAlignment: Text.AlignLeft
        text: "Layer 3"
        verticalAlignment: Text.AlignBottom
        width: 45
        x: 10
        y: 55
    }
    Text {
        clip: true
        color: "#ffff8000"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 19
        horizontalAlignment: Text.AlignLeft
        text: "Layer 4"
        verticalAlignment: Text.AlignBottom
        width: 45
        x: 10
        y: 80
    }
    Text {
        clip: true
        color: "#ff8000ff"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 19
        horizontalAlignment: Text.AlignLeft
        text: "Layer 5"
        verticalAlignment: Text.AlignBottom
        width: 45
        x: 10
        y: 105
    }
    Text {
        clip: true
        color: "#ff008080"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 19
        horizontalAlignment: Text.AlignLeft
        text: "Layer 6"
        verticalAlignment: Text.AlignBottom
        width: 45
        x: 10
        y: 130
    }
    Text {
        clip: true
        color: "#ff808000"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 19
        horizontalAlignment: Text.AlignLeft
        text: "Layer 7"
        verticalAlignment: Text.AlignBottom
        width: 45
        x: 10
        y: 155
    }
    Text {
        clip: true
        color: "#ff404040"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 19
        horizontalAlignment: Text.AlignLeft
        text: "Layer 8"
        verticalAlignment: Text.AlignBottom
        width: 45
        x: 10
        y: 180
    }
    Text {
        clip: true
        color: "#ffc000c0"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 19
        horizontalAlignment: Text.AlignLeft
        text: "Layer 9"
        verticalAlignment: Text.AlignBottom
        width: 45
        x: 10
        y: 205
    }
    Text {
        clip: true
        color: "#ff00c000"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 19
        horizontalAlignment: Text.AlignLeft
        text: "Layer 10"
        verticalAlignment: Text.AlignBottom
        width: 53
        x: 10
        y: 230
    }
}
