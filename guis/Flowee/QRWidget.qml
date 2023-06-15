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
import Flowee.org.pay

Item {
    id: root
    property alias qrSize: qrImage.width
    property alias textVisible: addressLine.visible
    property string qrText: ""

    property bool useRawString: false

    implicitWidth: qrImage.width
    implicitHeight: qrImage.height + (addressLine.visible ? addressLine.height : 0)

    function handleOnClicked() {
        Pay.copyToClipboard(qrText);
        // invert the feedback so a second tap removes the feedback again.
        clipboardFeedback.opacity = clipboardFeedback.opacity == 0 ? 1 : 0
    }

    Image {
        id: qrImage
        source: {
            var text = root.qrText;
            if (text === "")
                return "";
            if (useRawString)
                return "image://qr-raw/" + text;
            return "image://qr/" + text;
        }
        smooth: false
        width: 256 // exported at root level
        height: width
        anchors.horizontalCenter: parent.horizontalCenter

        MouseArea {
            anchors.fill: parent
            onClicked: root.handleOnClicked()
        }
        Behavior on width { NumberAnimation { } }
    }
    Rectangle {
        id: addressLine
        width: parent.width
        height: addressLabel.height + 10
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        radius: 6
        color: palette.base
        Label {
            id: addressLabel
            anchors.centerIn: parent
            width: parent.width
            text: {
                var address = root.qrText
                let index = address.indexOf(":");
                if (index >= 0)
                    address = address.substr(index + 1); // cut off the prefix
                index = address.indexOf("?")
                if (index >= 0)
                    address = address.substr(0, index - 1); // cut off the tailing parts
                return address;
            }
            horizontalAlignment: Text.AlignHCenter

            // font:
            minimumPixelSize: 2 // min
            font.pixelSize: 20 // max
            fontSizeMode: Text.HorizontalFit // fit in width
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.handleOnClicked()
        }
    }

    Rectangle {
        id: clipboardFeedback
        opacity: 0
        width: feedbackText.width + 20
        height: feedbackText.height + 14
        radius: 10
        color: Pay.useDarkSkin ? "#333" : "#ddd"
        anchors.bottom: qrImage.bottom
        anchors.bottomMargin: -8
        anchors.horizontalCenter: qrImage.horizontalCenter

        Label {
            id: feedbackText
            x: 10
            y: 7
            text: qsTr("Copied to clipboard")
            wrapMode: Text.WordWrap
        }

        Behavior on opacity { OpacityAnimator {} }

        /// after 8 seconds, remove feedback.
        Timer {
            interval: 8000
            running: clipboardFeedback.opacity >= 1
            onTriggered: clipboardFeedback.opacity = 0
        }
    }

    Behavior on opacity { OpacityAnimator {} }
}
