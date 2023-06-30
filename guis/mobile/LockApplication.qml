/*
 * This file is part of the Flowee project
 * Copyright (C) 2023 Tom Zander <tom@flowee.org>
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
import QtQuick.Controls as QQC2
import Flowee.org.pay

Page {
    id: root
    headerText: qsTr("Security")

    Column {
        id: column
        width: parent.width
        y: 10
        spacing: 20

        PageTitledBox {
            id: pinOnStartup
            title: qsTr("PIN on startup")
            width: parent.width
            spacing: 10

            property string newPassword: ""
            property bool verified: false

            Flowee.Label {
                id: mainLabel
                text: Pay.appProtection === FloweePay.AppUnlocked
                        ? qsTr("Enter current PIN") : qsTr("Enter new PIN");
                width: parent.width
                wrapMode: Text.Wrap
            }
            Item {
                width: parent.width
                height: outline.height + 12
                Rectangle {
                    id: outline
                    y: 6
                    color: "#00000000"
                    border.width: 1
                    border.color: palette.button
                    height: pwd1.height + 10
                    width: Math.max(pwd1.implicitWidth, parent.width / 2)
                    anchors.horizontalCenter: parent.horizontalCenter
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            pinOnStartup.newPassword = "";
                            pinOnStartup.verified = false;
                            inputWidget.open();
                        }
                    }
                }

                Flowee.Label {
                    id: pwd1
                    text: pinOnStartup.newPassword === "" ? "" : "∙∙∙∙∙";
                    font.pixelSize: mainLabel.font.pixelSize * 2
                    anchors.centerIn: outline
                }
            }

            Flowee.Label {
                text: qsTr("Repeat PIN")
                width: parent.width
                wrapMode: Text.Wrap
                visible: Pay.appProtection === FloweePay.NoProtection
            }
            Item {
                width: parent.width
                height: outline2.height + 12
                visible: Pay.appProtection === FloweePay.NoProtection
                Rectangle {
                    id: outline2
                    y: 6
                    color: "#00000000"
                    border.width: 1
                    border.color: palette.button
                    height: pwd2.height + 10
                    width: Math.max(pwd2.implicitWidth, parent.width / 2)
                    anchors.horizontalCenter: parent.horizontalCenter
                    MouseArea {
                        anchors.fill: parent
                        onClicked: inputWidget.open();
                    }
                }

                Flowee.Label {
                    id: pwd2
                    text: pinOnStartup.verified ? "∙∙∙∙∙" : "";
                    font.pixelSize: mainLabel.font.pixelSize * 2
                    anchors.centerIn: outline2
                }
            }

            Item {
                width: parent.width
                height: button.height
                Flowee.Button {
                    id: button
                    text: Pay.appProtection === FloweePay.AppUnlocked ? qsTr("Remove PIN") : qsTr("Set PIN")
                    anchors.right: parent.right
                    enabled: pinOnStartup.verified
                    onClicked: {
                        if (Pay.appProtection === FloweePay.AppUnlocked) {
                            // then we are trying to remove the PIN. We just set it to an empty string
                            pinOnStartup.newPassword = "";
                            var feedback = qsTr("PIN has been removed");
                        }
                        else {
                            feedback = qsTr("PIN has been set");
                        }
                        Pay.setAppPassword(pinOnStartup.newPassword)
                        pinOnStartup.newPassword = "";
                        pinOnStartup.verified = false;
                        notificationPopup.show(feedback);
                    }
                }
                Behavior on y { NumberAnimation { } }
            }
        }

        PageTitledBox {
            title: qsTr("Encrypt your wallet data")
            width: parent.width

            TextButton {
                text: qsTr("Require PIN to Pay on wallet")
                subtext: qsTr("Secure against spending")
                showPageIcon: true
            }
            TextButton {
                text: qsTr("Require PIN to Open wallet")
                subtext: qsTr("Secure against all usage")
                showPageIcon: true
            }
        }
    }

    Item {
    }


    // the invisible drag handler is separate but on similar to the input widget.
    Item {
        id: dragHandler
        width: parent.width
        y: root.height + 10 // outside of viewport
        height: 25
        onYChanged: {
            // widget moves with us, unless its moved too far
            // then we insta close.
            var base = inputWidget.baseY;
            var diff = y - base;
            inputWidget.y = diff > 250 ? root.height + 10 :  y - 5
        }

        DragHandler {
            xAxis.enabled: false
            yAxis.minimum: root.height - inputWidget.height - 15
            acceptedButtons: Qt.LeftButton
            margin: 15
            cursorShape: Qt.DragMoveCursor

            onActiveChanged: {
                if (!active) {
                    // either close or reset.
                    var base = inputWidget.baseY;
                    var diff = inputWidget.y - base;
                    dragHandler.y = diff > 130 ? root.height + 10 : base  + 5
                }
            }
        }
    }
    Rectangle {
        id: inputWidget

        function open() {
            dragHandler.y = baseY
            widget.takeFocus();
        }
        function close() {
            dragHandler.y = parent.height + 10 // outside of viewport
            widget.password = "";
        }

        property int baseY: root.height - height - 20
        color: palette.window
        border.width: 1
        border.color: palette.highlight
        radius: 20
        width: parent.width
        height: Math.min(root.height, 610)
        UnlockWidget {
            id: widget
            y: 10
            x: 10
            width: parent.width - 20
            height: parent.height - 40
            buttonText: qsTr("Ok")

            onPasswordEntered: {
                var pwd = password;
                if (pwd === "")
                    return;

                if (pinOnStartup.newPassword === "") {
                    if (Pay.appProtection === FloweePay.AppUnlocked) { // then the PIN needs to be checked.
                        if (Pay.checkAppPassword(pwd)) {
                            pinOnStartup.verified = true;
                        }
                        else {
                            passwordIncorrect();
                            return;
                        }
                    }

                    pinOnStartup.newPassword = pwd;
                    inputWidget.close();
                    widget.acceptedPassword();
                }
                else if (pinOnStartup.newPassword === pwd) {
                    pinOnStartup.verified = true;
                    inputWidget.close();
                    widget.acceptedPassword();
                } else {
                    // validate failed. Passwords not equal
                    passwordIncorrect();
                }
            }
        }
        Rectangle {
            width: parent.width - 50
            anchors.horizontalCenter: parent.horizontalCenter
            y: 5
            height: 3
            color: palette.button
            radius: 3
        }

        Behavior on y { NumberAnimation { duration: 120 } }
    }

    // android navigation
    Keys.onPressed: (event)=> {
        if (event.key === Qt.Key_Back) {
            if (inputWidget.y < root.height) {
                inputWidget.close();
                event.accepted = true;
            }
        }
    }
}
