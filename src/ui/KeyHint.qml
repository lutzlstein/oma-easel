import QtQuick

// A key cap followed by what it does: [v] vary.
Row {
    property string key
    property string label
    property bool muted: true
    spacing: 6
    KeyCap { key: parent.key; anchors.verticalCenter: parent.verticalCenter; ink: parent.muted ? Qt.alpha(theme.style.text, 0.7) : theme.style.text }
    OText { text: parent.label; muted: parent.muted; anchors.verticalCenter: parent.verticalCenter }
}
