import QtQuick

// Text in the system font and the theme's colors. `muted` dims it for hints.
Text {
    property bool muted: false
    property real scale: 1.0
    font.family: theme.style.fontFamily
    font.pixelSize: Math.round(theme.style.fontSize * scale)
    color: muted ? Qt.alpha(theme.style.text, 0.55) : theme.style.text
}
