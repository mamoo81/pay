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

Flow {
    id: root
    Repeater {
        model: 12
        delegate: Item {
            width: root.width / 3
            height: textLabel.height + 20
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
                    let editor = priceInput.editor;
                    if (index < 9) // these are digits
                        var shake = !editor.insertNumber("" + (index + 1));
                    else if (index === 9)
                        shake = !editor.addSeparator()
                    else if (index === 10)
                        shake = !editor.insertNumber("0");
                    else
                        shake = !editor.backspacePressed();
                    if (shake)
                        priceInput.shake();
                }
                onClicked: doSomething();
                onDoubleClicked: doSomething();
            }
        }
    }
}

