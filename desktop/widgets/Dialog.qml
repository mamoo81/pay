import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11

/*
 * This is a simple message-box style dialog that does not creat a new window.
 *
 * This popup is similar to various widges in the Qt library, but all
 * them suffer from the DialogButtonBox having really bad defaults.
 * Specifically, it doesn't align the buttons right, but uses the
 * full width (making the buttons too big).
 * Also it doesn't have any spacing between the buttons.
 */
Popup {
    id: root

    signal accepted
    property alias title: titleLabel.text
    property alias text: mainTextLabel.text
    property alias standardButtons: box.standardButtons

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    Column {
        spacing: 10
        Label {
            id: titleLabel
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Label {
            id: mainTextLabel
        }
        RowLayout {
            width: parent.width
            Item { width: 1; height: 1; Layout.fillWidth: true } // spacer
            DialogButtonBox {
                id: box
                // next line is a hack to give spacing between the buttons.
                Component.onCompleted: children[0].spacing = 10 // In Qt5.15 the first child is ListView

                standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
                onAccepted: {
                    root.accepted();
                    root.close()
                }
                onRejected: root.close()
            }
        }
    }
}
