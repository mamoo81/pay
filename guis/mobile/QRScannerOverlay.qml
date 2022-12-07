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
// import QtQuick.Layouts
import QtMultimedia
import "../Flowee" as Flowee

Item {
    id: root

    // We put the 'Camera' in a loader to avoid Android permissions to be popped up until the
    // feature is actually requisted by the user.
    Loader {
        sourceComponent: videoFeedPanel
        active: CameraController.loadCamera
        anchors.fill: parent
    }

    Component {
        id: videoFeedPanel
        Rectangle {
            color: mainWindow.palette.window
            anchors.left: parent.left
            anchors.right: parent.right
            height: 300

            Component.onCompleted: {
                CameraController.camera = camera
                CameraController.videoSink = videoOutput.videoSink
            }
            Connections {
                target: CameraController
                function onCameraActiveChanged() {
                     if (CameraController.cameraActive) {
                         camera.start();
                     } else {
                         // camera.stop(); // stopping the camera on Qt641 tends to not allow us to restart it
                     }
                }
            }

            Camera {
                id: camera
            }
            CaptureSession {
                camera: camera
                videoOutput: videoOutput
            }
            VideoOutput {
                id: videoOutput
                visible: CameraController.cameraActive
                fillMode: VideoOutput.Stretch
                width: parent.width
                height: parent.height
            }
            Rectangle {
                opacity: 0.3
                color: "black"
                width: parent.width;
                height: parent.height / 4
            }
            Rectangle {
                opacity: 0.3
                color: "black"
                width: parent.width / 5
                height: parent.height
            }
            Rectangle {
                opacity: 0.3
                color: "black"
                width: parent.width;
                height: parent.height / 4
                anchors.bottom: parent.bottom
            }
            Rectangle {
                opacity: 0.3
                color: "black"
                width: parent.width / 5
                height: parent.height
                anchors.right: parent.right
            }

            Flowee.CloseIcon {
                anchors.right: parent.right
                anchors.rightMargin: 10
                y: 10
                onClicked: CameraController.abort();
            }
        }
    }
}
