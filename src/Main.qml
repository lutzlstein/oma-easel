import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1280
    height: 860
    minimumWidth: 860
    minimumHeight: 620
    visible: true
    title: "Easel"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property int selected: 0
    readonly property bool typing: sceneField.activeFocus
    readonly property var current: studio.candidates[selected]

    function clampSelection() {
        selected = Math.max(0, Math.min(selected, studio.candidates.length - 1))
    }
    function move(step) {
        if (studio.candidates.length > 0) {
            selected = (selected + step + studio.candidates.length) % studio.candidates.length
            grid.positionViewAtIndex(selected, GridView.Contain)
        }
    }
    function requestQuit() {
        if (studio.unkept > 0 || studio.busy)
            quitDialog.open()
        else
            Qt.quit()
    }

    Connections {
        target: studio
        function onCandidatesChanged() { win.clampSelection() }
        function onThemeChanged() { win.selected = 0 }
    }

    onClosing: (close) => {
        if (studio.unkept > 0 || studio.busy) {
            close.accepted = false
            quitDialog.open()
        }
    }

    // Letter keys act on candidates only while the scene field isn't being
    // typed in; Enter in the field generates.
    Shortcut { sequence: "?"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: help.visible ? help.close() : help.open() }
    Shortcut { sequence: "Q"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.requestQuit() }
    Shortcut { sequence: "Space"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: studio.generate() }
    Shortcut { sequences: ["Return", "Enter"]; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: studio.keep(win.selected) }
    Shortcut { sequence: "D"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: studio.discard(win.selected) }
    Shortcut { sequence: "V"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: studio.vary(win.selected) }
    Shortcut {
        sequence: "P"; enabled: !win.typing; context: Qt.ApplicationShortcut
        onActivated: studio.previewing === win.selected ? studio.endPreview() : studio.preview(win.selected)
    }
    Shortcut { sequence: "I"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: studio.pickImports() }
    Shortcut { sequence: "C"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: studio.copyPrompt() }
    Shortcut { sequence: "Left"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.move(-1) }
    Shortcut { sequence: "Right"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.move(1) }
    Shortcut { sequence: "Up"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.move(-3) }
    Shortcut { sequence: "Down"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.move(3) }
    Shortcut { sequences: ["/", "Ctrl+L"]; context: Qt.ApplicationShortcut; onActivated: sceneField.forceActiveFocus() }
    Shortcut {
        sequence: "Escape"; context: Qt.ApplicationShortcut
        onActivated: {
            if (help.visible) help.close()
            else if (studio.previewing >= 0) studio.endPreview()
            else if (studio.busy) studio.cancel()
            else grid.forceActiveFocus()
        }
    }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        DropArea {
            anchors.fill: parent
            keys: ["text/uri-list"]
            onDropped: (drop) => { studio.importUrls(drop.urls); drop.accept() }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // Theme, its palette, and how images will be made.
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ComboBox {
                    id: themePicker
                    Layout.preferredWidth: 260
                    model: studio.themes
                    textRole: "name"
                    valueRole: "slug"
                    Component.onCompleted: currentIndex = indexOfValue(studio.themeSlug)
                    onActivated: studio.loadTheme(currentValue)
                    Connections {
                        target: studio
                        function onThemeChanged() { themePicker.currentIndex = themePicker.indexOfValue(studio.themeSlug) }
                    }
                }

                Row {
                    spacing: 3
                    Repeater {
                        model: studio.palette
                        Rectangle {
                            width: 22; height: 22
                            radius: Math.min(4, theme.radius)
                            color: modelData.color
                            border.color: "#40ffffff"
                            ToolTip.visible: swatchHover.hovered
                            ToolTip.text: modelData.name + " " + modelData.color
                            HoverHandler { id: swatchHover }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: studio.manual ? qsTr("Manual: copy the prompt to any image platform")
                                        : qsTr("Generating with %1").arg(studio.backend === "codex" ? "Codex" : studio.backend)
                    color: studio.manual ? theme.accent : "#a0a0a8"
                    ToolTip.visible: backendHover.hovered
                    ToolTip.text: studio.backendDetail
                    HoverHandler { id: backendHover }
                }
                Label { text: qsTr("Export %1").arg(studio.exportSize); color: "#70707a" }
            }

            // Existing backgrounds; clicked ones become style references.
            Label {
                text: studio.references.length === 0
                      ? qsTr("Style references: click backgrounds to use them (optional)")
                      : qsTr("%n style reference(s)", "", studio.references.length)
                color: "#a0a0a8"
            }
            ListView {
                id: references
                Layout.fillWidth: true
                Layout.preferredHeight: 104
                orientation: ListView.Horizontal
                spacing: 8
                clip: true
                model: studio.backgrounds
                footer: Item {
                    // A varied candidate is a reference that isn't one of the theme's backgrounds.
                    width: varyThumb.visible ? varyThumb.width + 8 : 0
                    height: references.height
                    Rectangle {
                        id: varyThumb
                        readonly property string path: studio.references.length === 1
                            && studio.backgrounds.every(b => b.path !== studio.references[0]) ? studio.references[0] : ""
                        visible: path !== ""
                        x: 8
                        width: height * 16 / 9; height: parent.height
                        radius: theme.radius
                        color: "transparent"
                        border.color: theme.accent; border.width: 3
                        Image { anchors.fill: parent; anchors.margins: 3; source: varyThumb.path ? "file://" + varyThumb.path : ""; fillMode: Image.PreserveAspectCrop; sourceSize.height: 200; asynchronous: true }
                        Label { anchors.bottom: parent.bottom; anchors.margins: 6; x: 8; text: qsTr("Varying"); font.bold: true; color: "white"; style: Text.Outline; styleColor: "black" }
                        TapHandler { onTapped: studio.toggleReference(varyThumb.path) }
                    }
                }
                delegate: Rectangle {
                    required property var modelData
                    width: height * 16 / 9
                    height: references.height
                    radius: theme.radius
                    color: "#18181c"
                    border.color: modelData.selected ? theme.accent : "transparent"
                    border.width: 3
                    Image {
                        anchors.fill: parent
                        anchors.margins: 3
                        source: "file://" + modelData.path
                        sourceSize.height: 200
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        opacity: modelData.selected || studio.references.length === 0 ? 1 : 0.55
                    }
                    ToolTip.visible: thumbHover.hovered
                    ToolTip.text: modelData.name
                    HoverHandler { id: thumbHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: studio.toggleReference(modelData.path) }
                }
            }

            // The scene and the batch size.
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                TextField {
                    id: sceneField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Describe a scene: a quiet harbor at dawn, fishing boats, autumn trees")
                    text: studio.scene
                    onTextEdited: studio.scene = text
                    onAccepted: { studio.generate(); grid.forceActiveFocus() }
                    Keys.onEscapePressed: grid.forceActiveFocus()
                }
                Label { text: qsTr("Candidates"); color: "#a0a0a8"; visible: !studio.manual }
                SpinBox {
                    visible: !studio.manual
                    from: 3; to: 6
                    value: studio.count
                    onValueModified: studio.count = value
                }
                Button {
                    highlighted: true
                    enabled: studio.scene.trim().length > 0 && !studio.busy
                    text: studio.manual ? qsTr("Make prompt") : qsTr("Generate")
                    onClicked: { studio.generate(); grid.forceActiveFocus() }
                }
                Button {
                    visible: studio.busy
                    text: qsTr("Cancel")
                    onClicked: studio.cancel()
                }
            }

            // Manual mode: the prompt to copy and how to bring images back.
            Pane {
                Layout.fillWidth: true
                visible: studio.manual && studio.manualPrompt.length > 0
                Material.background: "#18181c"
                Material.roundedScale: theme.radius
                padding: 12
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                        TextArea { text: studio.manualPrompt; readOnly: true; wrapMode: TextEdit.Wrap; selectByMouse: true }
                    }
                    Label { Layout.fillWidth: true; text: studio.manualSteps.trim(); wrapMode: Text.Wrap; color: "#a0a0a8" }
                    RowLayout {
                        spacing: 8
                        Button { text: qsTr("Copy prompt"); highlighted: true; onClicked: studio.copyPrompt() }
                        Button { text: qsTr("Copy reference paths"); visible: studio.references.length > 0; onClicked: studio.copyReferencePaths() }
                        Button { text: qsTr("Open reference folder"); visible: studio.references.length > 0; onClicked: studio.openReferenceFolder() }
                        Button { text: qsTr("Import images…"); onClicked: studio.pickImports() }
                        Label { text: qsTr("or drop them onto this window"); color: "#70707a" }
                    }
                }
            }

            // The batch.
            GridView {
                id: grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                focus: true
                cellWidth: Math.floor(width / 3)
                cellHeight: Math.floor(cellWidth / studio.aspect) + 8
                model: studio.candidates
                currentIndex: win.selected

                delegate: Item {
                    id: cell
                    required property var modelData
                    required property int index
                    readonly property bool isSelected: index === win.selected
                    width: grid.cellWidth
                    height: grid.cellHeight

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: theme.radius
                        color: "#18181c"
                        border.color: cell.isSelected ? theme.accent : "transparent"
                        border.width: 3
                        clip: true

                        Image {
                            anchors.fill: parent
                            anchors.margins: 3
                            visible: modelData.state === "ready" || modelData.state === "kept"
                            source: visible ? "file://" + modelData.path : ""
                            sourceSize.width: 900
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                        }
                        BusyIndicator { anchors.centerIn: parent; running: modelData.state === "pending" }
                        Label {
                            anchors.fill: parent
                            anchors.margins: 16
                            visible: modelData.state === "failed"
                            text: modelData.error
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                            color: "#e0a0a0"
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Rectangle {
                            visible: modelData.state === "kept" || studio.previewing === cell.index
                            anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 10
                            radius: Math.min(4, theme.radius)
                            color: theme.accent
                            width: badge.implicitWidth + 12; height: badge.implicitHeight + 6
                            Label {
                                id: badge
                                anchors.centerIn: parent
                                text: studio.previewing === cell.index ? qsTr("Previewing") : qsTr("Kept")
                                color: theme.accentForeground
                                font.bold: true
                            }
                        }
                        TapHandler { onTapped: { win.selected = cell.index; grid.forceActiveFocus() } }
                        TapHandler { acceptedButtons: Qt.LeftButton; onDoubleTapped: studio.preview(cell.index) }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: studio.candidates.length === 0
                    text: studio.manual ? qsTr("Describe a scene and press Enter for a prompt, or drop images here.")
                                        : qsTr("Describe a scene and press Enter to generate.")
                    color: "#70707a"
                }
            }

            // Status and the keys that matter most.
            RowLayout {
                Layout.fillWidth: true
                Label { Layout.fillWidth: true; text: studio.message; elide: Text.ElideRight; color: "#c0c0c8" }
                Label {
                    text: qsTr("Space generate · Enter keep · D discard · V vary · P preview · ? keys")
                    color: "#70707a"
                }
            }
        }
    }

    Popup {
        id: help
        anchors.centerIn: parent
        modal: true
        padding: 20
        background: Rectangle { color: "#1c1c20"; radius: theme.radius; border.color: "#30ffffff" }
        ColumnLayout {
            spacing: 6
            Repeater {
                model: [
                    ["Space", qsTr("Generate a batch (or make the prompt in manual mode)")],
                    ["Enter", qsTr("Keep the selected candidate")],
                    ["D", qsTr("Discard the selected candidate")],
                    ["V", qsTr("Vary: new batch using the selected candidate as reference")],
                    ["P", qsTr("Preview on the desktop, or put the background back")],
                    ["← → ↑ ↓", qsTr("Move between candidates")],
                    ["/", qsTr("Type a scene")],
                    ["I", qsTr("Import images")],
                    ["C", qsTr("Copy the prompt")],
                    ["Esc", qsTr("End preview, cancel, or leave the scene field")],
                    ["?", qsTr("Show these keys")],
                    ["Q", qsTr("Quit")]
                ]
                delegate: RowLayout {
                    required property var modelData
                    spacing: 16
                    Label { Layout.preferredWidth: 80; text: modelData[0]; font.bold: true; color: theme.accent }
                    Label { text: modelData[1] }
                }
            }
        }
    }

    Dialog {
        id: quitDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Quit Easel?")
        Label {
            text: studio.busy ? qsTr("Candidates are still generating and will be lost.")
                              : qsTr("%n candidate(s) haven't been kept and will be lost.", "", studio.unkept)
        }
        standardButtons: Dialog.Cancel | Dialog.Discard
        onDiscarded: Qt.quit()
    }
}
