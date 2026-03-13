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
        height: 33
        source: "images/this_paragraph_has_left_and_right_indentation_applied_for_testi.png"
        width: 277
        x: 10
        y: 10
    }
}
