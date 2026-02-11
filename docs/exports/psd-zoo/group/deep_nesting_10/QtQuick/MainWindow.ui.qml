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
    Item {
        anchors.fill: parent
        Item {
            anchors.fill: parent
            Item {
                anchors.fill: parent
                Item {
                    anchors.fill: parent
                    Item {
                        anchors.fill: parent
                        Item {
                            anchors.fill: parent
                            Item {
                                anchors.fill: parent
                                Item {
                                    anchors.fill: parent
                                    Item {
                                        anchors.fill: parent
                                        Item {
                                            anchors.fill: parent
                                            Image {
                                                fillMode: Image.PreserveAspectFit
                                                height: 200
                                                source: "images/deepest.png"
                                                width: 200
                                                x: 0
                                                y: 0
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
