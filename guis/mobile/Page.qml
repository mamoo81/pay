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
    property alias columns: child.columns
    property alias headerButtonVisible: headerButton.visible
    property alias headerButtonText: headerButton.text
    property alias headerButtonEnabled: headerButton.enabled
    signal headerButtonClicked

    function takeFocus() {
        focusScope.forceActiveFocus();
    }

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

        QQC2.Label {
            id: headerLabel
            color: "white"
            anchors.left: backButton.right
            anchors.right: headerButton.left
            horizontalAlignment: Text.AlignHCenter
            anchors.verticalCenter: parent.verticalCenter
        }

        Flowee.Button {
            id: headerButton
            visible: false
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.headerButtonClicked()
        }
    }

    Flickable {
        width: parent.width
        y: header.height
        height: parent.height - y
        clip: true
        contentHeight: child.implicitHeight
        contentWidth: width
        FocusScope {
            id: focusScope
            GridLayout {
                id: child
                width: root.width - 20
                x: 10 // default margin
                height: implicitHeight
                columns: 1
            }
        }
    }
    Keys.onPressed: (event)=> {
        if (event.key === Qt.Key_Back) {
            event.accepted = true;
            thePile.pop();
        }
    }
}
