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
import "../Flowee" as Flowee

FocusScope {
    id: root
    anchors.fill: parent
    enabled: thePopup.visible

    property bool isOpen: false;

    /**
      * @param sourceComponent is a Component we set on the loader.
      * @param target is the visual item we position next to.
      * @returns the item instance of the sourceComponent template
      */
    function open(sourceComponent, target) {
        thePopup.palette = mainWindow.palette
        thePopup.width = target.width
        thePopup.x = (width - target.width) / 2
        thePopup.sourceRect = root.mapFromItem(target, 0, 0, target.width, target.height);
        loader.sourceComponent = sourceComponent; // last, as it starts the loading
        return loader.item;
    }
    function close() {
        thePopup.visible = false;
    }

    QQC2.Popup {
        id: thePopup
        width: parent.width
        height: 100
        modal: true
        closePolicy: QQC2.Popup.CloseOnEscape + QQC2.Popup.CloseOnReleaseOutside
        property rect sourceRect: Qt.rect(0, 0, 0, 0)
        onVisibleChanged: {
            if (!visible) { // closing
                loader.sourceComponent = undefined;
            }
            root.isOpen = visible; // ensure listeners of that property get notified after we acted on visibility changes.
        }
        background: Rectangle {
            color: mainWindow.palette.light
            border.color: mainWindow.palette.highlight
            border.width: 1
            radius: 5
        }
        Loader {
            id: loader
            anchors.fill: parent
            onLoaded: {
                thePopup.height = item.implictHeight
                var h = item.implicitHeight
                var rect = thePopup.sourceRect;
                if (root.height - rect.bottom >= h) // fits below
                    thePopup.y = rect.bottom;
                else if (h < rect.y) // fits above
                    thePopup.y = rect.y - h;
                else
                    thePopup.y = root.height - h; // make it bottom aligned, even if it overlaps
                thePopup.visible = true;
                root.forceActiveFocus();
            }
        }
    }

    Keys.onReleased: (event)=> {
        if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
            event.accepted = true;
            root.close();
        }
    }
}
