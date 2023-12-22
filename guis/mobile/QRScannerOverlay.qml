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
import QtQuick.Controls as QQC2
import QtQuick.Shapes
import QtMultimedia
import "../Flowee" as Flowee
import Flowee.org.pay;

FocusScope {
    id: root
    visible: CameraController.visible
    enabled: visible
    onVisibleChanged: if (visible) root.forceActiveFocus();

    Rectangle {
        id: background
        anchors.fill: parent
        color: palette.window
    }
    MouseArea {
        anchors.fill: parent
        // eat all clicks
    }

    // We put the 'Camera' in a loader to avoid Android permissions to be popped up until the
    // feature is actually requisted by the user.
    Loader {
        sourceComponent: videoFeedPanel
        active: CameraController.loadCamera
        anchors.fill: parent
    }

    // The 'progress' icon.
    Shape {
        id: cutout
        anchors.fill: parent.fill
        smooth: true
        opacity: 0.5

        property int x1: root.width / 2 - 100
        property int y1: root.height / 2 - 100
        property int y2: y1 + 200
        property int x2: x1 + 200
        property int radius: 30

        ShapePath {
            fillColor: "black"
            strokeWidth: 0
            strokeColor: "transparent"
            startX: 0; startY: 0

            PathLine { x: width; y: 0 }
            PathLine { x: width; y: height }
            PathLine { x: 0; y: height }
            PathLine { x: 0; y: height / 2 }

            // move to the center part.
            PathLine { x: cutout.x1; y: height / 2 }

            PathLine { x: cutout.x1; y: cutout.y1 + cutout.radius }
            PathArc {
                x: cutout.x1 + cutout.radius
                y: cutout.y1
                radiusX: cutout.radius
                radiusY: cutout.radius
            }

            PathLine { x: cutout.x2 - cutout.radius; y: cutout.y1 }
            PathArc {
                x: cutout.x2
                y: cutout.y1 + cutout.radius
                radiusX: cutout.radius
                radiusY: cutout.radius
            }
            PathLine { x: cutout.x2; y: cutout.y2 - cutout.radius}
            PathArc {
                x: cutout.x2 - cutout.radius
                y: cutout.y2
                radiusX: cutout.radius
                radiusY: cutout.radius
            }
            PathLine { x: cutout.x1 + cutout.radius; y: cutout.y2 }
            PathArc {
                x: cutout.x1
                y: cutout.y2 - cutout.radius
                radiusX: cutout.radius
                radiusY: cutout.radius
            }
            PathLine { x: cutout.x1; y: height / 2}
            PathLine { x: 0; y: height / 2 }
            PathLine { x: 0; y: 0 }
        }
    }

    Rectangle {
        id: pasteFrame
        x: 50
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 50
        visible: CameraController.supportsPaste && cbh.text !== ""
        radius: 6
        width: pasteButton.width
        height: pasteButton.height
        color: palette.base
        Flowee.ImageButton {
            id: pasteButton
            source: "qrc:/edit-clipboard" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
            text: qsTr("Paste")
            onClicked: pasteFeedback.visible = !CameraController.pasteData(cbh.text);
        }

        ClipboardHelper {
            id: cbh
            filter: ClipboardHelper.Addresses + ClipboardHelper.LegacyAddresses | ClipboardHelper.AddressUrl
            enabled: CameraController.cameraActive
        }

        Rectangle {
            id: pasteFeedback
            color: palette.toolTipBase
            border.color: palette.toolTipText
            border.width: 2
            width: errorLabel.width + 10
            height: errorLabel.height + 10
            radius: 5
            anchors.top : pasteButton.bottom
            visible: false

            Flowee.Label {
                id: errorLabel
                anchors.centerIn: parent
                text: qsTr("Failed")
                color: palette.toolTipText
            }
            Timer {
                interval: 4000
                running: parent.visible
                onTriggered: parent.visible = false
            }
        }
    }
    Rectangle {
        id: flashFrame
        anchors.top: pasteFrame.top
        anchors.right: parent.right
        anchors.rightMargin: 50
        radius: 6
        visible: false

        width: flashButton.width
        height: flashButton.height
        color: palette.base
        Flowee.ImageButton {
            id: flashButton
            source: "qrc:/flash" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
            opacity: CameraController.torchEnabled ? 0.3 : 1
            onClicked: CameraController.torchEnabled = !CameraController.torchEnabled
        }
    }

    Rectangle {
        width: parent.width
        height: Math.max(closeIcon.height, instaLabel.height) + 20
        color: mainWindow.floweeBlue

        Flowee.CloseIcon {
            id: closeIcon
            anchors.right: parent.right
            anchors.rightMargin: 10
            y: 10
            onClicked: CameraController.abort();
        }

        Flowee.Label {
            id: instaLabel
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.right: closeIcon.left
            anchors.margins: 10
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            color: "white"

            text: {
                if (isLoading)
                    return "";

                // slight hack, make this field be re-calculated on camera active change in order to ensure
                // we have the latest limit (which doesn't have a signal of its owo)
                var dummy = CameraController.cameraActive;

                let cur = portfolio.current;
                if (cur === null || !cur.allowInstaPay)
                    return "";
                let fiatName = Fiat.currencyName;
                let limit = cur.fiatInstaPayLimit(fiatName);
                if (limit === 0)
                    return "";
                var answer = qsTr("Instant Pay limit is %1").arg(Fiat.formattedPrice(limit));

                if (!portfolio.singleAccountSetup)
                    answer += "\n" + qsTr("Selected wallet: '%1'").arg(cur.name);
                return answer;
            }
        }
    }


    // ------ components below this, which are not instantiated by default -----
    Component {
        id: videoFeedPanel
        Item {
            Component.onCompleted: {
                CameraController.camera = camera
                CameraController.videoSink = videoOutput.videoSink
            }
            Connections {
                target: CameraController
                function onCameraActiveChanged() {
                     if (CameraController.cameraActive) {
                         camera.start();
                         flashFrame.visible = camera.isTorchModeSupported(Camera.TorchOn)
                     } else if (Qt.platform.os !== "linux") {
                         // at least on Linux stopping a camera and turning it on again fails with
                         // "Camera is in use"
                         camera.stop();
                     }
                }
            }

            Camera {
                id: camera
                active: false
            }
            CaptureSession {
                camera: camera
                videoOutput: videoOutput
            }
            VideoOutput {
                id: videoOutput
                fillMode: VideoOutput.PreserveAspectCrop
                width: parent.width
                height: parent.height
            }
        }
    }
}
