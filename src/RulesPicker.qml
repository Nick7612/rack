import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

RowLayout {
    id: picker
    spacing: 8

    RackButton {
        text: qsTr("8-Ball")
        checkable: true
        checked: backend.rules === 0
        highlighted: checked
        Layout.fillWidth: true
        onClicked: backend.rules = 0
    }

    RackButton {
        text: qsTr("9-Ball")
        checkable: true
        checked: backend.rules === 1
        highlighted: checked
        Layout.fillWidth: true
        onClicked: backend.rules = 1
    }
}
