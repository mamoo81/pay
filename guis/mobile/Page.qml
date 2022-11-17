/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
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
import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 2.15
import "../Flowee" as Flowee

QQC2.Control {
    id: root

    width: parent == null ? 10 : parent.width
    height: parent == null ? 10 : parent.height

    default property alias content: child.children
    property alias headerText: headerLabel.text

    Rectangle {
        id: header
        width: parent.width
        height: 40
        color: Pay.useDarkSkin ? root.palette.base : mainWindow.floweeBlue

        Image {
            id: backButton
            x: 13
            source: "qrc:/back-arrow.svg"
            width: 20 * 1.1
            height: 15 * 1.1
            anchors.verticalCenter: parent.verticalCenter
            MouseArea {
                anchors.fill: parent
                anchors.margins: -15
                onClicked: thePile.pop();
            }
        }

        Text {
            id: headerLabel
            color: "white"
            anchors.centerIn: parent
        }
    }

    Flickable {
        width: parent.width
        y: header.height
        height: parent.height - y
        clip: true
        contentHeight: child.height
        contentWidth: width
        GridLayout {
            id: child
            width: parent.width
            height: implicitHeight
            columns: 1
        }
    }
    Keys.onPressed: (event)=> {
        if (event.key === Qt.Key_Back) {
            event.accepted = true;
            thePile.pop();
        }
    }
}
