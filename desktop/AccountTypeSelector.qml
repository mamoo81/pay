/* * This file is part of the Flowee project
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

Item {
    id: root
    property int accountType: 0
    property alias title: name.text
    property var features: []
    signal clicked;

    property bool selected: parent.selectedAccountType === accountType
    width: name.width * 2
    height: 300

    Rectangle {
        color: {
            var base = name.palette.base
            if (Pay.useDarkSkin)
                return parent.selected ? Qt.lighter(base, 1.4) : Qt.darker(base, 0.9)
            return parent.selected ? Qt.darker(base, 1.04) : Qt.darker(base, 1.1)
        }
        border.width: parent.selected ? 5 : 2
        border.color: parent.selected ? name.palette.mid : name.palette.alternateBase
        width: parent.width
        height: parent.selected ? parent.height : parent.height - 50
        radius: 10
        y: parent.selected ? 0 : 25

        Behavior on height { NumberAnimation { } }
        Behavior on y { NumberAnimation { } }
        Behavior on border.width { NumberAnimation { } }
        Behavior on color { ColorAnimation { } }
        Behavior on border. color { ColorAnimation { } }
    }

    Label {
        id: name
        anchors.horizontalCenter: parent.horizontalCenter
        font.pointSize: 14
        font.bold: true
        y: 14 + 25
    }
    Column {
        id: contentArea
        spacing: 15
        width: parent.width
        anchors.top: name.bottom
        anchors.topMargin: 26

        Repeater {
            model: root.features
            Label {
                text: modelData
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(implicitWidth, contentArea.width - 50)
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: optionsRow.selectedAccountType = parent.accountType
    }
}
