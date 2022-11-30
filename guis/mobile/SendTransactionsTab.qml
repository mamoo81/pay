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
import "../Flowee" as Flowee
// import Flowee.org.pay;
import QtMultimedia

FocusScope {
    id: root
    property string icon: "qrc:/homeButtonIcon" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
    property string  title: qsTr("Send")

    Component.onCompleted: setOpacities()

    ListView {
        id: tabHeader
        width: parent.width
        height: 50
        clip: true
        orientation: ListView.Horizontal
        snapMode: ListView.SnapToItem

        model: stack.children.length
        delegate: Rectangle {
            width: Math.max(130, tabLabel.width)
            height: tabLabel.height + 25
            color: {
                if (index === tabHeader.currentIndex)
                    return Pay.useDarkSkin ? "darkgreen" : "green";
                return Pay.useDarkSkin ? "#565f68" : "#a5b5c6"
            }
            Flowee.Label {
                id: tabLabel
                anchors.centerIn: parent
                text: stack.children[index].title
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    tabHeader.currentIndex = index
                    root.setOpacities()
                }
            }
        }
    }

    function setOpacities() {
        for (let i = 0; i < stack.children.length; ++i) {
            let on = i === tabHeader.currentIndex;
            let child = stack.children[i];
            child.visible = on;
        }
        // try to make sure the current tab gets keyboard focus properly.
        forceActiveFocus();
        let visibleChild = stack.children[tabHeader.currentIndex];
        visibleChild.focus = true
    }

    Item {
        id: stack
        width: parent.width
        anchors.top: tabHeader.bottom
        anchors.bottom: parent.bottom

        FocusScope {
            id: tab1
            anchors.fill: parent
            property string title: qsTr("Rejected")
            /*
            Payment { // the model behind the Payment logic
                id: payment
                fiatPrice: Fiat.price
                account: portfolio.current
            } */
            Rectangle {
                width: 10
                height: 10
                x: 10
                color: "red"
            }
        }
        FocusScope {
            id: scanQrTab
            anchors.fill: parent
            property string title: qsTr("Scan QR")
            onFocusChanged: {
                console.log("focus received " + focus)
                CameraHandler.activate();
            }

            // only load this after authorization has become available.
            Loader {
                id: cameraStufff
                sourceComponent: CameraHandler.authorized ? videoFeedPanel : undefined
                anchors.fill: parent
                anchors.margins: 6
            }

            Component {
                id: videoFeedPanel
                Item {
                    anchors.fill: parent

                    Component.onCompleted:  CameraHandler.qmlCamera = camera
                    CaptureSession {
                        id: captureSession
                        videoOutput: videoOutput
                        camera: Camera { id: camera; }
                    }
                    VideoOutput {
                        id: videoOutput
                        fillMode: VideoOutput.Stretch
                        width: parent.width
                        height: {
                            var feedSize = camera.cameraFormat.resolution;
                            // on all phones I tried, the width and height are swapped. (Qt641)
                            // This fixed it, but makes it look weird on Desktop or when Qt gets fixed.
                            return width * (feedSize.width / feedSize.height);
                        }
                    }
                }
            }

            Flowee.Label {
                wrapMode: Text.Wrap
                text: {
                    if (CameraHandler.denied)
                        return qsTr("Camera permission denied. Change permissions to scan");
                    var cam = CameraHandler.qmlCamera;
                    if (cam == null)
                        return "waiting for permission";
                    return cam.errorString
                }
                x: 10
                y: 350
                width: parent.width
            }
        }
        FocusScope {
            property string title: "Tab C"
            anchors.fill: parent
            Rectangle {
                width: 10
                height: 10
                x: 30
                color: "yellow"
            }
            QQC2.TextField {
                focus: true
                y: 100
            }
        }
    }
}
