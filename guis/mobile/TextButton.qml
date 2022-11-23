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
import "../Flowee" as Flowee

Item {
    id: root
    width: parent.width
    height: label.height + (smallLabel.text === "" ? 0 : smallLabel.height + 6) + 20

    signal clicked
    property alias text: label.text
    property alias subtext: smallLabel.text
    property bool showPageIcon: false
    property string imageSource: ""
    property int imageWidth: 16
    property int imageHeight: 16

    Flowee.Label {
        id: label
        y: 10
        width: parent.width
        wrapMode: Text.WordWrap
    }
    Flowee.Label {
        id: smallLabel
        anchors.top: label.bottom
        anchors.topMargin: 6
        font.pointSize: mainWindow.font.poinSize * 0.7
        font.bold: false
        color: Qt.darker(palette.text, 1.5)
    }
    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }

    Loader {
        sourceComponent: {
            if (root.showPageIcon)
                return pageIcon;
            if (root.imageSource !== "")
                return genericImage;
            return undefined;
        }
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
    }
    Component {
        id: pageIcon
        Image {
            width: 12
            height: 8
            source: Pay.useDarkSkin ? "qrc:/smallArrow-light.svg" : "qrc:/smallArrow.svg";
            rotation: 270
        }
    }
    Component {
        id: genericImage
        Image {
            source: root.imageSource
            width: root.imageWidth
            height: root.imageHeight
        }
    }
}
