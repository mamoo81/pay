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
import Flowee.org.pay;

Item {
    id: root

    implicitWidth: 300
    implicitHeight: 600

    /// This will hold the password when the user is done typing it.
    property string password : "";
    property alias buttonText: openButton.text

    /// Emitted when the user submits the password
    signal passwordEntered;

    /// in an onPasswordEntered callback, call this method
    /// to signify the password is incorrect.
    /// Otherwise just close this widget, or call acceptedPassword()
    function passwordIncorrect() {
        shaker.start();
        moveFocusTimer.start();
    }
    /// clears the password typed, if needed.
    function acceptedPassword() {
        pwdField.text = "";
    }

    function takeFocus() {
        moveFocusTimer.start();
    }

    // we can't move focus from things like the onClicked handler of a button
    // therefore a little timer that does it very briefly afterwards is used.
    Timer {
        id: moveFocusTimer
        interval: 50
        onTriggered: {
            if (!switchButton.numericInput) {
                pwdField.selectAll();
                pwdField.forceActiveFocus();
            }
        }
    }

    Image {
        id: lockIcon
        source: "qrc:/lock" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
        width: 80
        height: 80
        anchors.horizontalCenter: parent.horizontalCenter
        smooth: true
        y: 40
    }

    Image {
        id: switchButton
        property bool numericInput: true

        width: numericInput ? 40 : 30
        height: width
        anchors.right: parent.right
        source: "qrc:/" + (numericInput ? "keyboard" : "num-keyboard")
                    + (Pay.useDarkSkin ? "-light.svg" : ".svg");

        MouseArea {
            anchors.fill: parent
            onClicked: {
                keyboard.dataInput.editor.reset();
                var newValue = !switchButton.numericInput;
                switchButton.numericInput = newValue;
                if (newValue === false)
                    pwdField.forceActiveFocus();
            }
        }
    }

    Flowee.Label {
        id: introText
        text: qsTr("Enter your wallet passcode")
        anchors.top: lockIcon.bottom
        anchors.topMargin: 20
        Flowee.ObjectShaker { id: shaker } // 'shake' to give feedback on mistakes
    }

    // Show the typed pin code, but as bullets as you type.
    Row {
        anchors.top: introText.bottom
        anchors.topMargin: 20
        spacing: 10
        anchors.horizontalCenter: parent.horizontalCenter
        visible: switchButton.numericInput
        Repeater {
            id: repeater
            // take the number typed and turn it into an array of characters.
            model: {
                var inputString = keyboard.dataInput.editor.enteredString
                var answer = [];
                for (let i = 0; i < inputString.length; ++i) {
                    answer.push(inputString.substr(i, 1));
                }
                return answer;
            }

            Flowee.Label {
                text: {
                    if (index !== keyboard.dataInput.editor.insertedIndex)
                        return "∙"
                    return modelData;
                }
                font.pixelSize: introText.font.pixelSize * 2

                Timer {
                    interval: 1000
                    running: index === keyboard.dataInput.editor.insertedIndex
                    onTriggered: text = "∙"
                }
            }
        }
    }

    NumericKeyboardWidget {
        id: keyboard

        x: switchButton.numericInput ? 0 : 0 - parent.width
        y: parent.height - openButton.height - height - 20
        width: parent.width
        dataInput: Item {
            property QtObject editor: Item {
                property string enteredString;
                property int insertedIndex: -1
                function insertNumber(character) {
                    if (enteredString.length >= 10)
                        return false;
                    insertedIndex = enteredString.length;
                    enteredString = enteredString + character;
                    return true;
                }
                function addSeparator() { return false; }
                function backspacePressed() {
                    if (enteredString == "")
                        return false;
                    insertedIndex = -1;
                    enteredString = enteredString.substring(0, enteredString.length - 1);
                    return true;
                }
                function reset() {
                    insertedIndex = -1;
                    enteredString = "";
                }
            }
            function shake() { shaker.start(); }
        }
        Behavior on x { NumberAnimation { } }
    }

    Flowee.TextField {
        id: pwdField
        x: switchButton.numericInput ? parent.width + 30 : 0
        visible: !switchButton.numericInput
        anchors.top: introText.bottom
        anchors.topMargin: 20
        width: parent.width
        focus: switchButton.numericInput === false
        inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhSensitiveDataTyped
                          | (hideText ? Qt.ImhHiddenTextCharacters : Qt.ImhNone)
        echoMode: hideText ? TextInput.Password : TextInput.Normal

        property bool hideText: true
        onEditingFinished: openButton.clicked()

        Image {
            width: 14
            height: 14
            anchors.right: parent.right
            anchors.rightMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            source: {
                var state = (pwdField.hideText) ? "open" : "closed";
                var skin = Pay.useDarkSkin ? "-light" : ""
                return "qrc:/eye-" + state + skin + ".png";
            }

            MouseArea {
                width: parent.width + 20 // extend to right physical edge
                x: -5
                y: 0 - parent.y - 5
                height: parent.height + 10
                onClicked: pwdField.hideText = !pwdField.hideText
            }
        }
        Behavior on x { NumberAnimation { } }
    }

    Flowee.Button {
        id: openButton
        anchors.right: parent.right
        y: {
            if (switchButton.numericInput)
                return parent.height - height
            return pwdField.y + pwdField.height + 10;
        }

        text: qsTr("Open", "open wallet with PIN")

        onClicked: {
            if (switchButton.numericInput)
                var pwd = keyboard.dataInput.editor.enteredString;
            else
                pwd = pwdField.text;
            if (pwd !== "") {
                root.password = pwd;
                keyboard.dataInput.editor.reset();
                root.passwordEntered();
            }
        }
    }
}
