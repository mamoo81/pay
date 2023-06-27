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
    anchors.fill: parent
    anchors.margins: 10

    property bool numericInput: true

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
        width: parent.numericInput ? 40 : 30
        height: width
        anchors.right: parent.right
        source: "qrc:/" + (parent.numericInput ? "keyboard" : "num-keyboard")
                    + (Pay.useDarkSkin ? "-light.svg" : ".svg");

        MouseArea {
            anchors.fill: parent
            onClicked: {
                keyboard.dataInput.editor.reset();
                var newValue = !root.numericInput;
                root.numericInput = newValue;
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
        visible: parent.numericInput
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
        x: parent.numericInput ? 0 : 0 - parent.width
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
        x: parent.numericInput ? parent.width + 10 : 0
        anchors.top: introText.bottom
        anchors.topMargin: 20
        width: parent.width
        focus: parent.numericInput === false
        onEditingFinished: openButton.clicked()
        echoMode: TextInput.Password
        Behavior on x { NumberAnimation { } }

        Image {
            width: 14
            height: 14
            anchors.right: parent.right
            anchors.rightMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            source: {
                var state = (pwdField.echoMode === TextInput.Normal) ? "closed" : "open";
                var skin = Pay.useDarkSkin ? "-light" : ""
                return "qrc:/eye-" + state + skin + ".png";
            }

            MouseArea {
                width: parent.width + 20 // extend to right physical edge
                x: -5
                y: 0 - parent.y - 5
                height: parent.height + 10
                onClicked: {
                    var old = pwdField.echoMode;
                    if (old == TextInput.Normal)
                        pwdField.echoMode = TextInput.Password
                    else
                        pwdField.echoMode = TextInput.Normal
                }
            }
        }
    }

    Flowee.Button {
        id: openButton
        anchors.right: parent.right
        y: {
            if (parent.numericInput)
                return parent.height - height
            return pwdField.y + pwdField.height + 10;
        }

        text: qsTr("Open", "open wallet with PIN")

        onClicked: {
            if (root.numericInput)
                var pwd = keyboard.dataInput.editor.enteredString;
            else
                pwd = pwdField.text;
            if (pwd !== "") {
                portfolio.current.decrypt(pwd);
                if (!portfolio.current.isDecrypted) {
                    keyboard.dataInput.editor.reset();
                    shaker.start();
                    if (!numericInput) {
                        pwdField.selectAll();
                        pwdField.forceActiveFocus();
                    }
                }
            }
        }
    }
}
