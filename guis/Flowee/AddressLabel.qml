/*
 * This file is part of the Flowee project
 * Copyright (C) 2024 Tom Zander <tom@flowee.org>
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

QQC2.Control {
    id: root
    required property QtObject txInfo;
    property alias highlight: highlight.visible

    visible: txInfo !== null
    width: implicitWidth
    height: implicitHeight
    implicitHeight: Math.max(theLabel.implicitHeight, copyIcon.height)
    implicitWidth: theLabel.implicitWidth + 10 + copyIcon.width

    Label {
        background: Rectangle {
            color: Pay.useDarkSkin ? "#4fb2e7" : "yellow"
            id: highlight
            radius: height / 3
            opacity: 0.2
        }
        width: parent.width - copyIcon.width - 10
        height: parent.height

        id: theLabel
        elide: wrapMode === Text.NoWrap ? Text.ElideMiddle : Text.ElideNone
        property bool allowCloak: true

        text: {
            if (allowCloak) {
                var cloaked = root.txInfo.cloakedAddress
                if (cloaked !== "")
                    return cloaked;
            }
            return root.txInfo.address;
        }
        font.pixelSize: {
            if (text == root.txInfo.cloakedAddress)
                return root.font.pixelSize;
            return root.font.pixelSize * 0.9;
        }

        MouseArea {
            anchors.fill: parent
            anchors.margins: -6 // a bit bigger
            onClicked: parent.allowCloak = !parent.allowCloak
        }
    }
    Rectangle {
        id: copiedFeedback
        anchors.fill: theLabel
        color: palette.mid
        opacity: 0
        visible: opacity > 0
        Timer {
            running: parent.visible
            onTriggered: parent.opacity = 0;
            interval: 500
        }
        Behavior on opacity { NumberAnimation { duration: 250 } }
    }
    Image {
        id: copyIcon
        anchors.right: parent.right
        width: 20
        height: 20
        source: "qrc:/edit-copy" + (Pay.useDarkSkin ? "-light" : "") + ".svg"
        MouseArea {
            anchors.fill: parent
            anchors.margins: -15
            onClicked: {
                copiedFeedback.opacity = 0.6
                Pay.copyToClipboard(Pay.chainPrefix + root.txInfo.address)
            }
        }
    }
}
