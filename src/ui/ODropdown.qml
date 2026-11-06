import QtQuick
import QtQuick.Controls.Basic

// A dropdown matching the shell's: bordered field, popup card with the
// popup background and the active-border color, a faint fill on the
// highlighted row.
ComboBox {
    id: control
    readonly property var s: theme.style

    font.family: s.fontFamily
    font.pixelSize: s.fontSize
    implicitHeight: 28 * s.spacing
    hoverEnabled: true

    contentItem: Text {
        leftPadding: 10 * control.s.spacing
        rightPadding: control.indicator.width
        text: control.displayText
        font: control.font
        color: control.s.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Text {
        x: control.width - width - 10 * control.s.spacing
        anchors.verticalCenter: parent.verticalCenter
        text: control.popup.visible ? "▴" : "▾"
        font: control.font
        color: Qt.alpha(control.s.text, 0.7)
    }
    background: Rectangle {
        radius: theme.radius
        color: Qt.alpha(control.s.controlColor, control.hovered || control.activeFocus ? control.s.hoverFill : control.s.normalFill)
        border.width: control.s.borderWidth
        border.color: Qt.alpha(control.s.controlColor, control.s.normalBorderAlpha)
    }
    delegate: ItemDelegate {
        id: row
        required property var modelData
        required property int index
        width: ListView.view.width
        height: 28 * control.s.spacing
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: row.modelData[control.textRole]
            font: control.font
            color: control.s.text
            verticalAlignment: Text.AlignVCenter
            leftPadding: 4
        }
        background: Rectangle {
            radius: theme.radius
            color: row.highlighted ? Qt.alpha(control.s.controlColor, control.s.hoverFill)
                 : control.currentIndex === row.index ? Qt.alpha(control.s.controlColor, control.s.selectedFill * 0.6)
                 : "transparent"
        }
    }
    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 12, 420)
        padding: 6
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            radius: theme.radius
            color: control.s.popupBackground
            border.width: 1
            border.color: control.s.border
        }
    }
}
