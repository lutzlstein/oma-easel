import QtQuick
import QtQuick.Layouts

// A small number stepper: − value +, built from shell-style buttons.
RowLayout {
    id: stepper
    property int value: 3
    property int from: 1
    property int to: 10
    signal modified(int value)
    readonly property var s: theme.style
    spacing: 4

    OButton {
        text: "−"
        enabled: stepper.value > stepper.from
        onClicked: stepper.modified(stepper.value - 1)
    }
    Text {
        Layout.preferredWidth: 2 * stepper.s.fontSize
        horizontalAlignment: Text.AlignHCenter
        text: stepper.value
        font.family: stepper.s.fontFamily
        font.pixelSize: stepper.s.fontSize
        color: stepper.s.text
    }
    OButton {
        text: "+"
        enabled: stepper.value < stepper.to
        onClicked: stepper.modified(stepper.value + 1)
    }
}
