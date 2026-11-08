import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: lobby

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            RackButton {
                text: "←"
                flat: true
                font.pixelSize: 20
                implicitWidth: 46
                onClicked: backend.leave()
            }

            ColumnLayout {
                spacing: 0
                Text {
                    text: backend.roomName.length > 0 ? backend.roomName : qsTr("Table")
                    color: theme.foreground
                    font.pixelSize: 30
                    font.bold: true
                }
                Text {
                    text: backend.rulesName + " · " + qsTr("two players · Wi-Fi")
                    color: theme.muted
                    font.pixelSize: 13
                }
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                implicitHeight: 28
                implicitWidth: rulesText.implicitWidth + 22
                radius: 14
                color: Qt.rgba(0.55, 0.85, 0.55, 0.12)
                border.color: Qt.rgba(0.55, 0.85, 0.55, 0.25)
                Text {
                    id: rulesText
                    anchors.centerIn: parent
                    text: qsTr("Omarchy-only")
                    color: theme.accent
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Math.min(10, theme.radius)
                color: Qt.rgba(1, 1, 1, 0.025)
                border.color: Qt.rgba(1, 1, 1, 0.07)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Text {
                        text: qsTr("At the table")
                        color: theme.muted
                        font.pixelSize: 11
                        font.letterSpacing: 1.2
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8
                        clip: true
                        model: backend.lobbyPlayers
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width
                            height: 68
                            radius: Math.min(8, theme.radius)
                            color: Qt.rgba(1, 1, 1, 0.035)
                            border.color: modelData.you ? Qt.rgba(0.55, 0.75, 1.0, 0.35)
                                                        : Qt.rgba(1, 1, 1, 0.07)

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 14
                                spacing: 12

                                Rectangle {
                                    width: 38
                                    height: 38
                                    radius: width / 2
                                    color: modelData.host ? theme.accent : Qt.rgba(1, 1, 1, 0.10)
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.name.length > 0 ? modelData.name.charAt(0).toUpperCase() : "?"
                                        color: modelData.host ? theme.accentForeground : theme.foreground
                                        font.bold: true
                                        font.pixelSize: 16
                                    }
                                }

                                ColumnLayout {
                                    spacing: 2
                                    Layout.fillWidth: true
                                    RowLayout {
                                        spacing: 8
                                        Text {
                                            text: modelData.name
                                            color: theme.foreground
                                            font.pixelSize: 16
                                            font.bold: true
                                        }
                                        Text {
                                            visible: modelData.host
                                            text: qsTr("host")
                                            color: theme.accent
                                            font.pixelSize: 11
                                        }
                                        Text {
                                            visible: modelData.you
                                            text: qsTr("you")
                                            color: theme.muted
                                            font.pixelSize: 11
                                        }
                                    }
                                    Text {
                                        text: qsTr("Omarchy %1").arg(modelData.omarchy)
                                        color: theme.muted
                                        font.pixelSize: 11
                                    }
                                }

                                Text {
                                    text: modelData.ready ? qsTr("Ready") : qsTr("Not ready")
                                    color: modelData.ready ? theme.accent : theme.muted
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }

                    Text {
                        visible: backend.lobbyPlayers.length < 2
                        Layout.fillWidth: true
                        text: backend.isHost
                              ? qsTr("Share your address with a friend on this Wi-Fi. They can also find the table from Join.")
                              : qsTr("Waiting for the host to start the match.")
                        color: theme.muted
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 330
                Layout.fillHeight: true
                radius: Math.min(10, theme.radius)
                color: Qt.rgba(1, 1, 1, 0.035)
                border.color: Qt.rgba(1, 1, 1, 0.07)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 14

                    Text {
                        text: qsTr("Connection")
                        color: theme.muted
                        font.pixelSize: 11
                        font.letterSpacing: 1.2
                    }

                    Text {
                        Layout.fillWidth: true
                        text: backend.isHost ? backend.address : qsTr("Connected over your local network")
                        color: theme.foreground
                        font.pixelSize: 14
                        font.family: "monospace"
                        wrapMode: Text.WrapAnywhere
                    }

                    RackButton {
                        Layout.fillWidth: true
                        text: qsTr("Copy address")
                        visible: backend.isHost && backend.address.length > 0
                        onClicked: backend.copyAddress()
                    }

                    Item { Layout.fillHeight: true }

                    Text {
                        Layout.fillWidth: true
                        text: backend.statusText
                        color: theme.muted
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }

                    RackButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        visible: backend.isHost
                        enabled: backend.canStart
                        highlighted: true
                        text: qsTr("Start match")
                        onClicked: backend.startMatch()
                    }

                    RackButton {
                        Layout.fillWidth: true
                        visible: backend.isClient
                        enabled: false
                        text: qsTr("Waiting for host")
                    }

                    RackButton {
                        Layout.fillWidth: true
                        text: qsTr("Leave table")
                        onClicked: backend.leave()
                    }
                }
            }
        }
    }
}
