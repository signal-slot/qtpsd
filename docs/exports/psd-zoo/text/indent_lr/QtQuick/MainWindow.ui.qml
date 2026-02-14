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
        font.pixelSize: 12
        height: 181
        horizontalAlignment: Text.AlignLeft
        text: "This paragraph has left and right indentation applied for testing margin parameters."
        width: 281
        wrapMode: Text.Wrap
        x: 10
        y: 9
    }
}
