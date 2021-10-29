import QtQuick 2.11
import QtQuick.Controls 2.11
import QtQuick.Layouts 1.11
import Flowee.org.pay 1.0

Item {
    id: closeIcon
    width: 30
    height: 30
    MouseArea {
        anchors.fill: parent
        onClicked: accountDetails.state = "showTransactions"
    }

    Rectangle {
        color: "#bbbbbb"
        width: 30
        height: 3
        radius: 3
        rotation: 45
        anchors.centerIn: parent
    }
    Rectangle {
        color: "#bbbbbb"
        width: 30
        height: 3
        radius: 3
        rotation: -45
        anchors.centerIn: parent
    }
}
