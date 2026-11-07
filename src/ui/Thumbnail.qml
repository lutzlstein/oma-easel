import QtQuick

// A background thumbnail. Chosen ones get the active-border color, like a
// focused window; the rest dim once any are chosen.
Rectangle {
    id: thumb
    property string path
    property bool chosen: false
    property bool dimmed: false
    property string caption
    property bool removable: false
    property bool companionable: false
    signal clicked()
    signal removeRequested()
    signal companionRequested()
    readonly property var s: theme.style

    radius: theme.radius
    color: Qt.alpha(s.controlColor, s.normalFill)
    border.width: chosen ? 2 : s.borderWidth
    border.color: chosen ? s.border : Qt.alpha(s.controlColor, hover.hovered ? s.hoverBorderAlpha * 2 : s.normalBorderAlpha * 0.5)
    clip: true

    Image {
        anchors.fill: parent
        anchors.margins: thumb.border.width
        source: thumb.path ? "file://" + thumb.path : ""
        sourceSize.height: 200
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        opacity: thumb.dimmed ? 0.45 : 1
    }
    Rectangle {
        visible: thumb.caption !== ""
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 6
        width: label.implicitWidth + 10; height: label.implicitHeight + 4
        radius: Math.min(3, theme.radius)
        color: thumb.s.background
        OText { id: label; anchors.centerIn: parent; text: thumb.caption }
    }
    // Backgrounds Oma Easel added get a remove button on hover.
    OButton {
        visible: thumb.removable && hover.hovered
        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 4
        text: "×"
        solid: true
        topPadding: 0; bottomPadding: 0; leftPadding: 6; rightPadding: 6
        implicitHeight: 20
        onClicked: thumb.removeRequested()
    }
    // Recompose this background for the user's other screen shapes.
    OButton {
        visible: thumb.companionable && hover.hovered
        anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 4
        text: qsTr("companion")
        solid: true
        topPadding: 0; bottomPadding: 0; leftPadding: 6; rightPadding: 6
        implicitHeight: 20
        onClicked: thumb.companionRequested()
    }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: thumb.clicked() }
}
