import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: home

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(660, home.width - 72)
        spacing: 16

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 16

            Rectangle {
                width: 58
                height: 58
                radius: width / 2
                color: "#141418"
                border.color: Qt.rgba(1, 1, 1, 0.12)
                border.width: 1

                Rectangle {
                    anchors.centerIn: parent
                    width: 28
                    height: 28
                    radius: width / 2
                    color: "#f6f2e7"
                    Text {
                        anchors.centerIn: parent
                        text: "8"
                        color: "#141418"
                        font.pixelSize: 16
                        font.bold: true
                    }
                }
            }

            ColumnLayout {
                spacing: 2
                Text {
                    text: qsTr("RACK")
                    color: theme.foreground
                    font.pixelSize: 58
                    font.bold: true
                    font.letterSpacing: 10
                }
                Text {
                    text: qsTr("Billiards for Omarchy")
                    color: theme.accent
                    font.pixelSize: 16
                    font.letterSpacing: 1.4
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            Rectangle {
                implicitHeight: 24
                implicitWidth: badgeText.implicitWidth + 20
                radius: 12
                color: Qt.rgba(1, 1, 1, 0.06)
                border.color: Qt.rgba(1, 1, 1, 0.10)
                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: qsTr("Omarchy %1").arg(backend.omarchyVersion)
                    color: theme.muted
                    font.pixelSize: 11
                }
            }

            Rectangle {
                implicitHeight: 24
                implicitWidth: lanText.implicitWidth + 20
                radius: 12
                color: Qt.rgba(1, 1, 1, 0.06)
                border.color: Qt.rgba(1, 1, 1, 0.10)
                Text {
                    id: lanText
                    anchors.centerIn: parent
                    text: qsTr("Wi-Fi multiplayer")
                    color: theme.muted
                    font.pixelSize: 11
                }
            }
        }

        Item { Layout.preferredHeight: 8 }

        TextField {
            id: nameField
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 320
            text: backend.playerName
            placeholderText: qsTr("Your name")
            selectByMouse: true
            horizontalAlignment: TextInput.AlignHCenter
            onEditingFinished: {
                backend.playerName = text
                text = backend.playerName
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Rules")
            color: theme.muted
            font.pixelSize: 11
            font.letterSpacing: 1.2
        }

        RulesPicker {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 320
        }

        Item { Layout.preferredHeight: 10 }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            RackButton {
                text: qsTr("Host a table")
                highlighted: true
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                onClicked: {
                    backend.playerName = nameField.text
                    backend.hostGame(backend.rules)
                }
            }

            RackButton {
                text: qsTr("Join a table")
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                onClicked: {
                    backend.playerName = nameField.text
                    backend.openJoin()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            RackButton {
                text: qsTr("Local table")
                Layout.fillWidth: true
                onClicked: {
                    backend.playerName = nameField.text
                    backend.startLocal(backend.rules)
                }
            }

            RackButton {
                text: qsTr("Practice")
                Layout.fillWidth: true
                onClicked: {
                    backend.playerName = nameField.text
                    backend.startPractice(backend.rules)
                }
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.topMargin: 12
            text: qsTr("Host on your Wi-Fi, then let the other player pick your table from their Join screen. Direct addresses work when a network hides broadcasts.")
            color: theme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
