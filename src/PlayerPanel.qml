import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: panel

    property int playerIndex: 0
    property string playerName: ""
    property bool active: false
    property bool isWinner: false
    property int group: 0
    property int remaining: 0
    property int rules: 0
    property int target: 0

    implicitHeight: 72
    radius: Math.min(9, theme.radius)
    color: active ? Qt.rgba(1, 1, 1, 0.075) : Qt.rgba(1, 1, 1, 0.035)
    border.width: active || isWinner ? 2 : 1
    border.color: isWinner ? theme.accent
                : active ? theme.accent
                : Qt.rgba(1, 1, 1, 0.07)

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 16
        spacing: 12

        Rectangle {
            width: 34
            height: 34
            radius: width / 2
            color: active ? theme.accent : Qt.rgba(1, 1, 1, 0.09)
            Text {
                anchors.centerIn: parent
                text: panel.playerName.length > 0 ? panel.playerName.charAt(0).toUpperCase() : "?"
                color: active ? theme.accentForeground : theme.foreground
                font.bold: true
                font.pixelSize: 15
            }
        }

        ColumnLayout {
            spacing: 1
            Layout.fillWidth: true

            RowLayout {
                spacing: 7

                Text {
                    text: panel.playerName.length > 0 ? panel.playerName : qsTr("Waiting")
                    color: theme.foreground
                    font.pixelSize: 15
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    visible: panel.active && !panel.isWinner
                    text: qsTr("to shoot")
                    color: theme.accent
                    font.pixelSize: 11
                }
                Text {
                    visible: panel.isWinner
                    text: qsTr("winner")
                    color: theme.accent
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            Text {
                text: {
                    if (panel.rules === 1)
                        return panel.target > 0 ? qsTr("Target %1 · %2 balls left").arg(panel.target).arg(panel.remaining)
                                                : qsTr("%1 balls left").arg(panel.remaining)
                    if (panel.group === 0)
                        return qsTr("Open table")
                    return (panel.group === 1 ? qsTr("Solids") : qsTr("Stripes"))
                           + " · " + qsTr("%1 left").arg(panel.remaining)
                }
                color: theme.muted
                font.pixelSize: 11
            }
        }
    }
}
