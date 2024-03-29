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
import QtQuick
import QtQuick.Controls as QQC2

Label {
    id: root
    elide: wrapMode === Text.NoWrap ? Text.ElideMiddle : Text.ElideNone

    // override the text to be copied to clipboard
    property string clipboardText: ""
    property string menuText: qsTr("Copy")

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton | Qt.LeftButton
        onClicked: menu.start(parent);
        cursorShape: Qt.PointingHandCursor

        QQC2.Menu {
            id: menu
            function start(label) {
                if (label.clipboardText !== "")
                    this.text = label.clipboardText;
                else
                    this.text = label.text;
                popup();
            }
            property string text: ""
            QQC2.MenuItem {
                text: root.menuText
                onTriggered: Pay.copyToClipboard(menu.text)
            }
        }
    }
}
