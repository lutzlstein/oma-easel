import QtQuick
import QtQuick.Controls.Basic

// A text input in the shell's style: same fill and border as buttons, the
// border brightening while focused.
TextField {
    id: control
    readonly property var s: theme.style

    font.family: s.fontFamily
    font.pixelSize: s.fontSize
    color: s.text
    placeholderTextColor: Qt.alpha(s.text, 0.45)
    selectionColor: Qt.alpha(s.accent, s.selectionFill)
    selectedTextColor: s.text
    leftPadding: 10 * s.spacing
    rightPadding: 10 * s.spacing
    topPadding: 7 * s.spacing
    bottomPadding: 7 * s.spacing
    hoverEnabled: true

    background: Rectangle {
        radius: theme.radius
        color: Qt.alpha(control.s.controlColor, control.activeFocus ? control.s.focusFill
                                               : control.hovered ? control.s.hoverFill : control.s.normalFill)
        border.width: control.s.borderWidth
        border.color: control.activeFocus ? Qt.alpha(control.s.accent, 0.9)
                                          : Qt.alpha(control.s.controlColor, control.s.normalBorderAlpha)
    }
}
