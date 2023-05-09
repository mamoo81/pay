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
    id: pageTitledBox

    property alias title: boxTitle.text
    default property alias content: contentColumn.children

    implicitHeight: (boxTitle.text === "" ? 0 : boxTitle.height + 6) + 4 + contentColumn.implicitHeight + 6
    Layout.fillWidth: true
    height: implicitHeight

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: -10
        anchors.rightMargin: -10
        color: palette.alternateBase
    }
    Flowee.Label {
        id: boxTitle
        font.weight: 600
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
