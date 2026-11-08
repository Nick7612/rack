import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: joinScreen

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            RackButton {
                text: "←"
                flat: true
                font.pixelSize: 20
                implicitWidth: 46
                onClicked: backend.closeJoin()
            }

            ColumnLayout {
                spacing: 0
                Text {
                    text: qsTr("Join a table")
                    color: theme.foreground
                    font.pixelSize: 30
                    font.bold: true
                }
                Text {
                    text: qsTr("Tables on the same Wi-Fi appear here.")
                    color: theme.muted
                    font.pixelSize: 13
                }
            }

            Item { Layout.fillWidth: true }

            BusyIndicator {
                running: backend.connecting
                visible: running
                implicitWidth: 32
                implicitHeight: 32
                Material.accent: theme.accent
            }

            RackButton {
                text: qsTr("Refresh")
                onClicked: backend.refreshGames()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Math.min(10, theme.radius)
            color: Qt.rgba(1, 1, 1, 0.025)
            border.color: Qt.rgba(1, 1, 1, 0.07)

            ListView {
                id: games
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                spacing: 8
                model: backend.discoveredGames

                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 74
                    radius: Math.min(8, theme.radius)
                    color: Qt.rgba(1, 1, 1, 0.035)
                    border.color: Qt.rgba(1, 1, 1, 0.07)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 14

                        Rectangle {
                            width: 38
                            height: 38
                            radius: width / 2
                            color: "#141418"
                            border.color: Qt.rgba(1, 1, 1, 0.12)
                            Text {
                                anchors.centerIn: parent
                                text: modelData.rules === 0 ? "8" : "9"
                                color: "#f6f2e7"
                                font.bold: true
                                font.pixelSize: 17
                            }
                        }

                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true
                            Text {
                                text: modelData.name
                                color: theme.foreground
                                font.pixelSize: 16
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Text {
                                text: (modelData.rules === 0 ? qsTr("8-Ball") : qsTr("9-Ball"))
                                      + " · " + (modelData.status === "playing" ? qsTr("In a match") : qsTr("Waiting"))
                                      + " · " + modelData.address
                                color: theme.muted
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        RackButton {
                            text: qsTr("Connect")
                            highlighted: true
                            enabled: modelData.status !== "playing"
                            onClicked: backend.joinGame(modelData.address)
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(460, joinScreen.width - 80)
                visible: games.count === 0
                spacing: 8
                Text {
                    Layout.fillWidth: true
                    text: backend.connecting ? qsTr("Connecting...") : qsTr("No tables found yet")
                    color: theme.foreground
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Ask the host to open their table, or enter their address below. Some Wi-Fi networks block discovery but still allow a direct connection.")
                    color: theme.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            radius: Math.min(8, theme.radius)
            color: Qt.rgba(1, 1, 1, 0.035)
            border.color: Qt.rgba(1, 1, 1, 0.07)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Text {
                    text: qsTr("Direct address")
                    color: theme.muted
                    font.pixelSize: 12
                }

                TextField {
                    id: addressField
                    Layout.fillWidth: true
                    text: backend.joinAddress
                    placeholderText: "192.168.1.20:46321"
                    selectByMouse: true
                    onEditingFinished: backend.joinAddress = text
                    onAccepted: {
                        backend.joinAddress = text
                        backend.joinGame(text)
                    }
                }

                RackButton {
                    text: qsTr("Connect")
                    highlighted: true
                    enabled: !backend.connecting
                    onClicked: {
                        backend.joinAddress = addressField.text
                        backend.joinGame(addressField.text)
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: backend.statusText
            color: theme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
