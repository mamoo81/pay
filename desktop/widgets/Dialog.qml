/*
 * This file is part of the Flowee project
 * Copyright (C) 2021-2022 Tom Zander <tom@flowee.org>
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
    signal rejected
    property alias title: titleLabel.text
    property alias text: mainTextLabel.text
    property alias contentComponent: content.sourceComponent

    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    onVisibleChanged: {
        // make sure any content gets focus on open
        if (visible && content.item)
            content.item.forceActiveFocus()
    }

    Column {
        width: {
            let wanted = Math.max(mainTextLabel.implicitWidth, titleLabel.implicitWidth)
            if (content.item)
                wanted = Math.max(wanted, content.item.width)
            let max = root.parent.width
            let min = buttons.implicitWidth
            let ideal = Math.max(min, max / 3)
            if (wanted < ideal)
                return ideal
            if (wanted < max / 2 * 3)
                return wanted
            return max / 2 * 3;
        }
        spacing: 10
        Label {
            id: titleLabel
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Label {
            id: mainTextLabel
            width: parent.width
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }
        Loader {
            id: content
            width: parent.width
        }
        Flowee.DialogButtonBox {
            id: buttons
            standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
            width: parent.width
            onAccepted: {
                root.accepted();
                root.close()
            }
            onRejected: {
                root.rejected();
                root.close()
            }
        }
    }
}
