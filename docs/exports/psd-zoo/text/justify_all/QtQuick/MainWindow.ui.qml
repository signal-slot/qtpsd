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
    Image {
        fillMode: Image.PreserveAspectFit
        height: 38
        source: "images/this_text_is_fully_justified_including_the_last_line_of_each_pa.png"
        width: 280
        x: 10
        y: 10
    }
}
