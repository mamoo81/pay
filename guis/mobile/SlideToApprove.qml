import QtQuick
import "../Flowee" as Flowee

Item {
    id: root
    signal activated;

    height: 60
    Rectangle {
        x: 30
        width: parent.height
        height: width
        radius: width / 2
        color: root.palette.window
    }
    Rectangle {
        x: parent.width - width - 30
        width: parent.height
        height: width
        radius: width / 2
        color: root.palette.window
    }
    Rectangle {
        x: 30 + parent.height / 2
        width: parent.width - 30 - 30 - parent.height
        height: parent.height
        color: root.palette.window
    }
    Flowee.Label {
        id: textLabel
        text: qsTr("SLIDE TO SEND")
        anchors.centerIn: parent
    }

    Rectangle {
        id: dragCircle
        width: parent.height - 5
        height: width
        radius: width / 2
        x: 35
        y: 2.5
        color: {
            if (root.enabled)
                return Pay.useDarkSkin ? mainWindow.floweeGreen : mainWindow.floweeBlue;
            return Qt.darker(textLabel.palette.button, 1.2);
        }
        property bool finished: false
        onXChanged: {
            if (x >= root.width - 70) {
                finished = true;
                root.activated();
            }
        }

        DragHandler {
            id: dragHandler
            yAxis.enabled: false
            xAxis.enabled: !dragCircle.finished
            xAxis.minimum: 35
            xAxis.maximum: root.width - 70
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.DragMoveCursor
            onActiveChanged: {
                // should the user abort the swipe, move it back
                if (!active && !dragCircle.finished)
                    parent.x = 35
            }
        }
        Behavior on x {
            NumberAnimation {
                id: revertAnim
                easing.type: Easing.OutElastic
            }
        }
    }
}
