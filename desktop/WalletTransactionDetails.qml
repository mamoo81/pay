/*
 * This file is part of the Flowee project
 * Copyright (C) 2020 Tom Zander <tomz@freedommail.ch>
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

GridLayout {
    id: root

    property QtObject infoObject: null

    columns: 2
    Label {
        Layout.columnSpan: 2
        text: model.txid
    }
    Label {
        text: qsTr("Status:")
    }
    Label {
        text: {
            if (typeof  model.date === "undefined")
                return qsTr("unconfirmed")
            return qsTr("%1 confirmations (mined in block %2)")
                .arg(Flowee.headerChainHeight - model.height).arg(model.height);
        }
    }

    Label {
        text: mainLabel.text
    }
    BitcoinAmountLabel {
        value: model.fundsOut - model.fundsIn
    }
    Label {
        text: qsTr("Size:")
    }
    Label {
        text: qsTr("%1 bytes").arg(infoObject.size)
    }
    Label {
        text: qsTr("Inputs:")
    }
    ColumnLayout {
        Repeater {
            model: infoObject.inputs
            delegate:
                Label {
                    text: index + " " + modelData.address + " / " + modelData.value
                }
        }
    }
    Label {
        text: qsTr("Outputs:")
    }
    ColumnLayout {
        Repeater {
            model: infoObject.outputs
            delegate:
                Label {
                    text: index + " " + modelData.address + " / " + modelData.value + " spent: " + modelData.spent
                }
        }
    }
}
