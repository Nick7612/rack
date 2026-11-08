import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Rack 1.0

Item {
    id: gameScreen

    property var game: backend.game
    property bool chatOpen: false
    readonly property bool typing: chatOpen && chatPanel.input.activeFocus

    Shortcut {
        sequence: "C"
        context: Qt.ApplicationShortcut
        enabled: backend.screen === "game" && (backend.isHost || backend.isClient)
                 && !chatPanel.input.activeFocus
        onActivated: {
            gameScreen.chatOpen = !gameScreen.chatOpen
            if (gameScreen.chatOpen)
                chatPanel.input.forceActiveFocus()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 96

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 12
                anchors.bottomMargin: 8
                spacing: 14

                PlayerPanel {
                    Layout.preferredWidth: Math.min(300, Math.max(230, gameScreen.width * 0.26))
                    Layout.maximumWidth: 300
                    playerIndex: 0
                    playerName: game.player0
                    active: game.currentPlayer === 0 && game.winner < 0
                    isWinner: game.winner === 0
                    group: game.group0
                    remaining: game.remaining0
                    rules: game.rules
                    target: game.targetBall
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 2

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: game.rulesName + " · " + qsTr("turn %1").arg(game.currentPlayer + 1)
                        color: theme.muted
                        font.pixelSize: 11
                        font.letterSpacing: 1.2
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillWidth: true
                        text: game.message
                        color: theme.foreground
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        visible: game.phase === 2 && backend.myTurn
                        text: qsTr("Ball in hand — move the cue ball, then click to place it.")
                        color: theme.accent
                        font.pixelSize: 12
                    }
                }

                PlayerPanel {
                    Layout.preferredWidth: Math.min(300, Math.max(230, gameScreen.width * 0.26))
                    Layout.maximumWidth: 300
                    playerIndex: 1
                    playerName: game.player1
                    active: game.currentPlayer === 1 && game.winner < 0
                    isWinner: game.winner === 1
                    group: game.group1
                    remaining: game.remaining1
                    rules: game.rules
                    target: game.targetBall
                }

                RackButton {
                    text: qsTr("Leave")
                    flat: true
                    Layout.preferredWidth: 72
                    onClicked: backend.leave()
                }
            }
        }

        Item {
            id: tableArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10

            TableView {
                id: table
                anchors.fill: parent
                game: backend.game
                accent: theme.accent
                radius: theme.radius
                remoteAim: backend.remoteAim
                remoteAimVisible: backend.remoteAimVisible
            }

            MouseArea {
                id: aimArea
                anchors.fill: parent
                hoverEnabled: true
                enabled: game.winner < 0 && backend.myTurn && game.phase !== 1
                cursorShape: enabled ? Qt.CrossCursor : Qt.ArrowCursor

                property bool charging: false
                property real pressX: 0
                property real pressY: 0

                function point(mx, my) {
                    return table.mapToTable(mx, my)
                }

                onPressed: function(mouse) {
                    if (game.phase === 2) {
                        const p = point(mouse.x, mouse.y)
                        backend.previewCueBall(p.x, p.y)
                        return
                    }
                    if (game.phase === 0) {
                        const p = point(mouse.x, mouse.y)
                        pressX = mouse.x
                        pressY = mouse.y
                        backend.setAim(Math.atan2(p.y - game.cueY, p.x - game.cueX))
                        backend.setPower(0.10)
                        charging = true
                    }
                }

                onPositionChanged: function(mouse) {
                    const p = point(mouse.x, mouse.y)
                    if (game.phase === 2) {
                        backend.previewCueBall(p.x, p.y)
                        return
                    }
                    if (charging) {
                        const dx = mouse.x - pressX
                        const dy = mouse.y - pressY
                        const distance = Math.sqrt(dx * dx + dy * dy)
                        backend.setPower(Math.max(0.08, Math.min(1.0, distance / 240)))
                    } else if (game.phase === 0) {
                        backend.setAim(Math.atan2(p.y - game.cueY, p.x - game.cueX))
                    }
                }

                onReleased: function(mouse) {
                    if (game.phase === 2) {
                        const p = point(mouse.x, mouse.y)
                        backend.placeCueBall(p.x, p.y)
                        return
                    }
                    if (charging) {
                        charging = false
                        backend.shoot(game.aimAngle, game.aimPower)
                        backend.setPower(0.55)
                    }
                }
            }

            ChatPanel {
                id: chatPanel
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 10
                width: 330
                height: 300
                visible: chatOpen
                onSendRequested: function(text) { backend.sendChat(text) }
                onCloseRequested: gameScreen.chatOpen = false
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 78

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 14

                Text {
                    text: qsTr("Power")
                    color: theme.muted
                    font.pixelSize: 11
                    font.letterSpacing: 1.1
                }

                Slider {
                    id: power
                    Layout.preferredWidth: 250
                    from: 0.02
                    to: 1.0
                    value: game.aimPower
                    stepSize: 0.01
                    enabled: backend.myTurn && game.phase === 0
                    onMoved: backend.setPower(value)
                }

                Text {
                    text: Math.round(game.aimPower * 100) + "%"
                    color: theme.foreground
                    font.pixelSize: 12
                    Layout.minimumWidth: 40
                }

                Text {
                    Layout.fillWidth: true
                    text: backend.myTurn
                          ? qsTr("Move to aim · drag to set power · release to shoot · Space shoots · ? for shortcuts")
                          : qsTr("Waiting for the other player.")
                    color: theme.muted
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                RackButton {
                    text: qsTr("Chat")
                    visible: backend.isHost || backend.isClient
                    onClicked: {
                        gameScreen.chatOpen = !gameScreen.chatOpen
                        if (gameScreen.chatOpen)
                            chatPanel.input.forceActiveFocus()
                    }
                }
            }
        }
    }

    Rectangle {
        id: endOverlay
        anchors.centerIn: parent
        width: 450
        height: endColumn.implicitHeight + 44
        radius: Math.min(12, theme.radius)
        color: Qt.rgba(0.055, 0.055, 0.07, 0.98)
        border.width: 2
        border.color: theme.accent
        visible: game.winner >= 0
        z: 100

        ColumnLayout {
            id: endColumn
            anchors.fill: parent
            anchors.margins: 22
            spacing: 10

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: game.winner === 0 ? game.player0 : game.player1
                color: theme.accent
                font.pixelSize: 28
                font.bold: true
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("wins")
                color: theme.muted
                font.pixelSize: 12
                font.letterSpacing: 1.4
            }

            Text {
                Layout.fillWidth: true
                Layout.topMargin: 4
                text: game.message
                color: theme.foreground
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                spacing: 10

                RackButton {
                    text: qsTr("Rematch")
                    highlighted: true
                    visible: backend.isHost || backend.isLocal
                    onClicked: backend.rematch()
                }

                RackButton {
                    text: qsTr("Request rematch")
                    visible: backend.isClient
                    onClicked: backend.rematch()
                }

                RackButton {
                    text: qsTr("Leave")
                    onClicked: backend.leave()
                }
            }

            Text {
                Layout.fillWidth: true
                visible: backend.isClient
                text: backend.statusText
                color: theme.muted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
