import QtQuick 2.14
import QtQuick.Controls 2.14
import Flowee.org.pay 1.0

/*
 * A simple label that shows the amount of time a pair of block-heights are apart.
 */
Label {
    id: syncIndicator
    property int accountBlockHeight: 0
    property int globalPos: Flowee.headerChainHeight

    text: {
        var diff = globalPos - accountBlockHeight
        visible = diff !== 0;
        if (diff < 0) // we are ahead???
            return "--"
        var days = diff / 144
        var weeks = diff / 1008
        if (days > 10)
            return qsTr("%1 weeks behind").arg(Math.ceil(weeks));
        var hours = diff / 6
        if (hours > 48)
            return qsTr("%1 days behind").arg(Math.ceil(days));
        if (diff < 3)
            return qsTr("Updating");
        return qsTr("%1 hours behind").arg(Math.ceil(hours));
    }
    font.italic: true
}
