/*
 * This file is part of the Flowee project
 * Copyright (C) 2021 Tom Zander <tom@flowee.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import "." as Flowee;

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
            Flowee.DialogButtonBox {
                id: box
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
