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
        color: palette.window
    }
    Rectangle {
        x: parent.width - width - 30
        width: parent.height
        height: width
        radius: width / 2
        color: palette.window
    }
    Rectangle {
        x: 30 + parent.height / 2
        width: parent.width - 30 - 30 - parent.height
        height: parent.height
        color: palette.window
    }
    Flowee.Label {
        id: textLabel
        text: qsTr("SLIDE TO SEND")
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 30 + 50 // because the circle covers that.
        anchors.right: parent.right
        anchors.rightMargin: 80
        fontSizeMode: Text.HorizontalFit // account for long translations
        horizontalAlignment: Text.AlignHCenter
    }

    Rectangle {
        id: dragCircle
        width: parent.height - 5
        height: width
        radius: width / 2
        x: 35
        y: 2.5
        opacity: 0.7
        color: {
            if (root.enabled)
                return Pay.useDarkSkin ? mainWindow.floweeGreen : mainWindow.floweeBlue;
            return Qt.darker(palette.button, 1.2);
        }
        property bool finished: false
        onXChanged: {
            if (x >= root.width - 120) {
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
