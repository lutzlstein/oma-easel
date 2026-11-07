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
    title: Qt.application.displayName

    // Everything takes the shell's look from the current theme's shell.toml,
    // so Oma Easel sits on the desktop like the bar, menus, and panels do.
    readonly property var s: theme.style
    readonly property real gap: 8 * s.spacing
    FontLoader { id: wordmarkFont; source: "qrc:/assets/fonts/Bitcount.ttf" }
    color: s.background
    font.family: s.fontFamily
    font.pixelSize: s.fontSize

    property int selected: 0
    // Set once the user confirms quitting. Qt.quit() closes the window first,
    // and the close check below must not veto a quit that was already confirmed.
    property bool quitting: false
    function quitNow() { quitting = true; Qt.quit() }
    readonly property bool typing: sceneField.activeFocus || varyField.activeFocus || (win.activeFocusItem instanceof TextInput)
    readonly property bool overlayOpen: help.visible || quitDialog.visible || varyCard.visible || removeCard.visible || pairCard.visible
                                        || runCard.visible || stopCard.visible

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
        if (studio.anyRunning)
            quitDialog.open()
        else
            win.quitNow()
    }
    function canUse(index) {
        const c = studio.candidates[index]
        return c !== undefined && (c.state === "ready" || c.state === "kept")
    }
    // Runs and stops both ask first: a run spends image credits, a stop throws
    // work away. Manual mode only builds a prompt, so it doesn't ask.
    function requestGenerate() {
        if (studio.busy)
            return
        if (studio.scene.trim().length === 0) { studio.generate(); return }
        if (studio.manual) { studio.generate(); return }
        runCard.open()
    }
    function requestStop() {
        if (studio.busy)
            stopCard.open()
    }
    function askVary(index) {
        if (!canUse(index))
            return
        win.selected = index
        varyCard.open()
    }

    Component.onCompleted: {
        if (screenshotOpen === "settings") settingsDrawer.open()
        else if (screenshotOpen === "history") history.open()
        else if (screenshotOpen === "help") help.open()
        else if (screenshotOpen === "batch" && studio.history.length > 0) studio.openBatch(studio.history[0].batch)
    }

    Connections {
        target: studio
        function onCandidatesChanged() { win.clampSelection() }
        function onThemeChanged() { win.selected = 0 }
        function onSelectCandidate(index) { win.selected = index; grid.forceActiveFocus() }
        function onPairNeeded(index) { pairCard.index = index; pairCard.open() }
    }

    onClosing: (close) => {
        if (studio.anyRunning && !win.quitting) {
            close.accepted = false
            quitDialog.open()
        }
    }

    // Letter keys act on candidates only while nothing is being typed; Enter
    // in the scene field generates.
    readonly property bool keysLive: !typing && !overlayOpen
    Shortcut { sequence: "?"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: help.visible ? help.close() : help.open() }
    Shortcut { sequence: "Q"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.requestQuit() }
    Shortcut { sequence: "Space"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.requestGenerate() }
    Shortcut { sequences: ["Return", "Enter"]; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: studio.keep(win.selected) }
    Shortcut { sequence: "D"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: studio.discard(win.selected) }
    Shortcut { sequence: "V"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.askVary(win.selected) }
    Shortcut { sequence: "R"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: if (win.canUse(win.selected)) studio.toggleReference(studio.candidates[win.selected].path) }
    Shortcut { sequence: "P"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: studio.preview(win.selected) }
    Shortcut { sequence: "H"; enabled: win.keysLive || history.visible; context: Qt.ApplicationShortcut; onActivated: history.visible ? history.close() : history.open() }
    Shortcut { sequence: "S"; enabled: win.keysLive || settingsDrawer.visible; context: Qt.ApplicationShortcut; onActivated: settingsDrawer.visible ? settingsDrawer.close() : settingsDrawer.open() }
    Shortcut { sequence: "I"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: studio.pickImports() }
    Shortcut { sequence: "C"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: studio.copyPrompt() }
    Shortcut { sequence: "Left"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.move(-1) }
    Shortcut { sequence: "Right"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.move(1) }
    Shortcut { sequence: "Up"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.move(-grid.columnsAt(win.selected)) }
    Shortcut { sequence: "Down"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: win.move(grid.columnsAt(win.selected)) }
    Shortcut { sequence: "O"; enabled: win.keysLive; context: Qt.ApplicationShortcut; onActivated: if (win.canUse(win.selected)) studio.companion(win.selected) }
    Shortcut { sequences: ["/", "Ctrl+L"]; enabled: !win.overlayOpen; context: Qt.ApplicationShortcut; onActivated: sceneField.forceActiveFocus() }
    Shortcut {
        sequence: "Escape"; enabled: !varyCard.visible; context: Qt.ApplicationShortcut
        onActivated: {
            if (help.visible) help.close()
            else if (quitDialog.visible) quitDialog.close()
            else if (removeCard.visible) removeCard.close()
            else if (pairCard.visible) pairCard.close()
            else if (history.visible) history.close()
            else if (settingsDrawer.visible) settingsDrawer.close()
            else if (studio.busy) win.requestStop()
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

                Row {
                    spacing: win.gap
                    AppLogo {
                        height: 34 * s.spacing; width: height * 512 / 444
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    // The wordmark is set in Bitcount at weight 400 (SIL OFL,
                    // bundled in src/assets/fonts), one word per line; the rest
                    // of the window uses the system font.
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: -3 * s.spacing
                        Repeater {
                            model: Qt.application.displayName.split(" ")
                            OText {
                                required property string modelData
                                text: modelData
                                font.family: wordmarkFont.font.family
                                font.weight: 400
                                font.pixelSize: Math.round(18 * s.spacing)
                            }
                        }
                    }
                }

                // The theme and, under it, its palette.
                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: 6 * s.spacing
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
                }

                Item { Layout.fillWidth: true }

                OText {
                    text: studio.manual ? qsTr("manual · copy the prompt to any image platform")
                                        : qsTr("making images with %1").arg(studio.backendLabel)
                    color: studio.manual ? s.accent : Qt.alpha(s.text, 0.55)
                    TapHandler { onTapped: settingsDrawer.open() }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
                OText { text: studio.exportSize; muted: true }
                // What the image agent is doing, for every theme. Click to go
                // to that theme's batch.
                Repeater {
                    model: studio.activity
                    OButton {
                        required property var modelData
                        primary: true
                        text: {
                            const t = modelData.seconds
                            const clock = Math.floor(t / 60) + ":" + String(t % 60).padStart(2, "0")
                            const done = modelData.total - modelData.pending
                            return qsTr("%1 · generating %2/%3 · %4").arg(modelData.themeName).arg(done).arg(modelData.total).arg(clock)
                        }
                        onClicked: modelData.theme === studio.themeSlug ? studio.openBatch(modelData.batch)
                                                                      : studio.loadTheme(modelData.theme)
                    }
                }
                OButton {
                    keyHint: "h"
                    text: qsTr("history %1").arg(studio.history.length)
                    onClicked: history.visible ? history.close() : history.open()
                }
            }

            // References: the theme's backgrounds, then any candidates chosen
            // as references. Clicking toggles.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: win.gap
                OText {
                    text: studio.references.length === 0
                          ? qsTr("Style references · click backgrounds or candidates to use them (optional)")
                          : qsTr("%n reference(s)", "", studio.references.length)
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
                    delegate: Thumbnail {
                        required property var modelData
                        width: height * 16 / 9
                        height: references.height
                        path: modelData.path
                        chosen: modelData.selected
                        dimmed: studio.references.length > 0 && !modelData.selected
                        caption: [modelData.added ? qsTr("added") : "", ...modelData.variants].filter(x => x).join(" · ")
                        removable: modelData.added
                        onClicked: studio.toggleReference(modelData.path)
                        onRemoveRequested: { removeCard.path = modelData.path; removeCard.open() }
                    }
                    footer: Row {
                        height: references.height
                        leftPadding: studio.extraReferences.length > 0 ? win.gap : 0
                        spacing: win.gap
                        Repeater {
                            model: studio.extraReferences
                            Thumbnail {
                                required property var modelData
                                width: height * studio.aspect
                                height: references.height
                                path: modelData.path
                                chosen: true
                                caption: qsTr("candidate")
                                onClicked: studio.toggleReference(modelData.path)
                            }
                        }
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
                    onAccepted: { grid.forceActiveFocus(); win.requestGenerate() }
                    Keys.onEscapePressed: grid.forceActiveFocus()
                }
                OText { text: studio.enabledDisplays > 1 ? qsTr("per screen") : qsTr("candidates"); muted: true; visible: !studio.manual }
                OStepper {
                    visible: !studio.manual
                    from: 1; to: 6
                    value: studio.count
                    onModified: (value) => studio.count = value
                }
                // Which screens to compose for; each shape gets its own grid.
                Repeater {
                    model: studio.manual || studio.displays.length < 2 ? [] : studio.displays
                    OButton {
                        required property var modelData
                        text: (modelData.monitors ? modelData.monitors + " " : "") + modelData.ratio
                        primary: modelData.enabled
                        onClicked: studio.toggleDisplay(modelData.key)
                    }
                }
                OButton {
                    primary: true
                    keyHint: "space"
                    enabled: studio.scene.trim().length > 0 && !studio.busy
                    text: studio.manual ? qsTr("Make prompt") : qsTr("Generate")
                    onClicked: { grid.forceActiveFocus(); win.requestGenerate() }
                }
                OButton {
                    visible: studio.busy
                    keyHint: "esc"
                    text: qsTr("Stop")
                    onClicked: win.requestStop()
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
                        OButton { keyHint: "c"; text: qsTr("Copy prompt"); primary: true; onClicked: studio.copyPrompt() }
                        OButton { text: qsTr("Copy reference paths"); visible: studio.references.length > 0; onClicked: studio.copyReferencePaths() }
                        OButton { text: qsTr("Open reference folder"); visible: studio.references.length > 0; onClicked: studio.openReferenceFolder() }
                        OButton { keyHint: "i"; text: qsTr("Import images…"); onClicked: studio.pickImports() }
                        OText { text: qsTr("or drop them onto this window"); muted: true }
                    }
                }
            }

            // The prompt the batch on screen was made from.
            OText {
                Layout.fillWidth: true
                visible: studio.candidates.length > 0 && studio.batchScene !== ""
                text: "“" + studio.batchScene + "”" + (studio.batchChange ? qsTr("  ·  varied: %1").arg(studio.batchChange) : "")
                elide: Text.ElideRight
                muted: true
            }

            // The batch, one section per screen shape side by side. Each
            // section picks its columns so its candidates come out as large as
            // its share of the space allows.
            Item {
                id: grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                focus: true
                clip: true

                readonly property real headerHeight: studio.sections.length > 1 ? 28 * s.spacing : 0
                // The best layout of n tiles of one aspect in a w x h box.
                function fit(n, w, h, aspect) {
                    let best = { columns: 1, width: 0 }
                    for (let c = 1; c <= Math.max(1, n); ++c) {
                        const rows = Math.ceil(n / c)
                        const tw = Math.min(w / c, h / rows * aspect)
                        if (tw > best.width + 0.5) best = { columns: c, width: tw }
                    }
                    return { columns: best.columns, width: Math.floor(best.width), height: Math.floor(best.width / aspect) }
                }
                // How much of the width each section gets. Two sections search
                // for the split where both sets of tiles are as large as they
                // can be together; more share it evenly.
                readonly property var shares: {
                    const list = studio.sections
                    const h = height - headerHeight
                    if (list.length <= 1) return [1]
                    if (list.length > 2) return list.map(() => 1 / list.length)
                    let best = 0.5, bestScore = -Infinity
                    for (let f = 0.15; f <= 0.851; f += 0.01) {
                        const a = fit(list[0].indices.length, width * f, h, list[0].aspect)
                        const b = fit(list[1].indices.length, width * (1 - f), h, list[1].aspect)
                        if (a.width <= 0 || b.width <= 0) continue
                        const score = Math.log(a.width * a.height) + Math.log(b.width * b.height)
                        if (score > bestScore) { bestScore = score; best = f }
                    }
                    return [best, 1 - best]
                }
                function sectionOf(index) {
                    const list = studio.sections
                    for (let i = 0; i < list.length; ++i)
                        if (list[i].indices.indexOf(index) !== -1) return i
                    return 0
                }
                function columnsAt(index) {
                    const i = sectionOf(index)
                    const section = studio.sections[i]
                    if (!section) return 1
                    return fit(section.indices.length, width * shares[i], height - headerHeight, section.aspect).columns
                }
                function positionViewAtIndex() {}

                Row {
                    anchors.fill: parent
                    Repeater {
                        model: studio.sections
                        Item {
                            id: section
                            required property var modelData
                            required property int index
                            width: grid.width * (grid.shares[index] || 0)
                            height: grid.height
                            readonly property var layout: grid.fit(modelData.indices.length, width, height - grid.headerHeight, modelData.aspect)

                            OText {
                                visible: grid.headerHeight > 0
                                x: (section.width - tiles.width) / 2 + win.gap / 2
                                height: grid.headerHeight
                                verticalAlignment: Text.AlignVCenter
                                text: (section.modelData.monitors ? section.modelData.monitors + " · " : "") + section.modelData.ratio
                                      + (section.modelData.primary ? "" : qsTr(" · versions"))
                                muted: true
                            }
                            Grid {
                                id: tiles
                                y: grid.headerHeight
                                x: (section.width - width) / 2
                                columns: section.layout.columns
                                Repeater {
                                    model: section.modelData.indices
                                    CandidateTile {
                                        required property var modelData
                                        index: modelData
                                        candidate: studio.candidates[modelData] || ({})
                                        width: section.layout.width
                                        height: section.layout.height
                                    }
                                }
                            }
                        }
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

            // Attribution, settings, status, and the keys that matter most.
            RowLayout {
                Layout.fillWidth: true
                spacing: 14 * s.spacing
                OText {
                    text: qsTr("by %1").arg(appAuthor)
                    muted: !attributionHover.hovered
                    font.underline: attributionHover.hovered
                    HoverHandler { id: attributionHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: Qt.openUrlExternally(appRepository) }
                }
                OText {
                    text: qsTr("support on Ko-fi")
                    muted: !supportHover.hovered
                    font.underline: supportHover.hovered
                    HoverHandler { id: supportHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: Qt.openUrlExternally(appSupport) }
                }
                OButton { keyHint: "s"; text: qsTr("models & export"); onClicked: settingsDrawer.open() }
                OText { Layout.fillWidth: true; text: studio.message; elide: Text.ElideRight }
                Row {
                    spacing: 14 * s.spacing
                    KeyHint { key: "space"; label: qsTr("generate") }
                    KeyHint { key: "enter"; label: qsTr("keep") }
                    KeyHint { key: "v"; label: qsTr("vary") }
                    KeyHint { key: "o"; label: qsTr("companion"); visible: studio.displays.length > 1 }
                    KeyHint { key: "r"; label: qsTr("reference") }
                    KeyHint { key: "p"; label: qsTr("preview") }
                    KeyHint { key: "h"; label: qsTr("history") }
                    KeyHint { key: "?"; label: qsTr("keys") }
                }
            }
        }
    }

    // One candidate in the batch, with its actions on hover or selection.
    component CandidateTile: Item {
                    id: cell
                    property var candidate
                    property int index
                    readonly property var modelData: candidate
                    readonly property bool isSelected: index === win.selected
                    readonly property bool hasImage: modelData.state === "ready" || modelData.state === "kept"

                    Rectangle {
                        id: frame
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
                            sourceSize.width: modelData.aspect < 1 ? 900 : 1600
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
                            color: s.accent
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Row {
                            anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 10
                            spacing: 6
                            Badge { visible: modelData.state === "kept"; text: qsTr("kept") }
                            Badge { visible: modelData.reference; text: qsTr("reference") }
                            Badge { visible: studio.previewing === cell.index; text: qsTr("previewing") }
                        }

                        HoverHandler { id: cellHover }
                        TapHandler { onTapped: { win.selected = cell.index; grid.forceActiveFocus() } }
                        TapHandler { onDoubleTapped: studio.preview(cell.index) }

                        // Mouse actions, mirroring the keys, while hovered or selected.
                        // Wraps upward on narrow tiles, such as portrait ones.
                        Flow {
                            anchors.bottom: parent.bottom; anchors.right: parent.right; anchors.margins: 10
                            width: parent.width - 20
                            layoutDirection: Qt.RightToLeft
                            spacing: 6
                            visible: cell.hasImage && (cellHover.hovered || cell.isSelected)
                            OButton { keyHint: "enter"; text: modelData.variant ? qsTr("keep %1").arg(modelData.ratio) : qsTr("keep"); visible: modelData.state === "ready"; onClicked: studio.keep(cell.index); solid: true }
                            OButton { keyHint: "o"; text: qsTr("companion"); visible: studio.displays.length > 1 && !studio.manual; onClicked: studio.companion(cell.index); solid: true }
                            OButton { keyHint: "v"; text: qsTr("vary"); onClicked: win.askVary(cell.index); solid: true }
                            OButton { keyHint: "r"; text: modelData.reference ? qsTr("unreference") : qsTr("reference"); onClicked: studio.toggleReference(modelData.path); solid: true }
                            OButton { keyHint: "p"; text: qsTr("preview"); onClicked: studio.preview(cell.index); solid: true }
                            OButton { keyHint: "d"; text: qsTr("discard"); onClicked: studio.discard(cell.index); solid: true }
                        }
                    }
                }

    component Badge: Rectangle {
        property alias text: label.text
        radius: Math.min(3, theme.radius)
        color: s.background
        border.width: 1
        border.color: s.border
        width: label.implicitWidth + 12; height: label.implicitHeight + 6
        OText { id: label; anchors.centerIn: parent }
    }

    // Models & export: which image model makes candidates, how to connect
    // it, and how kept backgrounds are written.
    Drawer {
        id: settingsDrawer
        edge: Qt.LeftEdge
        width: Math.min(win.width * 0.45, 560 * s.spacing)
        height: win.height
        onOpened: studio.refreshBackends()
        Overlay.modal: Rectangle { color: Qt.alpha(s.background, 0.4) }
        background: Rectangle {
            color: s.popupBackground
            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: s.border }
        }

        ScrollView {
            anchors.fill: parent
            anchors.margins: 18 * s.spacing
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 12 * s.spacing

                OText { text: qsTr("Models & export"); size: 1.333 }

                OText { text: qsTr("Make images with"); muted: true; Layout.topMargin: 6 }
                Repeater {
                    model: studio.backends
                    delegate: Rectangle {
                        id: backendCard
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: backendColumn.implicitHeight + 24 * s.spacing
                        radius: theme.radius
                        color: Qt.alpha(s.controlColor, modelData.active ? s.selectedFill : s.normalFill)
                        border.width: modelData.active ? 2 : s.borderWidth
                        border.color: modelData.active ? s.border : Qt.alpha(s.controlColor, s.normalBorderAlpha)
                        ColumnLayout {
                            id: backendColumn
                            anchors.fill: parent
                            anchors.margins: 12 * s.spacing
                            spacing: 8 * s.spacing
                            RowLayout {
                                Layout.fillWidth: true
                                OText { text: backendCard.modelData.label; Layout.fillWidth: true }
                                OText { visible: backendCard.modelData.active; text: qsTr("in use"); color: s.accent }
                                OButton {
                                    visible: !backendCard.modelData.active
                                    enabled: backendCard.modelData.available
                                    text: qsTr("use")
                                    onClicked: studio.chooseBackend(backendCard.modelData.name)
                                }
                            }
                            OText { Layout.fillWidth: true; text: backendCard.modelData.detail; wrapMode: Text.Wrap; muted: true }
                            // API backends: the model id and the key.
                            RowLayout {
                                visible: backendCard.modelData.keyed
                                Layout.fillWidth: true
                                OText { text: qsTr("model"); muted: true; Layout.preferredWidth: 60 * s.spacing }
                                OField {
                                    Layout.fillWidth: true
                                    text: backendCard.modelData.model
                                    onEditingFinished: {
                                        if (backendCard.modelData.name === "openai") studio.settings.openaiModel = text
                                        else studio.settings.geminiModel = text
                                        studio.refreshBackends()
                                    }
                                }
                            }
                            RowLayout {
                                visible: backendCard.modelData.keyed && !backendCard.modelData.keyFromEnvironment
                                Layout.fillWidth: true
                                OText { text: qsTr("key"); muted: true; Layout.preferredWidth: 60 * s.spacing }
                                OField {
                                    id: keyField
                                    Layout.fillWidth: true
                                    echoMode: TextInput.Password
                                    placeholderText: backendCard.modelData.available ? qsTr("saved in the keyring · paste to replace")
                                                                                     : qsTr("paste an API key")
                                    onAccepted: { studio.saveKey(backendCard.modelData.name, text); text = "" }
                                }
                                OButton { text: qsTr("save"); enabled: keyField.text.length > 0; onClicked: { studio.saveKey(backendCard.modelData.name, keyField.text); keyField.text = "" } }
                                OButton { text: qsTr("forget"); visible: backendCard.modelData.available; onClicked: studio.forgetKey(backendCard.modelData.name) }
                            }
                        }
                    }
                }

                OText { text: qsTr("Export kept backgrounds as"); muted: true; Layout.topMargin: 12 }
                RowLayout {
                    spacing: win.gap
                    Repeater {
                        model: [["webp", "WebP"], ["jpeg", "JPEG"], ["png", "PNG"]]
                        OButton {
                            required property var modelData
                            text: modelData[1]
                            primary: studio.settings.exportFormat === modelData[0]
                            onClicked: studio.settings.exportFormat = modelData[0]
                        }
                    }
                    OText { text: qsTr("quality"); muted: true; visible: studio.settings.exportFormat !== "png"; Layout.leftMargin: 8 }
                    OStepper {
                        visible: studio.settings.exportFormat !== "png"
                        from: 40; to: 100
                        value: studio.settings.exportQuality
                        onModified: (value) => studio.settings.exportQuality = value > studio.settings.exportQuality
                                    ? Math.min(100, studio.settings.exportQuality + 5) : Math.max(40, studio.settings.exportQuality - 5)
                    }
                }
                OText {
                    Layout.fillWidth: true
                    text: qsTr("Sized to %1 for your screens, into %2's backgrounds.").arg(studio.exportSize).arg(studio.themeName)
                    wrapMode: Text.Wrap
                    muted: true
                }

                OText { text: qsTr("Also copy kept backgrounds for %1 to (optional)").arg(studio.themeName); muted: true; Layout.topMargin: 12 }
                RowLayout {
                    Layout.fillWidth: true
                    OField {
                        id: extraFolder
                        Layout.fillWidth: true
                        placeholderText: qsTr("a folder, like a theme repo's backgrounds/")
                        text: studio.extraExportFolder()
                        onEditingFinished: studio.setExtraExportFolder(text)
                        Connections { target: studio; function onThemeChanged() { extraFolder.text = studio.extraExportFolder() } }
                    }
                    OButton { text: qsTr("clear"); visible: extraFolder.text.length > 0; onClicked: { extraFolder.text = ""; studio.setExtraExportFolder("") } }
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

    // Every earlier batch for the theme, with its prompt. Click a candidate to
    // use it as a reference; open a batch to bring it back to the grid.
    Drawer {
        id: history
        edge: Qt.RightEdge
        width: Math.min(win.width * 0.4, 520 * s.spacing)
        height: win.height
        Overlay.modal: Rectangle { color: Qt.alpha(s.background, 0.4) }
        background: Rectangle {
            color: s.popupBackground
            Rectangle { width: 1; height: parent.height; color: s.border }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16 * s.spacing
            spacing: 12 * s.spacing

            RowLayout {
                Layout.fillWidth: true
                OText { Layout.fillWidth: true; text: qsTr("History · %1").arg(studio.themeName); size: 1.167 }
                OText { text: qsTr("click to reference · double-click to open"); muted: true }
            }
            OText {
                visible: studio.history.length === 0
                text: qsTr("Nothing generated for this theme yet.")
                muted: true
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 18 * s.spacing
                model: studio.history
                ScrollIndicator.vertical: ScrollIndicator {}
                delegate: ColumnLayout {
                    id: batchItem
                    required property var modelData
                    width: ListView.view.width
                    spacing: 6 * s.spacing
                    RowLayout {
                        Layout.fillWidth: true
                        OText {
                            Layout.fillWidth: true
                            text: "“" + (batchItem.modelData.scene || qsTr("untitled")) + "”"
                            wrapMode: Text.Wrap
                        }
                        OButton { text: qsTr("open"); onClicked: { studio.openBatch(batchItem.modelData.batch); history.close() } }
                    }
                    OText {
                        Layout.fillWidth: true
                        text: [Qt.formatDateTime(new Date(batchItem.modelData.createdAt), "MMM d, hh:mm"),
                               batchItem.modelData.backend,
                               batchItem.modelData.change ? qsTr("varied: %1").arg(batchItem.modelData.change) : ""]
                              .filter(x => x).join("  ·  ")
                        wrapMode: Text.Wrap
                        muted: true
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: batchItem.modelData.candidates
                            Thumbnail {
                                required property var modelData
                                width: modelData.aspect < 1 ? (batchItem.width - 18) / 4 : (batchItem.width - 6) / 2
                                height: width / (modelData.aspect || studio.aspect)
                                path: modelData.path
                                chosen: modelData.reference
                                caption: modelData.keptAs ? qsTr("kept") : ""
                                onClicked: studio.toggleReference(modelData.path)
                                TapHandler {
                                    onDoubleTapped: { studio.openBatch(batchItem.modelData.batch, modelData.path); history.close() }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Vary asks what to change. Enter varies with that change; dismissing it
    // varies the usual way, with the candidate as the only style reference.
    Card {
        id: varyCard
        property bool submitted: false
        width: Math.min(win.width * 0.6, 720 * s.spacing)
        onOpened: { submitted = false; varyField.text = ""; varyField.forceActiveFocus() }
        onClosed: {
            studio.vary(win.selected, submitted ? varyField.text : "")
            grid.forceActiveFocus()
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 12 * s.spacing
            OText { text: qsTr("Vary this candidate"); size: 1.167 }
            OField {
                id: varyField
                Layout.fillWidth: true
                placeholderText: qsTr("What should change? warmer light, add fog, a boat on the water…")
                onAccepted: { varyCard.submitted = true; varyCard.close() }
            }
            KeyHint { key: "enter"; label: qsTr("keep this image and change only that") }
            KeyHint { key: "esc"; label: qsTr("fresh takes on the same scene, in its style") }
        }
    }

    // Keeping a candidate made for another screen shape: which background
    // is it a version of?
    Card {
        id: pairCard
        property int index: -1
        width: Math.min(win.width * 0.7, 900 * s.spacing)
        onClosed: grid.forceActiveFocus()
        ColumnLayout {
            anchors.fill: parent
            spacing: 12 * s.spacing
            OText {
                text: qsTr("Which background is this %1 version for?").arg(studio.candidates[pairCard.index] ? studio.candidates[pairCard.index].ratio : "")
                size: 1.167
            }
            OText { text: qsTr("It is shown on screens of its shape, and replaces any version that background already has."); muted: true }
            Flow {
                Layout.fillWidth: true
                spacing: win.gap
                Repeater {
                    model: studio.backgrounds
                    Thumbnail {
                        required property var modelData
                        width: 200 * s.spacing
                        height: width / studio.aspect
                        path: modelData.path
                        caption: modelData.variants.length > 0 ? qsTr("has %1").arg(modelData.variants.join(", ")) : modelData.name
                        onClicked: { studio.keepVariant(pairCard.index, modelData.path); pairCard.close() }
                    }
                }
            }
            KeyHint { key: "esc"; label: qsTr("cancel") }
        }
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
                    ["v", qsTr("Vary: say what to change, or esc for fresh takes")],
                    ["o", qsTr("Companion: the same scene composed for your other screens")],
                    ["r", qsTr("Use the selected candidate as a reference")],
                    ["p", qsTr("Preview on an empty workspace")],
                    ["h", qsTr("History of earlier batches")],
                    ["s", qsTr("Models & export settings")],
                    ["← → ↑ ↓", qsTr("Move between candidates")],
                    ["/", qsTr("Type a scene")],
                    ["i", qsTr("Import images")],
                    ["c", qsTr("Copy the prompt")],
                    ["esc", qsTr("Close, cancel, or leave the scene field")],
                    ["?", qsTr("Show these keys")],
                    ["q", qsTr("Quit")]
                ]
                delegate: RowLayout {
                    required property var modelData
                    spacing: 16 * s.spacing
                    Item {
                        Layout.preferredWidth: 90 * s.spacing
                        implicitHeight: keyRow.implicitHeight
                        // Several keys ("← → ↑ ↓") get a cap each.
                        Row {
                            id: keyRow
                            spacing: 4
                            Repeater { model: modelData[0].split(" "); KeyCap { required property string modelData; key: modelData } }
                        }
                    }
                    OText { text: modelData[1] }
                }
            }
        }
    }

    // A confirmation card: enter or space confirms, esc backs out.
    component ConfirmCard: Card {
        id: confirm
        property string heading
        property string detail
        property string confirmLabel
        property string backLabel: qsTr("Cancel")
        signal confirmed()
        onOpened: keys.forceActiveFocus()
        onClosed: grid.forceActiveFocus()
        ColumnLayout {
            spacing: 14 * s.spacing
            Item {
                id: keys
                focus: true
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                        event.accepted = true
                        confirm.close()
                        confirm.confirmed()
                    }
                }
            }
            OText { text: confirm.heading; size: 1.333 }
            OText {
                visible: confirm.detail !== ""
                text: confirm.detail
                wrapMode: Text.Wrap
                Layout.maximumWidth: 560 * s.spacing
                muted: true
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: win.gap
                OButton { keyHint: "esc"; text: confirm.backLabel; onClicked: confirm.close() }
                OButton { keyHint: "enter"; text: confirm.confirmLabel; primary: true; onClicked: { confirm.close(); confirm.confirmed() } }
            }
        }
    }

    ConfirmCard {
        id: runCard
        heading: qsTr("Generate %n candidate(s) for %1?", "", studio.count * studio.enabledDisplays).arg(studio.themeName)
        detail: "“" + studio.scene.trim() + "”"
                + (studio.references.length > 0 ? "\n" + qsTr("%n reference(s)", "", studio.references.length) : "")
                + "\n" + qsTr("with %1").arg(studio.backend)
        confirmLabel: qsTr("Generate")
        onConfirmed: studio.generate()
    }

    ConfirmCard {
        id: stopCard
        heading: qsTr("Stop generating?")
        detail: qsTr("Candidates still generating in this batch are lost. Finished ones stay.")
        confirmLabel: qsTr("Stop")
        backLabel: qsTr("Keep going")
        onConfirmed: studio.cancel()
    }

    Card {
        id: removeCard
        property string path
        ColumnLayout {
            spacing: 14 * s.spacing
            OText { text: qsTr("Remove this background?"); size: 1.333 }
            OText {
                text: qsTr("%1 leaves %2's background cycle. It and its master go to the trash, and the candidate stays in history.")
                      .arg(removeCard.path.split("/").pop()).arg(studio.themeName)
                wrapMode: Text.Wrap
                Layout.maximumWidth: 520 * s.spacing
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: win.gap
                OButton { keyHint: "esc"; text: qsTr("Cancel"); onClicked: removeCard.close() }
                OButton { text: qsTr("Remove"); primary: true; onClicked: { studio.removeBackground(removeCard.path); removeCard.close() } }
            }
        }
    }

    Card {
        id: quitDialog
        ColumnLayout {
            spacing: 14 * s.spacing
            OText { text: qsTr("Quit %1?").arg(Qt.application.displayName); size: 1.333 }
            OText { text: qsTr("Images are still generating. Quitting stops them.") }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: win.gap
                OButton { keyHint: "esc"; text: qsTr("Cancel"); onClicked: quitDialog.close() }
                OButton { text: qsTr("Quit"); primary: true; onClicked: win.quitNow() }
            }
        }
    }
}
