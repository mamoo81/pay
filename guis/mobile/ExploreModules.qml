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
import QtQuick.Layouts
import "../Flowee" as Flowee

Page {
    id: root
    headerText: qsTr("Explore")

    Item {
        anchors.fill: parent
        // clip: true

        Flickable {
            anchors.fill: parent
            anchors.topMargin: 10
            anchors.bottomMargin: 10
            contentWidth: width
            contentHeight: content.height

            Column {
                id: content
                width: parent.width
                spacing: 15

                Repeater {
                    model: ModuleManager.registeredModules
                    Rectangle {
                        width: root.width - 30
                        height: 35 + titleLabel.height + statusField.height + Math.min(120, descriptionLabel.implicitHeight)
                        radius: 20
                        color: palette.alternateBase
                        border.width: 1
                        border.color: palette.midlight

                        Flowee.Label {
                            id: titleLabel
                            y: 15
                            text: modelData.title
                            font.bold: true
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Image {
                            x: 10
                            width: 42
                            anchors.top: descriptionFrame.top
                            source: modelData.iconSource
                            visible: modelData.iconSource !== ""
                            smooth: true
                            fillMode: Image.PreserveAspectFit
                        }

                        Item {
                            id: descriptionFrame
                            anchors.top: titleLabel.bottom
                            anchors.topMargin: 10
                            width: parent.width - (modelData.iconSource === "" ? 0 : 52)
                            clip: true
                            anchors.bottom: statusField.top
                            anchors.bottomMargin: 10
                            anchors.right: parent.right
                            z: 10
                            Rectangle {
                                color: palette.alternateBase
                                anchors.fill: descriptionLabel
                            }
                            Flowee.Label {
                                id: descriptionLabel
                                width: parent.width - 20
                                x: 10
                                text: modelData.description
                                horizontalAlignment: Text.AlignJustify
                                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: descriptionFrame.clip = !descriptionFrame.clip
                                }
                            }
                        }

                        Image {
                            id: enabledIndicator
                            source: "qrc:/ribbon.svg"
                            smooth: true
                            anchors.right: parent.right
                            anchors.rightMargin: -8
                            y: -9
                            width: 90
                            height: 90
                            opacity: modelData.enabled ? 1 : 0
                            visible: opacity > 0
                            Behavior on opacity { NumberAnimation { } }

                            Item {
                                rotation: 45
                                width: 80
                                height: 22
                                x: 16
                                y: 22
                                clip: true

                                Text {
                                    color: "white"
                                    text: qsTr("ON", "Enabled. SHORT TEXT!");
                                    font.bold: true
                                    anchors.centerIn: parent
                                }
                            }
                        }

                        Item {
                            id: statusField
                            width: parent.width
                            height: 40
                            anchors.bottom: parent.bottom
                            z: 1

                            Row {
                                spacing: 10
                                anchors.fill: parent
                                anchors.leftMargin: 10

                                Flowee.CheckBox {
                                    anchors.verticalCenter: parent.verticalCenter
                                    checked: modelData.enabled
                                    onClicked: {
                                        var newValue = checked;
                                        for (let s of modelData.sections) {
                                            s.enabled = newValue;
                                        }
                                    }
                                }

                                // one icon per section-type
                                Image {
                                    source: "qrc:/sending" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
                                    width: 16
                                    height: width
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: section != null
                                    opacity: (visible && section.enabled) ? 0.8 : 0.3
                                    property QtObject section: {
                                        // find the section our icon represents.
                                        for (let s of modelData.sections) {
                                            if (s.isSendMethod)
                                                return s;
                                        }
                                        return null; // module doesn't have such a section.
                                    }
                                }

                                Image {
                                    source: "qrc:/module-mainmenu" + (Pay.useDarkSkin ? "-light.svg" : ".svg");
                                    width: 16
                                    height: width
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: section != null
                                    opacity: (visible && section.enabled) ? 0.8 : 0.3
                                    property QtObject section: {
                                        // find the section our icon represents.
                                        for (let s of modelData.sections) {
                                            if (s.isMainMenuMethod)
                                                return s;
                                        }
                                        return null; // module doesn't have such a section.
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
