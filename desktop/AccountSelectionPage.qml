/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tom@flowee.org>
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
import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import Flowee.org.pay 1.0

Pane {
    ColumnLayout {
        anchors.fill: parent
        Label {
            text: qsTr("Pick an account") + ":"
        }
        ListView {
            width: parent.width
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: {
                if (typeof portfolio === "undefined")
                    return 0;
                return portfolio.accounts;
            }

            delegate: Rectangle {
                width: parent.width
                height: grid.height
                color: index % 2 === 0 ? mainWindow.palette.button : mainWindow.palette.alternateBase
                GridLayout {
                    width: parent.width
                    id: grid
                    columns: 3
                    Label {
                        id: name
                        font.bold: true
                        text: modelData.name
                        Layout.fillWidth: true
                    }
                    Label {
                        text: Flowee.priceToString(modelData.balanceUnconfirmed + modelData.balanceConfirmed + modelData.balanceImmature)
                        Layout.alignment: Qt.AlignRight
                    }
                    Label {
                        text: Flowee.unitName
                    }
                    Item { width: 1; height: 1} // spacer
                    Label {
                        text: "0.00"
                        Layout.alignment: Qt.AlignRight
                    }
                    Label {
                        text: "Euro"
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: portfolio.current = modelData
                }
            }
        }
    }
}
