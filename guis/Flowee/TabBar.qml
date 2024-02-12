/*
 * This file is part of the Flowee project
 * Copyright (C) 2021-2024 Tom Zander <tom@flowee.org>
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
        for (let i = 0; i < header.tabs.length; ++i) {
            let on = i === currentIndex;
            let child = header.tabs[i];
            child.visible = on;
            if (on)
                visibleChild = child;
        }
        // try to make sure the current tab gets keyboard focus properly.
        // We do some extra work in case the tab itself is a loader.
        if (visibleChild) {
            forceActiveFocus();
            visibleChild.focus = true
            if (visibleChild instanceof Loader) {
                var child = visibleChild.item;
                if (child !== null)
                    child.focus = true;
            }
        }
    }

    Row {
        id: header
        property var tabs: []
        onTabsChanged: floweeTabBar.setOpacities();
        Repeater {
            model: header.tabs
            delegate: Item {
                height: payTabButtonText.height + 10
                width: floweeTabBar.width / header.tabs.length

                Item { // text + icon, centered together.
                    width: {
                        var w = payTabButtonText.implicitWidth;
                        if (tabIcon.visible)
                            w += 6 + tabIcon.width;
                        if (w > parent.width) // limit text to available space
                            w = parent.width
                        return w;
                    }
                    height: parent.height
                    anchors.horizontalCenter: parent.horizontalCenter

                    Image {
                        id: tabIcon
                        visible: source != ""
                        source: enabled ? header.tabs[index].icon : ""
                        anchors.bottom: payTabButtonText.baseline
                        anchors.bottomMargin: -2
                        height: payTabButtonText.height
                        width: height
                    }
                    Label {
                        id: payTabButtonText
                        y: 6
                        x: tabIcon.visible ? tabIcon.width + 6 : 0
                        width: parent.width - x;
                        elide: Text.ElideMiddle
                        horizontalAlignment: Text.AlignHCenter
                        font.bold: true
                        color: {
                            if (!header.tabs[index].enabled)
                                return "#888"
                            return "white"
                        }
                        text: enabled ? header.tabs[index].title: ""
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
                        if (!header.tabs[index].enabled)
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
                        // respect 'disabled' bool and don't change to the tab
                        if (header.tabs[index].enabled)
                            floweeTabBar.currentIndex = index
                    }
                }
            }
        }
    }

    Item {
        id: stack
        onChildrenChanged: {
            // copy the children, but skip Items that are not actually
            // content. Like Repeaters
            var copyOfChildren = [];
            for (let i = 0; i < children.length; ++i) {
                var item = children[i];
                if (item instanceof Repeater)
                    continue;
                copyOfChildren.push(item);
            }
            header.tabs = copyOfChildren;
        }

        width: floweeTabBar.width
        anchors.top: header.bottom; anchors.bottom: floweeTabBar.bottom
    }
}
