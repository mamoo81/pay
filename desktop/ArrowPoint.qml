import QtQuick 2.14

Item {
    id: arrowPoint
    property alias color: rotatedRect.color

    clip: true
    Rectangle {
        id: rotatedRect
        width: parent.height / 1.41
        height: width
        rotation: 45
        transformOrigin: Item.Center
        x: (width - parent.width) / 2 - (width / 2 * 1.41) + 2
        y: (height - parent.width) / 2
    }
}
