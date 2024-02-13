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
    height: contentHeight

    required property var dataInput;
    /**
     * The keyboard can have a separator in the bottom left position,
     * which is either a dot or a comma based on locale.
     * Or if this property is false, we put the backspace there and
     * reserve the bottom right button for finished()
     */
    property bool hasSeparator: true

    /// emitted when the user used the 'Ok' button. @see hasSeparator
    signal finished;

    // We export an implicitHeight that is rather small,
    // the parent widget may consider giving this thing a LOT more
    // space, though. Which equally looks bad.
    // So here we have a maximum height we'll actually occupy for
    // usability sake
    property int contentHeight: {
        // we aim to be have buttons no taller than 18mm
        var realPixelSize = Screen.pixelDensity * 18;
        return Math.min(height, realPixelSize * 4)
    }

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
        anchors.bottom: parent.bottom
        Repeater {
            model: 12
            delegate: Item {
                width: root.width / 3
                height: root.contentHeight / 4
                Rectangle {
                    id: backgroundCircle
                    width: Math.min(parent.width, parent.height) - 6
                    height: width
                    anchors.centerIn: parent
                    radius: pressed ? 20 : height

                    property bool pressed: false
                    color: {
                        if (pressed || index === 11 || index === 9)
                            return palette.midlight
                        return palette.mid
                    }
                    opacity: enabled ? 1 : 0
                    Behavior on opacity { NumberAnimation { } }
                    Behavior on color { ColorAnimation { } }
                    Behavior on radius { NumberAnimation { duration: 100 } }

                    Timer {
                        interval: 200
                        running: parent.pressed
                        onTriggered: parent.pressed = false
                    }
                }
                QQC2.Label {
                    id: textLabel
                    anchors.centerIn: parent
                    font.pixelSize: 28
                    text: {
                        if (index < 9)
                            return index + 1;
                        if (index === 9 && root.hasSeparator)
                            return Qt.locale().decimalPoint
                        if (index === 10)
                            return "0"
                        return "";
                    }
                    // make dim when not enabled.
                    color: {
                        if (!enabled)
                            return Pay.useDarkSkin ? Qt.darker(palette.buttonText, 2) : Qt.lighter(palette.buttonText, 2);
                        return palette.windowText;
                    }
                }
                Image {
                    visible: index === 11 || (!root.hasSeparator && index == 9)
                    anchors.centerIn: parent
                    source: {
                        let base = "";
                        if ((root.hasSeparator && index == 11)
                                || (!root.hasSeparator && index == 9))
                            base = "qrc:/backspace";
                        else if (!root.hasSeparator && index == 11)
                            base = "qrc:/confirmIcon";
                        if (base === "")
                            return base;
                        return base + (Pay.useDarkSkin ? "-light" : "") + ".svg";
                    }
                    width: 22
                    height: 15
                    opacity: enabled ? 1 : 0.4
                }

                MouseArea {
                    anchors.fill: parent
                    function doSomething() {
                        let editor = dataInput.editor;
                        let fail = false;
                        if (index < 9) // these are digits
                            fail = !editor.insertNumber("" + (index + 1));
                        else if (index === 9 && root.hasSeparator)
                            fail = !editor.addSeparator()
                        else if (index === 10)
                            fail = !editor.insertNumber("0");
                        else if (index === 11 && !root.hasSeparator) {
                            fail = editor.enteredString === ""
                            if (!fail)
                                root.finished();
                        }
                        else
                            fail = !editor.backspacePressed();
                        if (fail) {
                            anim.duration = 40
                            errorFeedback.opacity = 0.7
                            dataInput.shake();
                        }
                        else {
                            backgroundCircle.pressed = true;
                        }
                    }
                    onClicked: doSomething();
                    onDoubleClicked: doSomething();
                }
            }
        }
    }
}

