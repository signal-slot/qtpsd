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
        color: "#ff0000"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 16
        horizontalAlignment: Text.AlignLeft
        text: "Layer 1"
        width: 45
        x: 10
        y: 5
    }
    Text {
        color: "#00ff00"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 16
        horizontalAlignment: Text.AlignLeft
        text: "Layer 2"
        width: 45
        x: 10
        y: 30
    }
    Text {
        color: "#0000ff"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 16
        horizontalAlignment: Text.AlignLeft
        text: "Layer 3"
        width: 45
        x: 10
        y: 55
    }
    Text {
        color: "#ff8000"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 16
        horizontalAlignment: Text.AlignLeft
        text: "Layer 4"
        width: 45
        x: 10
        y: 80
    }
    Text {
        color: "#8000ff"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 16
        horizontalAlignment: Text.AlignLeft
        text: "Layer 5"
        width: 45
        x: 10
        y: 105
    }
    Text {
        color: "#008080"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 16
        horizontalAlignment: Text.AlignLeft
        text: "Layer 6"
        width: 45
        x: 10
        y: 130
    }
    Text {
        color: "#808000"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 16
        horizontalAlignment: Text.AlignLeft
        text: "Layer 7"
        width: 45
        x: 10
        y: 155
    }
    Text {
        color: "#404040"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 16
        horizontalAlignment: Text.AlignLeft
        text: "Layer 8"
        width: 45
        x: 10
        y: 180
    }
    Text {
        color: "#c000c0"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 16
        horizontalAlignment: Text.AlignLeft
        text: "Layer 9"
        width: 45
        x: 10
        y: 205
    }
    Text {
        color: "#00c000"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 16
        horizontalAlignment: Text.AlignLeft
        text: "Layer 10"
        width: 53
        x: 10
        y: 230
    }
}
