import QtQuick

// A key drawn as a small square cap, the way the key is typed: lowercase
// letters are plain keys, a capital would mean Shift plus that letter.
Rectangle {
    id: cap
    property string key
    property color ink: theme.style.text
    readonly property var s: theme.style

    implicitHeight: label.implicitHeight + 4
    implicitWidth: Math.max(implicitHeight, label.implicitWidth + 8)
    radius: Math.min(3, theme.radius)
    color: Qt.alpha(ink, s.normalFill)
    border.width: 1
    border.color: Qt.alpha(ink, Math.max(0.5, s.normalBorderAlpha))

    Text {
        id: label
        anchors.centerIn: parent
        text: cap.key
        color: cap.ink
        font.family: cap.s.fontFamily
        font.pixelSize: Math.round(cap.s.fontSize * 0.92)
    }
}
