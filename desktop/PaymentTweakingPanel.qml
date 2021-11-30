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
            height: randomRect.height + title.height + 30

            Label {
                id: title
                text: qsTr("Add Detail")
                font.pointSize: 20
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Rectangle {
                id: randomRect
                width: inputSelectorButton.width + 30
                height: width
                anchors.top: title.bottom
                anchors.topMargin: 10
                x: 10

                color: helpText.palette.light
                border.width: 2
                border.color: helpText.palette.shadow

                Flowee.Button {
                    id: inputSelectorButton
                    text: qsTr("Coin Selector")
                    y: 15
                    anchors.horizontalCenter: parent.horizontalCenter
                    onClicked: {
                        payment.addInputSelector();
                        priv.collapsed = true
                    }
                }
            }
            Label {
                id: helpText
                anchors.top: randomRect.top
                anchors.topMargin: 20
                anchors.left: randomRect.right
                anchors.leftMargin: 10
                anchors.right: parent.right
                wrapMode: Label.Wrap
                text: qsTr("To override the default selection of coins that are used to pay a transaction, you can add the 'Coin Selector' where the wallets coins will be made visible.")
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
