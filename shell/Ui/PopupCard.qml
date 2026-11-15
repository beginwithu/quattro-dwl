import QtQuick
import Quickshell
import Quickshell.Wayland
import qs.Commons
import qs.Commons as Commons

// Layer-shell popup card anchored to a bar widget. Uses a full-screen
// PanelWindow (like KeyboardPanel) rather than xdg-popup (PopupWindow) so
// dynamic content resizing (such as async DBus tray menus) never stretches
// an initial buffer across a Wayland popup configure cycle.
PanelWindow {
  id: root

  required property Item anchorItem
  required property QtObject bar
  property var owner: null
  property int margin: Style.gapsOut
  property int padding: Style.spacing.popupPadding
  property int contentWidth: Style.space(280)
  property int contentHeight: Style.space(200)
  property color borderColor: Commons.Color.popups.border
  property var borderSpec: Border.localOrSurfaceSpec("popups", "border", borderColor, Commons.Color.popups.border, Math.max(1, Style.space(2)))
  property bool open: false
  property bool centerOnBar: false
  // "click" — dismisses when clicking outside the card on any screen.
  // "hover" — only the card region accepts input; owner controls open via hover.
  property string triggerMode: "click"

  readonly property var coordinatorKey: owner || root
  readonly property var anchorWindow: anchorItem ? anchorItem.QsWindow.window : null
  readonly property var popupScreen: screen
  readonly property string barPos: bar ? bar.position : "top"
  readonly property bool containsMouse: cardHover.hovered
  readonly property real screenW: screen ? screen.width : 0
  readonly property real screenH: screen ? screen.height : 0
  // Thickness of the painted bar strip on this screen, notch floor
  // included. The bar window spans its whole screen (only the strip inside
  // it is painted and takes input), so the window's dimensions say nothing
  // about the bar.
  readonly property real barThickness: !bar ? 0
    : (typeof bar.barSizeFor === "function" && screen ? bar.barSizeFor(screen.name) : bar.barSize)
  readonly property real availableCardWidth: screenW > 0
    ? Math.max(120, screenW - ((barPos === "left" || barPos === "right") ? barThickness : 0) - root.margin * 2)
    : 0
  readonly property real availableCardHeight: screenH > 0
    ? Math.max(120, screenH - ((barPos === "top" || barPos === "bottom") ? barThickness : 0) - root.margin * 2)
    : 0
  readonly property real verticalContentInset: padding * 2 + Border.top(borderSpec) + Border.bottom(borderSpec)

  function fittedContentWidth(width, cap) {
    var desired = Math.max(1, Number(width) || 1)
    var maxWidth = root.availableCardWidth > 0 ? root.availableCardWidth : desired
    if (cap !== undefined && Number(cap) > 0) maxWidth = Math.min(maxWidth, Number(cap))
    return Math.round(Math.min(desired, maxWidth))
  }

  function fittedContentHeight(implicitHeight, cap) {
    var desired = Math.max(root.verticalContentInset, (Number(implicitHeight) || 0) + root.verticalContentInset)
    var maxHeight = root.availableCardHeight > 0 ? root.availableCardHeight : desired
    if (cap !== undefined && Number(cap) > 0) maxHeight = Math.min(maxHeight, Number(cap))
    return Math.round(Math.min(desired, maxHeight))
  }

  function cappedContentHeight(height) {
    var desired = Math.max(root.padding * 2, Number(height) || root.padding * 2)
    var maxHeight = root.availableCardHeight > 0 ? root.availableCardHeight : desired
    return Math.round(Math.min(desired, maxHeight))
  }

  function close() {
    if (owner && "close" in owner) owner.close()
    else root.open = false
  }

  default property alias contentItem: contentHolder.children

  screen: anchorWindow ? anchorWindow.screen : null
  visible: open || card.opacity > 0
  color: "transparent"
  exclusionMode: ExclusionMode.Ignore

  WlrLayershell.namespace: "quattro-popup-card"
  WlrLayershell.layer: WlrLayer.Overlay
  WlrLayershell.keyboardFocus: WlrKeyboardFocus.None

  anchors {
    top: true
    bottom: true
    left: true
    right: true
  }

  // A closing card stays mapped until its fade ends; leave its input region
  // empty then so clicks meant for the bar or windows beneath pass through.
  Region { id: emptyRegion }
  Region { id: cardRegion; item: card }
  Region { id: fullRegion; width: root.screenW; height: root.screenH }
  mask: !root.open ? emptyRegion : (root.triggerMode === "hover" ? cardRegion : fullRegion)

  TransformWatcher {
    id: anchorWatcher
    a: anchorWindow ? anchorWindow.contentItem : null
    b: anchorItem
  }

  readonly property point anchorScreenPos: {
    anchorWatcher.transform
    if (!anchorItem || !anchorWindow) return Qt.point(0, 0)
    return anchorItem.mapToItem(anchorWindow.contentItem, 0, 0)
  }
  readonly property real anchorW: anchorItem ? anchorItem.width : 0
  readonly property real anchorH: anchorItem ? anchorItem.height : 0

  readonly property point cardOrigin: {
    if (!anchorItem || !bar) return Qt.point(margin, margin)
    var x = 0
    var y = 0
    if (centerOnBar && (barPos === "top" || barPos === "bottom")) {
      x = screenW / 2 - contentWidth / 2
      y = barPos === "bottom" ? screenH - barThickness - contentHeight - margin : barThickness + margin
    } else if (centerOnBar) {
      x = barPos === "left" ? barThickness + margin : screenW - barThickness - contentWidth - margin
      y = screenH / 2 - contentHeight / 2
    } else if (barPos === "bottom") {
      x = anchorScreenPos.x + anchorW / 2 - contentWidth / 2
      y = screenH - barThickness - contentHeight - margin
    } else if (barPos === "left") {
      x = barThickness + margin
      y = anchorScreenPos.y + anchorH / 2 - contentHeight / 2
    } else if (barPos === "right") {
      x = screenW - barThickness - contentWidth - margin
      y = anchorScreenPos.y + anchorH / 2 - contentHeight / 2
    } else {
      x = anchorScreenPos.x + anchorW / 2 - contentWidth / 2
      y = barThickness + margin
    }
    x = Math.max(margin, Math.min(x, screenW - contentWidth - margin))
    y = Math.max(margin, Math.min(y, screenH - contentHeight - margin))
    return Qt.point(Math.round(x), Math.round(y))
  }

  readonly property point growOrigin: {
    var x = Math.max(0, Math.min(anchorScreenPos.x + anchorW / 2 - cardOrigin.x, contentWidth))
    var y = Math.max(0, Math.min(anchorScreenPos.y + anchorH / 2 - cardOrigin.y, contentHeight))
    if (barPos === "bottom") return Qt.point(x, contentHeight)
    if (barPos === "left") return Qt.point(0, y)
    if (barPos === "right") return Qt.point(contentWidth, y)
    return Qt.point(x, 0)
  }

  onOpenChanged: {
    if (!bar) return
    if (open) bar.requestPopout(coordinatorKey)
    else if (bar.activePopout === coordinatorKey) bar.releasePopout(coordinatorKey)
  }

  // Click anywhere outside the card on the active output to dismiss.
  MouseArea {
    anchors.fill: parent
    enabled: root.open && root.triggerMode === "click"
    acceptedButtons: Qt.AllButtons
    onPressed: root.close()
  }

  // Cover other outputs so clicking elsewhere dismisses the popup there too.
  Variants {
    model: root.open && root.triggerMode === "click" ? Quickshell.screens : []

    delegate: Component {
      PanelWindow {
        required property var modelData

        screen: modelData
        visible: root.open && !!root.screen && modelData.name !== root.screen.name
        color: "transparent"
        exclusionMode: ExclusionMode.Ignore
        WlrLayershell.namespace: "quattro-popup-dismiss"
        WlrLayershell.layer: WlrLayer.Overlay
        WlrLayershell.keyboardFocus: WlrKeyboardFocus.None

        anchors {
          top: true
          bottom: true
          left: true
          right: true
        }

        MouseArea {
          anchors.fill: parent
          acceptedButtons: Qt.AllButtons
          onPressed: root.close()
        }
      }
    }
  }

  BorderSurface {
    id: card
    x: root.cardOrigin.x
    y: root.cardOrigin.y
    width: root.contentWidth
    height: root.contentHeight
    color: Commons.Color.popups.background
    borderSpec: root.borderSpec
    padding: root.padding
    radius: Style.cornerRadius

    // Grows out of the bar from just under full size as it fades in, as the
    // keyboard panels do, and goes in two thirds of the time.
    property real shown: root.open ? 1 : 0
    Behavior on shown {
      id: showing
      NumberAnimation {
        duration: Style.duration(showing.targetValue > 0 ? 180 : 120)
        easing.type: Easing.Bezier
        easing.bezierCurve: Style.easeOut
      }
    }
    opacity: shown
    transform: Scale {
      origin.x: root.growOrigin.x
      origin.y: root.growOrigin.y
      xScale: 0.97 + 0.03 * card.shown
      yScale: 0.97 + 0.03 * card.shown
    }

    // Swallow clicks on the card so they don't bubble to the dismissal
    // MouseArea behind us.
    MouseArea {
      anchors.fill: parent
      acceptedButtons: Qt.AllButtons
    }

    Item {
      id: contentHolder
      anchors.fill: parent
      // Right-to-left languages read the card mirrored; the card itself
      // stays where the bar puts it.
      LayoutMirroring.enabled: Commons.I18n.rightToLeft
      LayoutMirroring.childrenInherit: true
      anchors.topMargin: card.contentTopInset
      anchors.rightMargin: card.contentRightInset
      anchors.bottomMargin: card.contentBottomInset
      anchors.leftMargin: card.contentLeftInset
    }

    HoverHandler {
      id: cardHover
    }
  }
}
