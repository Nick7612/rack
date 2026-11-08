import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: chat

    property alias input: field
    signal sendRequested(string text)
    signal closeRequested()

    color: Qt.rgba(0.06, 0.06, 0.075, 0.96)
    border.color: Qt.rgba(1, 1, 1, 0.10)
    border.width: 1
    radius: Math.min(9, theme.radius)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Text {
            text: qsTr("Table chat")
            color: theme.muted
            font.pixelSize: 11
            font.letterSpacing: 1.1
        }

        ListView {
            id: messages
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 5
            model: backend.chatLog
            onCountChanged: positionViewAtEnd()

            delegate: Text {
                required property var modelData
                width: ListView.view.width
                text: modelData.system
                      ? modelData.text
                      : (modelData.name + ": " + modelData.text)
                color: modelData.system ? theme.muted
                     : modelData.mine ? theme.accent
                     : theme.foreground
                font.pixelSize: 12
                font.italic: modelData.system
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            TextField {
                id: field
                Layout.fillWidth: true
                placeholderText: qsTr("Say something")
                selectByMouse: true
                Keys.onEscapePressed: {
                    chat.closeRequested()
                    event.accepted = true
                }
                onAccepted: {
                    if (text.trim().length > 0)
                        chat.sendRequested(text)
                    text = ""
                }
            }

            RackButton {
                text: qsTr("Send")
                highlighted: true
                enabled: field.text.trim().length > 0
                onClicked: {
                    chat.sendRequested(field.text)
                    field.text = ""
                }
            }
        }
    }
}
