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
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "." as Flowee;

QQC2.Popup {
    id: root

    signal accepted
    signal rejected
    property alias title: titleLabel.text
    property alias text: mainTextLabel.text
    property alias contentComponent: content.sourceComponent

    function accept() {
        accepted();
        close();
    }

    modal: true
    closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutsideParent
    onVisibleChanged: {
        // make sure any content gets focus on open
        if (visible && content.item)
            content.item.forceActiveFocus()
    }
    onAboutToShow: reposition()
    onHeightChanged: reposition()
    function reposition() {
        // 'mainScreen' is defined in main.qml
        var window = mainScreen;
        var globalX = (window.width - root.width) / 2;
        var globalY = window.height / 3 - root.height;
        var local = mapFromItem(window, globalX, globalY);
        x = local.x;
        y = local.y;
    }

    Column {
        width: {
            // 'mainScreen' is defined in main.qml
            var window = mainScreen;

            let wanted = Math.max(mainTextLabel.contentWidth, titleLabel.implicitWidth)
            if (content.item)
                wanted = Math.max(wanted, content.item.implicitWidth)
            let max = window.width
            let min = buttons.implicitWidth
            let ideal = Math.max(min, max / 3)
            if (wanted < ideal)
                return ideal
            if (wanted < max / 2 * 3)
                return wanted
            return max / 2 * 3;
        }
        spacing: 10
        QQC2.Label {
            id: titleLabel
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }
        QQC2.Label {
            id: mainTextLabel
            // this next line will always create a binding loop. But its harmless, so ignore that comment.
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
            anchors.right: parent.right
            anchors.rightMargin: 10
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