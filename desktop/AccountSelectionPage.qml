import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import Flowee.org.pay 1.0

Pane {
    ColumnLayout {
        anchors.fill: parent
        Label {
            text: qsTr("Pick an account:")
        }
        ListView {
            width: parent.width
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: {
                if (typeof wallets === "undefined")
                    return 0;
                return wallets.accounts;
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
                        text: Flowee.priceToString(modelData.balance)
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
                    onClicked: wallets.current = modelData
                }
            }
        }
    }
}
