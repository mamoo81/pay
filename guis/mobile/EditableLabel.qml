/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2023 Tom Zander <tom@flowee.org>
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
import "../Flowee" as Flowee

Item {
    id: root
    implicitHeight: editWidget.implicitHeight
    height: editWidget.height
    property alias text: ourLabel.text
    property bool editable: true

    signal edited;

    Flowee.Label {
        id: ourLabel
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        anchors.left: parent.left
        anchors.right: editIcon.left
        anchors.rightMargin: 10
        anchors.baseline: editWidget.baseline
        visible: !editWidget.visible
    }
    Image {
        id: editIcon
        anchors.right: parent.right
        anchors.bottom: ourLabel.bottom
        width: 16
        height: 16
        smooth: true
        source: "qrc:/edit-pen" + (Pay.useDarkSkin ? "-light" : "") + ".svg"
        opacity: {
            if (root.editable == false)
                return 0;
            if (editWidget.visible) // button is disabled.
                return 0.4
            return 1; // enabled
        }
        visible: opacity > 0
        enabled: editWidget.visible == false
        MouseArea {
            anchors.fill: parent
            anchors.margins: -15
            onClicked: {
                editWidget.visible = !editWidget.visible;
                if (editWidget.focus)
                    editWidget.forceActiveFocus();
            }
        }
        Behavior on opacity { NumberAnimation { } }
    }
    Flowee.TextField {
        id: editWidget
        anchors.left: parent.left
        anchors.right: editIcon.left
        anchors.rightMargin: 10
        visible: false
        focus: visible
        text: ourLabel.text
        onTextChanged: {
            ourLabel.text = text;
            root.edited();
        }
        onEditingFinished: visible = false
    }
}
