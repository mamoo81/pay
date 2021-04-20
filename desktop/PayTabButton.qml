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
import QtQuick 2.14
import QtQuick.Controls 2.14
import Flowee.org.pay 1.0

Item {
    id: root
    height: payTabButtonText.height + 20
    width: 100
    property bool isActive: false
    property alias text: payTabButtonText.text
    property string icon: ""

    signal activated;

    function getIndex() {
        var index = 0;
        var children = parent.children
        for (let i in children) {
            if (children[i] === root)
                return index;
            ++index;
        }
        return -1;
    }

    Row {
        height: parent.height
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 10
        Image {
            visible: root.icon !== ""
            source: root.icon
            width: 32
            height: 32
        }
        Label {
            id: payTabButtonText
            anchors.verticalCenter: parent.verticalCenter
            // color: Flowee.useDarkSkin ? "white" : "black"
            font.bold: true
        }
    }

    Rectangle {
        id: highlight
        width: parent.width - 6
        x: 3
        height: 4
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        property bool hover: false
        color: {
            if (root.isActive)
                return mainWindow.floweeGreen
            if (Flowee.useDarkSkin) {
                if (hover)
                    return mainWindow.floweeSalmon
                return "#EEE"
            }
            // light skin
            if (hover)
                return mainWindow.floweeSalmon
            return "#111"
        }
    }
    MouseArea {
        hoverEnabled: true
        anchors.fill: parent
        onEntered: highlight.hover = true
        onExited: highlight.hover = false
        onClicked: root.activated()
    }
}
