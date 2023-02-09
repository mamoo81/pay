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

Image {
    id: root
    property QtObject request: null

    width: height
    height: {
        var h = parent.height - 220;
        return Math.min(h, 256)
    }
    source: root.request == null ? "" : "image://qr/" + root.request.qr
    smooth: false
    opacity: root.request == null || root.request.state === PaymentRequest.Unpaid ? 1: 0

    MouseArea {
        anchors.fill: parent
        onClicked: {
            Pay.copyToClipboard(root.request.qr)
            clipboardFeedback.opacity = 1
        }
    }

    Rectangle {
        id: clipboardFeedback
        opacity: 0
        width: feedbackText.width + 20
        height: feedbackText.height + 20
        radius: 10
        color: Pay.useDarkSkin ? "#333" : "#ddd"
        anchors.top: parent.bottom
        anchors.topMargin: -13
        anchors.horizontalCenter: parent.horizontalCenter

        Label {
            id: feedbackText
            x: 10
            y: 10
            text: qsTr("Copied to clipboard")
            wrapMode: Text.WordWrap
        }

        Behavior on opacity { OpacityAnimator {} }

        /// after 10 seconds, remove feedback.
        Timer {
            interval: 10000
            running: clipboardFeedback.opacity >= 1
            onTriggered: clipboardFeedback.opacity = 0
        }
    }

    Behavior on opacity { OpacityAnimator {} }
}
