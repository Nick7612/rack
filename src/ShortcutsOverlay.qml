import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: overlay

    signal dismissed()

    property var items: [
        { keys: qsTr("Mouse"), action: qsTr("Aim the cue") },
        { keys: qsTr("Drag"), action: qsTr("Set power and shoot on release") },
        { keys: qsTr("Space"), action: qsTr("Shoot with the current aim and power") },
        { keys: qsTr("← →"), action: qsTr("Fine aim (hold Shift for larger steps)") },
        { keys: qsTr("↑ ↓"), action: qsTr("Adjust power") },
        { keys: qsTr("Click"), action: qsTr("Place the cue ball after a foul") },
        { keys: qsTr("C"), action: qsTr("Open table chat") },
        { keys: qsTr("M"), action: qsTr("Mute or unmute sounds") },
        { keys: qsTr("F11"), action: qsTr("Toggle fullscreen") },
        { keys: qsTr("Esc"), action: qsTr("Close this overlay or leave the table") },
        { keys: qsTr("Q"), action: qsTr("Quit Rack") }
    ]

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.62)

        MouseArea {
            anchors.fill: parent
            onClicked: overlay.dismissed()
        }
    }

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(620, overlay.width - 60)
        height: column.implicitHeight + 48
        radius: Math.min(12, theme.radius)
        color: Qt.rgba(0.055, 0.055, 0.07, 0.99)
        border.color: Qt.rgba(1, 1, 1, 0.12)

        MouseArea {
            anchors.fill: parent
        }

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 24
            spacing: 10

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: qsTr("Keyboard shortcuts")
                    color: theme.foreground
                    font.pixelSize: 22
                    font.bold: true
                    Layout.fillWidth: true
                }

                RackButton {
                    text: qsTr("Close")
                    flat: true
                    onClicked: overlay.dismissed()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(1, 1, 1, 0.08)
            }

            Repeater {
                model: overlay.items

                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 16

                    Rectangle {
                        Layout.preferredWidth: 108
                        implicitHeight: 26
                        radius: Math.min(5, theme.radius)
                        color: Qt.rgba(1, 1, 1, 0.055)
                        Text {
                            anchors.centerIn: parent
                            text: modelData.keys
                            color: theme.accent
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.action
                        color: theme.foreground
                        font.pixelSize: 13
                    }
                }
            }
        }
    }
}
