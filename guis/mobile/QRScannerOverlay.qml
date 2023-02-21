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
import QtQuick.Shapes
import QtMultimedia
import "../Flowee" as Flowee

FocusScope {
    id: root
    visible: CameraController.visible
    enabled: visible
    onVisibleChanged: if (visible) root.forceActiveFocus();

    Keys.onPressed: (event)=> {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
            event.accepted = true;
            CameraController.abort();
        }
    }
    Rectangle {
        id: background
        anchors.fill: parent
        color: palette.window
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
    Flowee.CloseIcon {
        anchors.right: parent.right
        anchors.rightMargin: 10
        y: 10
        onClicked: CameraController.abort();
    }

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
                fillMode: VideoOutput.Stretch
                width: parent.width
                height: parent.height
            }
        }
    }
}
