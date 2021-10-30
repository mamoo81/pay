/* * This file is part of the Flowee project
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
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import Flowee.org.pay 1.0

/*
 * Annoyingly, there is a gap in the Qt default components.
 * The textfield is per definition one line, the textArea is overly complex and really not meant
 * as a basic input widget.
 * The TextEdit (used below) is the core component, but its not a Control and doesn't integrate
 * well at all.
 *
 * So, here we go, a basic multi line line-edit.
 */
Control {
    id: root

    property string text: ""
    property string placeholderText: ""
    property var nextFocusTarget: null
    signal editingFinished;
    property alias readOnly: textEdit.readOnly

    implicitHeight: textEdit.implicitHeight + 20
    implicitWidth: 100
    width: 100
    height: implicitHeight

    Connections {
        function onTextChanged() { textEdit.text = text; }
        function onPlaceholderTextChanged() {
            if (textEdit.text === "") {
                textEdit.showingPlaceholder = true
                textEdit.text = placeholderText
            }
        }
    }

    // init
    Component.onCompleted: {
        if (text === "" && placeholderText !== "") {
            textEdit.showingPlaceholder = true
            textEdit.text = placeholderText
        }
        else {
            textEdit.text = text
        }
    }

    TextEdit {
        id: textEdit
        x: 10
        y: 10
        width: parent.width - 20
        height: parent.height - 20
        activeFocusOnTab: true
        color: showingPlaceholder ? Qt.darker(root.palette.text, Flowee.useDarkSkin ? 1.6 : 0.65) : root.palette.text
        selectedTextColor: root.palette.highlightedText
        selectionColor: root.palette.highlight
        selectByMouse: true
        wrapMode: TextEdit.WordWrap

        property bool showingPlaceholder: false
        onActiveFocusChanged: {
            if (activeFocus && showingPlaceholder) {
                showingPlaceholder = false;
                text = ""
            }
            else if (!activeFocus && !showingPlaceholder && text === "") {
                showingPlaceholder = true;
                text = root.placeholderText
            }
        }
        onTextChanged: if (!showingPlaceholder) root.text = text
        Keys.onPressed: {
            if (event.key === Qt.Key_Tab && root.nextFocusTarget != null) {
                // don't accept the tab, make it change focus
                event.accepted = true;
                root.nextFocusTarget.forceActiveFocus()
            }
            else if (showingPlaceholder && event.text !== "") {
                showingPlaceholder = false;
                text = ""
                // don't accept the event to end up typing it.
            }
        }

        onEditingFinished: root.editingFinished()
    }

    background: Rectangle {
        color: "#00000000"
        border.color: textEdit.activeFocus ? root.palette.highlight : root.palette.mid
        border.width: 1.5
    }
}
