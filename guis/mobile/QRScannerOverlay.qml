import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
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
            color: "red"
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
                    console.log("CameraActive now: " + CameraController.cameraActive)
                     if (CameraController.cameraActive) {
                         camera.start();
                     } else {
                         // camera.stop();
                     }
                }
            }

           Camera {
                id: camera
                // active: CameraController.cameraActive
            }
            CaptureSession {
                camera: camera
                videoOutput: videoOutput
            }
            VideoOutput {
                id: videoOutput
                visible: CameraController.cameraActive
                fillMode: VideoOutput.Stretch
                // TODO use width/height from CameraController
                width: parent.width
                height: 400
            }
        }
    }
}
