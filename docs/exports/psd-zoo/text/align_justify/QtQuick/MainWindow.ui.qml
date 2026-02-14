import QtQuick

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
    Text {
        color: "#000000"
        font.family: "Roboto"
        font.pixelSize: 14
        height: 151
        horizontalAlignment: Text.AlignLeft
        text: "This is justified text that should wrap across multiple lines in a text box."
        width: 253
        wrapMode: Text.Wrap
        x: 25
        y: 24
    }
}
