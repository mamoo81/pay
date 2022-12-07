/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
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

Item {
    id: objectShaker
    property Item target: parent

    // center target in its parent.
    Component.onCompleted: target.x = (target.parent.width - target.width) / 2;

    function start() {
        if (anim.running)
            return;
        anim.loop = 0;
        anim.from = target.x;
        anim.to = target.x - 5;
        anim.start()
    }
    // keep target centerd in its parent
    Connections {
        target: objectShaker.target
        function onWidthChanged() {
            // we assume it will be centered in its parent
            target.x = (target.parent.width - target.width) / 2
        }
    }

    NumberAnimation {
        id: anim
        property int loop: 0
        target: objectShaker.target
        property: "x"
        duration: 30
        easing.type: Easing.InOutQuad
        onFinished: {
            if (loop === 0) {
                from = to
                to = to + 10
            }
            else if (loop === 1) {
                from = to
                to = to - 10
            }
            else if (loop === 2) {
                from = to
                to = to + 5
            }

            if (loop < 3) {
                loop = loop + 1
                start();
            }
        }
    }
}
