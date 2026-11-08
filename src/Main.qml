import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Rack 1.0

ApplicationWindow {
    id: win
    width: 1240
    height: 800
    minimumWidth: 980
    minimumHeight: 680
    visible: true
    title: qsTr("Rack")
    color: theme.background

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.foreground: theme.foreground
    Material.background: theme.background
    Material.roundedScale: theme.radius >= 16 ? Material.LargeScale
                        : theme.radius >= 12 ? Material.MediumScale
                        : theme.radius >= 8 ? Material.SmallScale
                        : theme.radius >= 4 ? Material.ExtraSmallScale
                        : Material.NotRounded

    property bool shortcutsVisible: false
    property bool typing: gameScreen.typing
    property bool forceClose: false
    property string confirmAction: ""

    readonly property bool inSession: backend.isHost || backend.isClient || backend.isLocal

    Item {
        id: root
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.lighter(theme.background, 1.10) }
                GradientStop { position: 0.55; color: theme.background }
                GradientStop { position: 1; color: Qt.darker(theme.background, 1.12) }
            }
        }

        Home {
            anchors.fill: parent
            visible: backend.screen === "home"
        }

        Join {
            anchors.fill: parent
            visible: backend.screen === "join"
        }

        Lobby {
            anchors.fill: parent
            visible: backend.screen === "lobby"
        }

        GameView {
            id: gameScreen
            anchors.fill: parent
            visible: backend.screen === "game"
        }

        Blocked {
            anchors.fill: parent
            visible: backend.screen === "blocked"
        }

        ShortcutsOverlay {
            anchors.fill: parent
            visible: win.shortcutsVisible
            onDismissed: win.shortcutsVisible = false
        }

        // A single inline error surface, used by the join screen and by any
        // connection problem that happens after leaving it.
        Rectangle {
            id: errorBanner
            anchors.top: parent.top
            anchors.topMargin: 14
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width - 60, errorRow.implicitWidth + 56)
            height: Math.max(44, errorRow.implicitHeight + 20)
            radius: Math.min(8, theme.radius)
            color: Qt.rgba(0.55, 0.08, 0.10, 0.94)
            border.color: Qt.rgba(1, 1, 1, 0.14)
            visible: backend.errorText.length > 0 && backend.screen !== "blocked"
            z: 50

            RowLayout {
                id: errorRow
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 8
                spacing: 12
                Text {
                    text: backend.errorText
                    color: "#ffe9e9"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                RackButton {
                    text: qsTr("Dismiss")
                    flat: true
                    onClicked: backend.clearError()
                }
            }
        }

        Text {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("rack %1 · Omarchy-only table · LAN multiplayer").arg(backend.version)
            color: theme.muted
            font.pixelSize: 11
            opacity: backend.screen === "home" ? 1 : 0
            visible: opacity > 0
        }
    }

    Shortcut {
        sequence: "?"
        context: Qt.ApplicationShortcut
        onActivated: win.shortcutsVisible = !win.shortcutsVisible
    }

    Shortcut {
        sequence: "Q"
        context: Qt.ApplicationShortcut
        enabled: !win.typing
        onActivated: win.requestQuit()
    }

    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        enabled: backend.screen === "game" && backend.myTurn && !win.typing
        onActivated: backend.shootCurrent()
    }

    Shortcut {
        sequence: "Left"
        context: Qt.ApplicationShortcut
        enabled: backend.screen === "game" && backend.myTurn && !win.typing
        onActivated: backend.adjustAim(-0.004)
    }

    Shortcut {
        sequence: "Right"
        context: Qt.ApplicationShortcut
        enabled: backend.screen === "game" && backend.myTurn && !win.typing
        onActivated: backend.adjustAim(0.004)
    }

    Shortcut {
        sequence: "Shift+Left"
        context: Qt.ApplicationShortcut
        enabled: backend.screen === "game" && backend.myTurn && !win.typing
        onActivated: backend.adjustAim(-0.02)
    }

    Shortcut {
        sequence: "Shift+Right"
        context: Qt.ApplicationShortcut
        enabled: backend.screen === "game" && backend.myTurn && !win.typing
        onActivated: backend.adjustAim(0.02)
    }

    Shortcut {
        sequence: "Up"
        context: Qt.ApplicationShortcut
        enabled: backend.screen === "game" && backend.myTurn && !win.typing
        onActivated: backend.adjustPower(0.05)
    }

    Shortcut {
        sequence: "Down"
        context: Qt.ApplicationShortcut
        enabled: backend.screen === "game" && backend.myTurn && !win.typing
        onActivated: backend.adjustPower(-0.05)
    }

    Shortcut {
        sequence: "M"
        context: Qt.ApplicationShortcut
        enabled: !win.typing
        onActivated: backend.sound.muted = !backend.sound.muted
    }

    Shortcut {
        sequence: "R"
        context: Qt.ApplicationShortcut
        enabled: backend.screen === "game" && backend.game.winner >= 0 && !win.typing
        onActivated: backend.rematch()
    }

    Shortcut {
        sequence: "F11"
        context: Qt.ApplicationShortcut
        onActivated: win.visibility === Window.FullScreen ? win.showNormal() : win.showFullScreen()
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: !win.typing
        onActivated: {
            if (win.shortcutsVisible) {
                win.shortcutsVisible = false
            } else if (win.inSession || backend.screen === "join") {
                win.confirmAction = "leave"
                confirmDialog.open()
            }
        }
    }

    function requestQuit() {
        if (win.inSession || backend.screen === "join") {
            win.confirmAction = "quit"
            confirmDialog.open()
        } else {
            win.forceClose = true
            Qt.quit()
        }
    }

    Dialog {
        id: confirmDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 400
        title: win.confirmAction === "quit" ? qsTr("Quit Rack?") : qsTr("Leave the table?")
        standardButtons: Dialog.Cancel | Dialog.Ok

        Text {
            width: parent.width
            text: win.confirmAction === "quit"
                  ? qsTr("A match in progress will end for everyone at the table.")
                  : qsTr("You can host or join another table afterwards.")
            color: theme.foreground
            wrapMode: Text.WordWrap
            font.pixelSize: 14
        }

        onAccepted: {
            backend.leave()
            if (win.confirmAction === "quit") {
                win.forceClose = true
                Qt.quit()
            }
        }
    }

    onClosing: function(close) {
        if (win.forceClose || (!win.inSession && backend.screen !== "join")) {
            close.accepted = true
        } else {
            close.accepted = false
            win.confirmAction = "quit"
            confirmDialog.open()
        }
    }
}
