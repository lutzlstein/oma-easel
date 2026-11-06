import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "ui"

ApplicationWindow {
    id: win
    width: 1280
    height: 860
    minimumWidth: 860
    minimumHeight: 620
    visible: true
    title: "Easel"

    // Everything takes the shell's look from the current theme's shell.toml,
    // so easel sits on the desktop like the bar, menus, and panels do.
    readonly property var s: theme.style
    readonly property real gap: 8 * s.spacing
    color: s.background
    font.family: s.fontFamily
    font.pixelSize: s.fontSize

    property int selected: 0
    readonly property bool typing: sceneField.activeFocus

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
    Shortcut { sequence: "Up"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.move(-grid.columns) }
    Shortcut { sequence: "Down"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.move(grid.columns) }
    Shortcut { sequences: ["/", "Ctrl+L"]; context: Qt.ApplicationShortcut; onActivated: sceneField.forceActiveFocus() }
    Shortcut {
        sequence: "Escape"; context: Qt.ApplicationShortcut
        onActivated: {
            if (help.visible) help.close()
            else if (quitDialog.visible) quitDialog.close()
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
            anchors.margins: 18 * s.spacing
            spacing: 14 * s.spacing

            // Theme, its palette, and how images will be made.
            RowLayout {
                Layout.fillWidth: true
                spacing: win.gap * 1.5

                ODropdown {
                    id: themePicker
                    Layout.preferredWidth: 260 * s.spacing
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
                    spacing: 2
                    Repeater {
                        model: studio.palette
                        Rectangle {
                            required property var modelData
                            width: 16 * s.spacing; height: 16 * s.spacing
                            radius: Math.min(3, theme.radius)
                            color: modelData.color
                            border.width: 1
                            border.color: Qt.alpha(s.text, 0.2)
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                OText {
                    text: studio.manual ? qsTr("manual · copy the prompt to any image platform")
                                        : qsTr("generating with %1").arg(studio.backend)
                    color: studio.manual ? s.accent : Qt.alpha(s.text, 0.55)
                }
                OText { text: studio.exportSize; muted: true }
            }

            // Existing backgrounds; clicked ones become style references.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: win.gap
                OText {
                    text: studio.references.length === 0
                          ? qsTr("Style references · click backgrounds to use them (optional)")
                          : qsTr("%n style reference(s)", "", studio.references.length)
                    muted: studio.references.length === 0
                }
                ListView {
                    id: references
                    Layout.fillWidth: true
                    Layout.preferredHeight: 96 * s.spacing
                    orientation: ListView.Horizontal
                    spacing: win.gap
                    clip: true
                    model: studio.backgrounds
                    footer: Item {
                        // A varied candidate is a reference that isn't one of the theme's backgrounds.
                        readonly property string path: studio.references.length === 1
                            && studio.backgrounds.every(b => b.path !== studio.references[0]) ? studio.references[0] : ""
                        width: path ? references.height * 16 / 9 + win.gap : 0
                        height: references.height
                        Thumbnail {
                            visible: parent.path !== ""
                            x: win.gap
                            width: references.height * 16 / 9; height: references.height
                            path: parent.path
                            chosen: true
                            caption: qsTr("varying")
                            onClicked: studio.toggleReference(path)
                        }
                    }
                    delegate: Thumbnail {
                        required property var modelData
                        width: height * 16 / 9
                        height: references.height
                        path: modelData.path
                        chosen: modelData.selected
                        dimmed: studio.references.length > 0 && !modelData.selected
                        onClicked: studio.toggleReference(modelData.path)
                    }
                }
            }

            // The scene and the batch size.
            RowLayout {
                Layout.fillWidth: true
                spacing: win.gap
                OField {
                    id: sceneField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Describe a scene: a quiet harbor at dawn, fishing boats, autumn trees")
                    text: studio.scene
                    onTextEdited: studio.scene = text
                    onAccepted: { studio.generate(); grid.forceActiveFocus() }
                    Keys.onEscapePressed: grid.forceActiveFocus()
                }
                OText { text: qsTr("candidates"); muted: true; visible: !studio.manual }
                OStepper {
                    visible: !studio.manual
                    from: 1; to: 6
                    value: studio.count
                    onModified: (value) => studio.count = value
                }
                OButton {
                    primary: true
                    enabled: studio.scene.trim().length > 0 && !studio.busy
                    text: studio.manual ? qsTr("Make prompt") : qsTr("Generate")
                    onClicked: { studio.generate(); grid.forceActiveFocus() }
                }
                OButton {
                    visible: studio.busy
                    text: qsTr("Cancel")
                    onClicked: studio.cancel()
                }
            }

            // Manual mode: the prompt to copy and how to bring images back.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: manualColumn.implicitHeight + 2 * manualColumn.anchors.margins
                visible: studio.manual && studio.manualPrompt.length > 0
                radius: theme.radius
                color: Qt.alpha(s.controlColor, s.normalFill)
                border.width: s.borderWidth
                border.color: Qt.alpha(s.controlColor, s.normalBorderAlpha)
                ColumnLayout {
                    id: manualColumn
                    anchors.fill: parent
                    anchors.margins: 14 * s.spacing
                    spacing: win.gap
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120 * s.spacing
                        TextArea {
                            text: studio.manualPrompt
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            color: s.text
                            selectionColor: Qt.alpha(s.accent, s.selectionFill)
                            font.family: s.fontFamily
                            font.pixelSize: s.fontSize
                            background: null
                        }
                    }
                    OText { Layout.fillWidth: true; text: studio.manualSteps.trim(); wrapMode: Text.Wrap; muted: true }
                    RowLayout {
                        spacing: win.gap
                        OButton { text: qsTr("Copy prompt"); primary: true; onClicked: studio.copyPrompt() }
                        OButton { text: qsTr("Copy reference paths"); visible: studio.references.length > 0; onClicked: studio.copyReferencePaths() }
                        OButton { text: qsTr("Open reference folder"); visible: studio.references.length > 0; onClicked: studio.openReferenceFolder() }
                        OButton { text: qsTr("Import images…"); onClicked: studio.pickImports() }
                        OText { text: qsTr("or drop them onto this window"); muted: true }
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
                // Up to three across; fewer candidates get bigger cells,
                // sized so every row fits without scrolling when it can.
                readonly property int columns: Math.max(1, Math.min(3, count))
                readonly property int rows: Math.max(1, Math.ceil(count / columns))
                cellWidth: Math.floor(Math.min(width / columns, height / rows * studio.aspect))
                cellHeight: Math.floor(cellWidth / studio.aspect)
                model: studio.candidates
                currentIndex: win.selected

                delegate: Item {
                    id: cell
                    required property var modelData
                    required property int index
                    readonly property bool isSelected: index === win.selected
                    readonly property bool hasImage: modelData.state === "ready" || modelData.state === "kept"
                    width: grid.cellWidth
                    height: grid.cellHeight

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: win.gap / 2
                        radius: theme.radius
                        color: Qt.alpha(s.controlColor, s.normalFill)
                        border.width: cell.isSelected ? 2 : s.borderWidth
                        border.color: cell.isSelected ? s.border : Qt.alpha(s.controlColor, s.normalBorderAlpha)
                        clip: true

                        Image {
                            anchors.fill: parent
                            anchors.margins: parent.border.width
                            visible: cell.hasImage
                            source: visible ? "file://" + modelData.path : ""
                            sourceSize.width: 900
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                        }
                        OText {
                            anchors.centerIn: parent
                            visible: modelData.state === "pending"
                            text: qsTr("generating") + ".".repeat(dots.count)
                            muted: true
                            Timer { id: dots; property int count: 1; interval: 450; repeat: true; running: parent.visible; onTriggered: count = count % 3 + 1 }
                        }
                        OText {
                            anchors.fill: parent
                            anchors.margins: 16
                            visible: modelData.state === "failed"
                            text: modelData.error
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                            color: theme.style.accent
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Rectangle {
                            visible: modelData.state === "kept" || studio.previewing === cell.index
                            anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 10
                            radius: Math.min(3, theme.radius)
                            color: s.background
                            border.width: 1
                            border.color: s.border
                            width: badge.implicitWidth + 12; height: badge.implicitHeight + 6
                            OText {
                                id: badge
                                anchors.centerIn: parent
                                text: studio.previewing === cell.index ? qsTr("previewing") : qsTr("kept")
                            }
                        }
                        TapHandler { onTapped: { win.selected = cell.index; grid.forceActiveFocus() } }
                        TapHandler { onDoubleTapped: studio.preview(cell.index) }
                    }
                }

                OText {
                    anchors.centerIn: parent
                    visible: studio.candidates.length === 0
                    text: studio.manual ? qsTr("Describe a scene and press Enter for a prompt, or drop images here.")
                                        : qsTr("Describe a scene and press Enter to generate.")
                    muted: true
                }
            }

            // Status and the keys that matter most.
            RowLayout {
                Layout.fillWidth: true
                OText { Layout.fillWidth: true; text: studio.message; elide: Text.ElideRight }
                OText {
                    text: qsTr("space generate · enter keep · d discard · v vary · p preview · ? keys")
                    muted: true
                }
            }
        }
    }

    // Overlay cards use the shell's popup surface: popup background with the
    // active-border color, over a dimmed window.
    component Card: Popup {
        anchors.centerIn: parent
        modal: true
        padding: 18 * s.spacing
        Overlay.modal: Rectangle { color: Qt.alpha(s.background, 0.5) }
        background: Rectangle { color: s.popupBackground; radius: theme.radius; border.width: 1; border.color: s.border }
    }

    Card {
        id: help
        ColumnLayout {
            spacing: 6 * s.spacing
            Repeater {
                model: [
                    ["space", qsTr("Generate a batch (or make the prompt in manual mode)")],
                    ["enter", qsTr("Keep the selected candidate")],
                    ["d", qsTr("Discard the selected candidate")],
                    ["v", qsTr("Vary: new batch using the selected candidate as reference")],
                    ["p", qsTr("Preview on the desktop, or put the background back")],
                    ["← → ↑ ↓", qsTr("Move between candidates")],
                    ["/", qsTr("Type a scene")],
                    ["i", qsTr("Import images")],
                    ["c", qsTr("Copy the prompt")],
                    ["esc", qsTr("End preview, cancel, or leave the scene field")],
                    ["?", qsTr("Show these keys")],
                    ["q", qsTr("Quit")]
                ]
                delegate: RowLayout {
                    required property var modelData
                    spacing: 16 * s.spacing
                    OText { Layout.preferredWidth: 80 * s.spacing; text: modelData[0]; color: s.accent }
                    OText { text: modelData[1] }
                }
            }
        }
    }

    Card {
        id: quitDialog
        ColumnLayout {
            spacing: 14 * s.spacing
            OText { text: qsTr("Quit Easel?"); scale: 1.333 }
            OText {
                text: studio.busy ? qsTr("Candidates are still generating and will be lost.")
                                  : qsTr("%n candidate(s) haven't been kept and will be lost.", "", studio.unkept)
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: win.gap
                OButton { text: qsTr("Cancel"); onClicked: quitDialog.close() }
                OButton { text: qsTr("Quit"); primary: true; onClicked: Qt.quit() }
            }
        }
    }
}
