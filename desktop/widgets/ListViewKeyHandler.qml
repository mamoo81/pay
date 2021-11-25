/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2021 Tom Zander <tom@flowee.org>
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

/*
 * Typical usage is to add this in a place that is the 'root' of the focus.
 * For instance a Pane that is the root on a tab or a window.
 *
 * This will make sur that 'myLstView' gets keyboard handling.
 * @code
        Keys.forwardTo: Flowee.ListViewKeyHandler {
            target: myListView
        }
 * @endcode
 */

Item {
    id: root
    property ListView target: parent

    Keys.onPressed: {
        if (root.target == null)
            return;
        if (event.key === Qt.Key_Down) {
            root.target.flick(0, -500);
            event.accepted = true;
        }
        else if (event.key === Qt.Key_Up) {
            root.target.flick(0, 500);
            event.accepted = true;
        }
        else if (event.key === Qt.Key_PageUp) {
            root.target.flick(0, Math.sqrt(2 * 1000 * root.target.height));
            event.accepted = true;
        }
        else if (event.key === Qt.Key_PageDown) {
            root.target.flick(0, -Math.sqrt(2 * 1000 * root.target.height));
            event.accepted = true;
        }
        else if (event.key === Qt.Key_Home) {
            root.target.contentY = 10;
            root.target.flick(0, 500);
            event.accepted = true;
        }
        else if (event.key === Qt.Key_End) {
            root.target.contentY = root.target.contentHeight - root.target.height - 10;
            root.target.flick(0, -500);
            event.accepted = true;
        }
    }
}

