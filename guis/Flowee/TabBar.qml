/*
 * This file is part of the Flowee project
 * Copyright (C) 2021-2022 Tom Zander <tom@flowee.org>
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
import QtQuick 2.11
import QtQuick.Controls 2.11 as QQC2

FocusScope {
    id: floweeTabBar

    // This trick  means any child items the FloweeTabBar are actually added to the 'stack' item's children.
    default property alias content: stack.children
    property int currentIndex: 0
    property alias headerHeight: header.height
    onCurrentIndexChanged: setOpacities()
    Component.onCompleted: setOpacities()

    function setOpacities() {
        var visibleChild = null;
        for (let i = 0; i < stack.children.length; ++i) {
            let on = i === currentIndex;
            let child = stack.children[i];
            child.visible = on;
            if (on)
                visibleChild = child;
        }

        // try to make sure the current tab gets keyboard focus properly.
        // We do some extra work in case the tab itself is a loader.
        forceActiveFocus();
        visibleChild.focus = true
        if (visibleChild instanceof Loader) {
            var child = visibleChild.item;
            if (child != null)
                child.focus = true;
        }
    }

    Row {
        id: header

        Repeater {
            model: stack.children.length
            delegate: Item {
                height: payTabButtonText.height + 10
                width: floweeTabBar.width / stack.children.length;

                Row {
                    height: parent.height
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 6
                    Image {
                        visible: source != ""
                        source: stack.children[index].icon
                        anchors.bottom: payTabButtonText.baseline
                        anchors.bottomMargin: -2
                        width: height
                        height: payTabButtonText.height
                    }
                    QQC2.Label {
                        id: payTabButtonText
                        anchors.verticalCenter: parent.verticalCenter
                        font.bold: true
                        color: {
                            var child = stack.children[index];
                            if (!child.enabled)
                                return "#888"
                            return "white"
                        }
                        text: stack.children[index].title
                    }
                }

                Rectangle {
                    id: highlight
                    width: parent.width - 6
                    x: 1
                    height: 2
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    property bool hover: false
                    color: {
                        if (index === floweeTabBar.currentIndex)
                            return mainWindow.floweeGreen
                        var child = stack.children[index];
                        if (!child.enabled)
                            return "#888"
                        if (Pay.useDarkSkin) {
                            if (hover)
                                return mainWindow.floweeSalmon
                            return "#EEE"
                        }
                        // light skin
                        if (hover)
                            return mainWindow.floweeSalmon
                        return mainWindow.floweeBlue
                    }
                }
                MouseArea {
                    hoverEnabled: true
                    anchors.fill: parent
                    onEntered: highlight.hover = true
                    onExited: highlight.hover = false
                    onClicked: {
                        let child = stack.children[index];
                        // respect 'disabled' bool and don't change to the tab
                        if (child.enabled)
                            floweeTabBar.currentIndex = index
                    }
                }
            }
        }
    }

    Item {
        id: stack
        width: floweeTabBar.width
        anchors.top: header.bottom; anchors.bottom: floweeTabBar.bottom
    }
}
