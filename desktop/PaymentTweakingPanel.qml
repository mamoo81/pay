import QtQuick 2.11
import QtQuick.Controls 2.11
import "widgets" as Flowee

Item {
    id: root

    Rectangle {
        id: background
        color: "black"
        opacity: priv.collapsed ? 0 : 0.2
        anchors.fill: parent

        Behavior on opacity { NumberAnimation { } }
    }
    MouseArea {
        // area over the background to close our screen should the user click there.
        width: parent.width
        y: priv.y + priv.height
        height: parent.height - y
        enabled: !priv.collapsed
        visible: enabled
        onClicked: priv.collapsed = true
        cursorShape: Qt.UpArrowCursor
    }
    Item {
        id: priv
        width: parent.width
        height: content.height + content.y
        clip: true
        property bool collapsed: true

        Rectangle {
            x: -width / 2
            y: -height / 2
            radius: width / 2
            width: priv.collapsed ? 70 : priv.width * 2.9
            height: priv.collapsed ? 70 : priv.height * 2.9
            color: priv.collapsed ? mainWindow.palette.windowText : mainWindow.palette.window

            Behavior on color { ColorAnimation { duration: 400 } }
            Behavior on width { NumberAnimation { duration: 400 } }
            Behavior on height { NumberAnimation { duration: 400 } }
        }

        MouseArea {
            visible: !priv.collapsed
            anchors.fill: parent
            // dummy, just make sure that the background doensn't get them.
        }

        Item {
            id: content
            y: 20
            opacity: priv.collapsed ? 0 : 1
            visible: opacity != 0
            width: parent.width
            height: title.height + 60 + Math.max(leftColumn.height, rightColumn.height)

            Label {
                id: title
                text: "Some label"
                font.pointSize: 20
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Column {
                id: leftColumn
                width: parent.width / 2
                anchors.top: title.bottom
                anchors.topMargin: 20
                spacing: 10
                Label {
                    text: "Templates"
                    font.pointSize: 14
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Item { }

                Flowee.Button {
                    text: "Pay Rent"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            Column {
                id: rightColumn
                x: parent.width / 2
                width: parent.width / 2
                anchors.top: title.bottom
                anchors.topMargin: 20
                spacing: 10

                Label {
                    text: "Components"
                    font.pointSize: 14
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Item { }

                Flowee.Button {
                    text: "Input Selector"
                    anchors.horizontalCenter: parent.horizontalCenter
                    onClicked: {
                        payment.addInputSelector();
                        priv.collapsed = true
                    }
                }
                Flowee.Button {
                    text: "Comment"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Flowee.Button {
                    text: "Contract"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            // line
            Rectangle {
                color: "black"
                height: 4
                width: parent.width
                anchors.bottom: parent.bottom
            }
            Flowee.CloseIcon {
                anchors.right: parent.right
                anchors.rightMargin: 10
                onClicked: priv.collapsed = true
            }
        }

        MouseArea {
            visible: priv.collapsed
            width: 60
            height: 30
            cursorShape: Qt.ClosedHandCursor

            onClicked: priv.collapsed = false
        }
    }

    // the two lines that make up the "+" icon
    Rectangle {
        opacity: priv.collapsed ? 1 : 0
        color: mainWindow.palette.window
        width: 17
        height: 3
        x: 6
        y: 13
    }
    Rectangle {
        opacity: priv.collapsed ? 1 : 0
        color: mainWindow.palette.window
        width: 3
        height: 17
        y: 6
        x: 13
    }
}
