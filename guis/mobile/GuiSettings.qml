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
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../Flowee" as Flowee

Page {
    headerText: qsTr("Display Settings")
    id: root

    ColumnLayout {
        width: parent.width
        Flowee.Label {
            text: qsTr("Font sizing")
        }

        Row {
            width: parent.width
            id: fontSizing
            property double buttonWidth: width / 6
            Repeater {
                model: 6
                delegate: MouseArea {
                    width: fontSizing.buttonWidth
                    height: 30
                    property int target: index * 25 + 75
                    onClicked: Pay.fontScaling = target

                    Rectangle {
                        width: parent.width - 5
                        x: 2.5
                        height: 3
                        color: Pay.fontScaling == target ? root.palette.highlight : root.palette.button
                    }

                    Flowee.Label {
                        font.pixelSize: 15
                        text: "" + target
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }

        }

    }
}
