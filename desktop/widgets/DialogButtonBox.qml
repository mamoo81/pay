/*
 * This file is part of the Flowee project
 * Copyright (C) 2021 Tom Zander <tom@flowee.org>
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
import QtQuick.Layouts 1.11

RowLayout {
    id: root
    property alias alignment: box.alignment
    property alias buttonLayout: box.buttonLayout
    property alias delegate: box.delegate
    property alias position: box.position
    property alias standardButtons: box.standardButtons

    signal accepted;
    signal discarded;
    signal rejected;

    Item { width: 1; height: 1; Layout.fillWidth: true } // spacer
    QQC2.DialogButtonBox {
        id: box
        // next line is a hack to give spacing between the buttons.
        Component.onCompleted: children[0].spacing = 10 // In Qt5.15 the first child is ListView

        onAccepted: root.accepted();
        onDiscarded: root.discarded();
        onRejected: root.rejected();
    }
}

