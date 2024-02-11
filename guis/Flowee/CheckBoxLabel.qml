/*
 * This file is part of the Flowee project
 * Copyright (C) 2021-2023 Tom Zander <tom@flowee.org>
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

/**
 * This is a buddy that goes with a CheckBox component for when
 * you want to put the label of a checkbox separately in a grid layout.
 * Assign the 'buddy' property to what your checkbox is and naturally
 * a text value, it will then become a clickable label which triggers
 * the checkbox when clicked.
 */
Label {
    id: root
    property var buddy: null
    property string toolTipText: ""

    QQC2.ToolTip {
        delay: 600
        text: root.toolTipText
        visible: root.toolTipText !== "" && mouseArea.containsMouse
    }
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        anchors.margins: -5
        onClicked: if (root.buddy != null) root.buddy.toggle();
        hoverEnabled: root.toolTipText !== ""
        cursorShape: Qt.PointingHandCursor
    }
}
