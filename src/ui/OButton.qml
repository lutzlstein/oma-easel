import QtQuick
import QtQuick.Controls.Basic

// A button drawn like the Omarchy shell's: a faint fill and a thin border in
// the theme's text color, brighter on hover and focus, filled when pressed.
// `primary` marks the one main action with the accent.
Button {
    id: control
    property bool primary: false
    readonly property var s: theme.style
    readonly property color ink: primary ? s.accent : s.controlColor

    font.family: s.fontFamily
    font.pixelSize: s.fontSize
    leftPadding: 10 * s.spacing
    rightPadding: 10 * s.spacing
    topPadding: 6 * s.spacing
    bottomPadding: 6 * s.spacing
    implicitHeight: Math.max(28 * s.spacing, implicitContentHeight + topPadding + bottomPadding)
    focusPolicy: Qt.TabFocus
    hoverEnabled: true

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? control.ink : Qt.alpha(control.ink, 0.4)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: theme.radius
        color: Qt.alpha(control.ink, control.down ? control.s.pressedFill
                                   : control.hovered || control.activeFocus ? control.s.hoverFill
                                   : control.s.normalFill)
        border.width: control.s.borderWidth
        border.color: Qt.alpha(control.ink, !control.enabled ? 0.15
                                          : control.primary ? 0.9
                                          : control.hovered || control.activeFocus ? Math.max(control.s.hoverBorderAlpha, control.s.normalBorderAlpha)
                                          : control.s.normalBorderAlpha)
    }
}
