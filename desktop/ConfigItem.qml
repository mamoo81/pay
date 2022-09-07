/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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

Item {
    id: root
    width: wide ? 12 : 4
    height: column.height
    property color color: Pay.useDarkSkin ? "white" : "black"
    property bool wide: false
    default property alias actions: ourMenu.contentData
    /// emitted when the menu is about to open.
    signal aboutToOpen;

    // replaces the current menu content with a list of actions
    function setMenuActions(actionList) {
        // remove old ones first
        while (ourMenu.count > 0) {
            ourMenu.takeItem(0);
        }
        // set new ones
        for (let i = 0; i < actionList.length; ++i) {
            ourMenu.addAction(actionList[i]);
        }
    }

    Column {
        id: column
        spacing: 3
        y: 1 // move the column down to account for the anti-alias line of the rectangle below

        Repeater {
            model: 3
            delegate: Rectangle {
                color: root.color
                width: root.wide ? 12 : 4
                height: 3
                radius: 2
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        anchors.margins: -10
        hoverEnabled: true // to make sure we eat them and avoid the hover feedback.
        acceptedButtons: Qt.RightButton | Qt.LeftButton
        cursorShape: Qt.PointingHandCursor

        Menu {
            id: ourMenu
        }
        onClicked: {
            root.aboutToOpen();
            ourMenu.popup()
        }
    }
}
