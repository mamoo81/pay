/*
 * This file is part of the Flowee project
 * Copyright (C) 2020-2022 Tom Zander <tom@flowee.org>
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
import "." as Flowee;

Dialog {
    id: passwdDialog
    property string pwd: ""

    function start() {
        contentComponent = textEntryField
        visible = true
    }
    onClosed: {
        pwd = ""
        contentComponent = null;
    }

    Component {
        id: textEntryField
        Flowee.TextField {
            echoMode: TextInput.Password
            onTextChanged: passwdDialog.pwd = text
            onAccepted: passwdDialog.accept();
            focus: true
        }
    }
}