/*
 * This file is part of the Flowee project
 * Copyright (C) 2023 Tom Zander <tom@flowee.org>
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
import QtQuick.Layouts
import "../Flowee" as Flowee

Item {
    property alias title: boxTitle.text
    property alias spacing: contentColumn.spacing
    default property alias content: contentColumn.children

    implicitHeight: {
        var h = 6;
        if (boxTitle.text !== "")
            h += 4 + boxTitle.height; // 4 is the spacing above title
        let contentHeight = contentColumn.implicitHeight;
        if (contentHeight > 0)
            h += contentHeight + 6;
        return h;
    }
    Layout.fillWidth: true
    height: implicitHeight
    implicitWidth: 50 // have SOME non-zero default.
    visible: implicitHeight > 0

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: -10
        anchors.rightMargin: -10
        color: palette.alternateBase
    }
    Flowee.Label {
        id: boxTitle
        font.weight: 700
        y: 4
    }

    Column {
        id: contentColumn
        width: parent.width
        anchors.top: boxTitle.text === "" ? parent.top : boxTitle.bottom
        anchors.topMargin: 6
        spacing: 6

    }
}
