import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 2.15
import "../Flowee" as Flowee

Item {
    id: accountHistory
    property string icon: "qrc:/bla"
    ColumnLayout {
        spacing: 10
        width: parent.width - 20
        x: 10

        Item {
            id: walletSelector
            height: currentWalletName.height

            Flowee.Label {
                id: currentWalletName
                text: "Daily Wallet"
            }
        }
        Flowee.Label {
            id: balanceLabel
            text: "€ 100";
        }

        GridLayout {
            width: parent.width
            columns: 3

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 60
                height: 60
                radius: 30
                color: "yellow"
            }
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 60
                height: 60
                radius: 30
                color: "yellow"
            }
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 60
                height: 60
                radius: 30
                color: "yellow"
            }
        }

        Item {
            width: parent.width
            height: 300
            Rectangle {
                width: parent.width - 30
                x: 15
                radius: 10
                height: 300
                color: mainWindow.palette.window
            }
        }
    }
}
