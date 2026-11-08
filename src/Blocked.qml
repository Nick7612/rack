import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: blocked

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(560, blocked.width - 72)
        spacing: 14

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 74
            height: 74
            radius: width / 2
            color: "#141418"
            border.color: Qt.rgba(1, 1, 1, 0.12)

            Text {
                anchors.centerIn: parent
                text: "8"
                color: "#f6f2e7"
                font.pixelSize: 34
                font.bold: true
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Rack is for Omarchy")
            color: theme.foreground
            font.pixelSize: 30
            font.bold: true
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("This table only opens on an Omarchy system running Hyprland, and hosts only accept players who can say the same.")
            color: theme.muted
            font.pixelSize: 14
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: detail.implicitHeight + 24
            radius: Math.min(8, theme.radius)
            color: Qt.rgba(1, 1, 1, 0.035)
            border.color: Qt.rgba(1, 1, 1, 0.08)

            Text {
                id: detail
                anchors.fill: parent
                anchors.margins: 12
                text: backend.statusText
                color: theme.foreground
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Get Omarchy at <a href=\"https://omarchy.org\">omarchy.org</a>.")
            textFormat: Text.RichText
            linkColor: theme.accent
            color: theme.muted
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            onLinkActivated: function(link) { Qt.openUrlExternally(link) }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10

            RackButton {
                text: qsTr("Open omarchy.org")
                highlighted: true
                onClicked: Qt.openUrlExternally("https://omarchy.org")
            }

            RackButton {
                text: qsTr("Quit")
                onClicked: Qt.quit()
            }
        }
    }
}
