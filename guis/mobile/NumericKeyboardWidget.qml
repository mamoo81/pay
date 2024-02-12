/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2024 Tom Zander <tom@flowee.org>
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

Item {
    id: root
    implicitHeight: 70 * 4
    height: 90 * 4

    required property var dataInput;

    Rectangle {
        // when the typed items are not allowd
        // by the back-end, we flash this red area for a very brief time
        // to let the user know.
        // notice we also 'shake' the actual typed text, but if the user is
        // focused on this widget they might miss that.
        // Useful since some currencies don't allow dots/commas, which should
        // be clearly communicated to the user!
        id: errorFeedback
        anchors.fill: parent
        radius: 20
        color: mainWindow.errorRedBg
        opacity: 0

        Timer {
            interval: 100
            running: errorFeedback.opacity > 0
            repeat: true
            onTriggered: {
                anim.duration = 400
                errorFeedback.opacity = 0
            }
        }

        Behavior on opacity {
            NumberAnimation {
                id: anim
                duration: 50
                easing.type: Easing.InOutQuad
            }
        }
    }

    Flow {
        id: flowLayout
        width: parent.width
        Repeater {
            model: 12
            delegate: Item {
                width: root.width / 3
                height: root.height / 4
                QQC2.Label {
                    id: textLabel
                    anchors.centerIn: parent
                    font.pixelSize: 28
                    text: {
                        if (index < 9)
                            return index + 1;
                        if (index === 9)
                            return Qt.locale().decimalPoint
                        if (index === 10)
                            return "0"
                        return ""; // index === 11, the backspace.
                    }
                    // make dim when not enabled.
                    color: {
                        if (!enabled)
                            return Pay.useDarkSkin ? Qt.darker(palette.buttonText, 2) : Qt.lighter(palette.buttonText, 2);
                        return palette.windowText;
                    }
                }
                Image {
                    visible: index === 11
                    anchors.centerIn: parent
                    source: index === 11 ? ("qrc:/backspace" + (Pay.useDarkSkin ? "-light" : "") + ".svg") : ""
                    width: 27
                    height: 17
                    opacity: enabled ? 1 : 0.4
                }

                MouseArea {
                    anchors.fill: parent
                    function doSomething() {
                        let editor = dataInput.editor;
                        if (index < 9) // these are digits
                            var fail = !editor.insertNumber("" + (index + 1));
                        else if (index === 9)
                            fail = !editor.addSeparator()
                        else if (index === 10)
                            fail = !editor.insertNumber("0");
                        else
                            fail = !editor.backspacePressed();
                        if (fail) {
                            anim.duration = 40
                            errorFeedback.opacity = 0.7
                            dataInput.shake();
                        }
                    }
                    onClicked: doSomething();
                    onDoubleClicked: doSomething();
                }
            }
        }
    }
}

