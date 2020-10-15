import QtQuick 2.14
import QtQuick.Controls 2.14
import Flowee.org.pay 1.0

// This is a silly hack to introduce a visual difference
// between enabled and disabled buttons.
Button {
    property var backupPalette: mainWindow.palette
    onEnabledChanged: {
        palette = backupPalette
        if (!enabled)
            palette.buttonText = Flowee.useDarkSkin
                    ? Qt.darker(palette.buttonText)
                    : Qt.lighter(palette.buttonText)
    }
}
