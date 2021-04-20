import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Layouts 1.14
import Flowee.org.pay 1.0

import "./ControlColors.js" as ControlColors

Row {
    id: floweeTabBar
    property int currentIndex: -1
    onCurrentIndexChanged: {
        let tabIndex = currentIndex;
        for (let i in children) {
            children[i].isActive = (i == tabIndex) // this requires 2 not 3 equals to work!
        }
    }
}
