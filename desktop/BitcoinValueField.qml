import QtQuick 2.14
import Flowee.org.pay 1.0

FocusScope {
    id: root
    height: balance.height + 16
    width: balance.width + 16
    focus: true
    activeFocusOnTab: true

    property alias value: privValue.value
    property alias valueObject: privValue

    function reset() {
        privValue.enteredString = "0";
        privValue.value = 0;
    }

    BitcoinValue {
        id: privValue
    }
    MouseArea {
        anchors.fill: parent
        onClicked: root.focus = true
    }
    
    BitcoinAmountLabel {
        id: balance
        x: 8
        y: 8
        value: root.value
        colorize: false
        visible: !root.activeFocus
    }
    
    Text {
        text: privValue.enteredString
        visible: root.activeFocus
        x: 8
        y: 8
    }
    Text {
        id: unit
        text: Flowee.unitName
        y: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        visible: root.activeFocus
    }
    
    Rectangle { // focus scope indicator
        anchors.fill: parent
        border.color: root.activeFocus ? "#0066ff" : "#bdbdbd"
        border.width: 1
        color: "#00000000" // transparant
    }
    
    Keys.onPressed: {
        if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            privValue.addNumber(event.key);
            event.accepted = true
        }
        else if (event.key === 44 || event.key === 46) {
            privValue.addSeparator();
            event.accepted = true
        }
        else if (event.key === Qt.Key_Backspace) {
            privValue.backspacePressed();
            event.accepted = true
        }
    }
}
