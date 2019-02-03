import QtQuick 2.7
import QtQuick.Controls 2.0 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.0 as Kirigami
import QtGraphicalEffects 1.0

Item {
  property string icon
  property string url
  property string type
  property string player
  property bool mobile: false
  property alias volume: volume_slider.value
  property bool fullscreen: false
  property string title

  property real last_volume: 1
  property bool playing: true
  property bool autoplay: false
  property color button_color: "#EEEEEE"
  property color tint_color: "#757575"
  property int extended_view_threshold: 525

  signal showCursor();
  signal hideCursor();
  signal errorOccurred(string domain, string message);
  signal ended();
  signal next();

  id: page

  onPlayingChanged: {
    if (playing) {
      player.item.play();
    } else {
      player.item.pause();
    }
  }

  function preferred_player() {
    return page.player === "mpv" ? "MpvPlayer.qml" : (page.player === "vlc" ? "VlcPlayer.qml" : "QtPlayer.qml");
  }

  function invoke_next() {
    player.item.source = "";
    next();
  }

  function print_timestamp(d) {
    d /= 1000;
    var hours = Math.floor(d / 3600);
    d %= 3600;
    var minutes = Math.floor(d / 60);
    d %= 60;
    var seconds = Math.floor(d);

    if (hours > 0 && minutes.toString().length == 1)
      minutes = "0" + minutes;

    if (seconds.toString().length == 1)
      seconds = "0" + seconds;

    return (hours > 0 ? hours + ":" : "") + minutes + ":" + seconds;
  }

  function set_url() {
    if (!player.item) return;
    player.item.source = url;
    if (playing) {
      player.item.play();
    }
  }

  onUrlChanged: set_url()

  Connections {
    id: connections
    target: null
    onEndedChanged: {
      if (target.ended && target.source.toString() !== "") {
        ended();
        if (autoplay) {
          invoke_next();
        }
      }
    }
    onError: {
      errorOccurred("MediaPlayer", error + ": " + errorString);
    }
  }

  MouseArea {
    id: mouse_area

    anchors.fill: parent
    hoverEnabled: true

    onContainsMouseChanged: {
      if (state === "overlay_visible" || !mouse_area.containsMouse)
        showCursor();
      else if (state === "overlay_invisible" && mouse_area.containsMouse)
        hideCursor();
    }

    onClicked: {
      if (page.state === "overlay_visible" &&
          Date.now() - timer.last_visible >= 2 * timer.interval &&
          (type === "video" || type === "audio"))
        playing ^= 1;
    }

    Rectangle {
      anchors.fill: parent
      color: "black"
    }

    Loader {
      id: player
      anchors.fill: parent
      asynchronous: true
      source: type === "video" || type === "audio" ? preferred_player() : "ImagePlayer.qml"
      onStatusChanged: {
        if (status === Loader.Ready) {
          set_url();
          connections.target = item;
        } else if (status === Loader.Error) {
          errorOccurred("LoadPlayer", source);
        }
      }
    }

    Timer {
      property real epsilon: 0.1
      property int idle_duration: 2000
      property real previous_x
      property real previous_y
      property int cnt
      property real last_visible

      function sqr(x) { return x * x; }

      id: timer
      interval: 100
      running: true
      repeat: true
      onTriggered: {
        var dist = sqr(previous_x - parent.mouseX) + sqr(previous_y - parent.mouseY);
        if (dist > epsilon) {
          cnt = 0;
          if (page.state === "overlay_invisible") {
            page.state = "overlay_visible";
            last_visible = Date.now();
          }
        } else if (page.state === "overlay_visible") {
          if ((!controls.containsMouse || mobile)
              && page.playing && player.item && !player.item.buffering
              && !volume_slider.visible)
            cnt++;
          if (cnt >= idle_duration / interval) {
            page.state = "overlay_invisible";
            cnt = 0;
          }
        }
        previous_x = parent.mouseX;
        previous_y = parent.mouseY;
      }
    }

    Component.onCompleted: {
      timer.previous_x = mouseX;
      timer.previous_y = mouseY;
    }
  }

  state: "overlay_visible"
  states: [
    State {
      name: "overlay_visible"
      PropertyChanges {
        target: controls
        opacity: 1
      }
      PropertyChanges {
        target: top_gradient
        opacity: 1
      }
    },
    State {
      name: "overlay_invisible"
      PropertyChanges {
        target: controls
        opacity: 0
      }
      PropertyChanges {
        target: top_gradient
        opacity: 0
      }
    }
  ]

  onStateChanged: {
    if (state === "overlay_visible") {
      showCursor();
    } else {
      if (mouse_area.containsMouse)
         hideCursor();
      subtitle_track_list.item.shown = false;
      audio_track_list.item.shown = false;
    }
  }

  transitions: [
    Transition {
      from: "overlay_invisible"
      to: "overlay_visible"
      PropertyAnimation {
        properties: "opacity"
        duration: 250
      }
    },
    Transition {
      from: "overlay_visible"
      to: "overlay_invisible"
      PropertyAnimation {
        properties: "opacity"
        duration: 250
      }
    }
  ]

  Kirigami.Icon {
    anchors.centerIn: parent
    width: 200
    height: 200
    visible: type === "audio"
    source: "audio-x-generic"
    isMask: true
  }

  Item {
    id: top_gradient
    anchors.top: parent.top
    anchors.left: parent.left
    anchors.right: parent.right
    height: 100
    visible: title !== ""

    LinearGradient {
      anchors.fill: parent
      gradient: Gradient {
        GradientStop {
          position: 0.0
          color: "#FF000000"
        }
        GradientStop {
          position: 1.0
          color: "#00000000"
        }
      }
    }
    Item {
      anchors.fill: parent
      anchors.margins: 20
      clip: true
      Text {
        text: title
        color: button_color
        font.pointSize: 20
      }
    }
  }

  MouseArea {
    id: controls
    hoverEnabled: !mobile
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    height: 50
    onClicked: timer.cnt = 0
    LinearGradient {
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.bottom: parent.bottom
      height: 100
      gradient: Gradient {
        GradientStop {
          position: 0.0
          color: "#00000000"
        }
        GradientStop {
          position: 1.0
          color: "#FF000000"
        }
      }
    }
    Row {
      anchors.fill: parent
      Kirigami.Icon {
        id: play_button
        width: height
        height: parent.height
        color: button_color
        isMask: true
        source: playing ? "media-playback-pause" : "media-playback-start"
        visible: type === "video" || type === "audio"
        MouseArea {
          anchors.fill: parent
          onClicked: {
            playing ^= 1;
            timer.cnt = 0;
          }
        }
      }
      Kirigami.Icon {
        id: next_button
        width: height
        height: parent.height
        source: "media-skip-forward"
        isMask: true
        color: button_color
        MouseArea {
          anchors.fill: parent
          onClicked: {
            timer.cnt = 0;
            invoke_next();
          }
        }
      }
      Item {
        id: audio_track_select
        anchors.margins: 10
        width: height
        height: parent.height
        visible: page.width > extended_view_threshold
                 && (type === "video" || type === "audio")
                 && player.item && player.item.audio_track_count > 1
        Kirigami.Icon {
          id: audio_track_icon
          anchors.fill: parent
          source: "media-album-track"
          isMask: true
          color: button_color
          MouseArea {
            anchors.fill: parent
            onPressed: {
              audio_track_list.item.shown ^= 1;
              subtitle_track_list.item.shown = false;
            }
          }
        }
        Loader {
          property int selected_track: 0
          id: audio_track_list
          sourceComponent: slide_out_menu
          anchors.horizontalCenter: audio_track_icon.horizontalCenter
          anchors.bottom: audio_track_icon.top
          onLoaded: {
            item.model = Qt.binding(function() { return player.item ? player.item.audio_tracks : []; });
            selected_track = Qt.binding(function() { return item.currentSelection; });
          }
          onSelected_trackChanged: {
            if (selected_track >= 0) {
              player.item.set_audio_track(selected_track);
              audio_track_list.item.shown = false;
            }
          }
        }
      }
      Kirigami.Icon {
        width: height
        height: parent.height
        function volume_icon(value) {
          if (value === 0.0)
            return "audio-volume-muted";
          else if (value < 1.0/3)
            return "audio-volume-low";
          else if (value < 2.0/3)
            return "audio-volume-medium";
          else
            return "audio-volume-high";
        }

        id: volume_control
        source: volume_icon(volume_slider.value)
        visible: (type === "video" || type === "audio") && page.width > extended_view_threshold
        color: button_color
        isMask: true
        MouseArea {
          id: volume_control_mouse_area
          anchors.fill: parent
          hoverEnabled: true
          onContainsMouseChanged: {
            volume_slider.timestamp = Date.now();
            volume_slider.recently_hovered = true;
          }
          onPressed: {
            timer.cnt = 0;
            if (volume_slider.visible) {
              if (volume_slider.value === 0)
                volume_slider.value = last_volume;
              else {
                last_volume = volume_slider.value;
                volume_slider.value = 0;
              }
              volume_slider.onMoved();
            }
          }
        }
      }
      Controls.Slider {
        property bool should_show: volume_control_mouse_area.containsMouse ||
                                   volume_slider.hovered ||
                                   volume_slider.pressed ||
                                   (mobile && recently_hovered)
        property real timestamp: 0
        property bool recently_hovered: false

        id: volume_slider
        anchors.verticalCenter: parent.verticalCenter
        visible: width !== 0
        hoverEnabled: !mobile
        onHoveredChanged: {
          volume_slider.timestamp = Date.now();
          volume_slider.recently_hovered = true;
        }
        onActiveFocusChanged: if (activeFocus) page.forceActiveFocus();
        background: Rectangle {
          x: volume_slider.leftPadding
          y: volume_slider.topPadding + volume_slider.availableHeight / 2 - height / 2
          implicitWidth: 200
          implicitHeight: 4
          width: volume_slider.availableWidth
          height: implicitHeight
          radius: 2
          color: Kirigami.Theme.backgroundColor

          Rectangle {
            width: volume_slider.visualPosition * parent.width
            height: parent.height
            color: Kirigami.Theme.highlightColor
            radius: 2
          }
        }
        Timer {
          running: true
          repeat: true
          interval: 500
          onTriggered: {
            volume_slider.recently_hovered = Date.now() - volume_slider.timestamp < 2000
          }
        }
        onMoved: {
          player.item.set_volume(value);
          volume_slider.timestamp = Date.now();
          volume_slider.recently_hovered = true;
          timer.cnt = 0;
        }
        state: should_show ? "volume_slider_visible" : "volume_slider_invisible"
        states: [
          State {
            name: "volume_slider_visible"
            PropertyChanges {
              target: volume_slider
              width: 100
            }
          },
          State {
            name: "volume_slider_invisible"
            PropertyChanges {
              target: volume_slider
              width: 0
            }
          }
        ]
        transitions: [
          Transition {
            from: "volume_slider_invisible"
            to: "volume_slider_visible"
            PropertyAnimation {
              properties: "width"
              duration: 200
            }
          },
          Transition {
            from: "volume_slider_visible"
            to: "volume_slider_invisible"
            PropertyAnimation {
              properties: "width"
              duration: 200
            }
          }
        ]
      }
      Item {
        id: current_time
        width: 1.5 * height
        height: parent.height
        visible: progress.visible
        Text {
          anchors.centerIn: parent
          color: "white"
          text: player.item ? print_timestamp(player.item.time) : ""
        }
      }
      Item {
        height: parent.height
        width: parent.width - fullscreen.width - play_button.width * play_button.visible -
               volume_control.width * volume_control.visible -
               volume_slider.width * volume_slider.visible -
               current_time.width * current_time.visible - total_time.width * total_time.visible -
               autoplay_icon.width - next_button.width - subtitle_track_select.width * subtitle_track_select.visible -
               audio_track_select.width * audio_track_select.visible
        Controls.Slider {
          id: progress
          anchors.verticalCenter: parent.verticalCenter
          height: parent.height * 0.5
          width: parent.width
          value: player.item ? player.item.position : 0
          visible: player.item && player.item.duration > 0 && page.width > 600
          onMoved: {
            player.item.set_position(value);
            timer.cnt = 0;
          }
          onActiveFocusChanged: if (activeFocus) page.forceActiveFocus();
          background: Rectangle {
            x: progress.leftPadding
            y: progress.topPadding + progress.availableHeight / 2 - height / 2
            implicitWidth: 200
            implicitHeight: 4
            width: progress.availableWidth
            height: implicitHeight
            radius: 2
            color: Kirigami.Theme.backgroundColor

            Rectangle {
              width: progress.visualPosition * parent.width
              height: parent.height
              color: Kirigami.Theme.highlightColor
              radius: 2
            }
          }
        }
        Text {
          visible: player.item && player.item.duration > 0 && !progress.visible
          anchors.centerIn: parent
          color: "white"
          text: player.item ? print_timestamp(player.item.time) + " / " +
                              print_timestamp(player.item.duration) : ""
        }
      }
      Item {
        id: total_time
        width: 1.5 * height
        height: parent.height
        visible: current_time.visible
        Text {
          anchors.centerIn: parent
          color: "white"
          text: player.item ? print_timestamp(player.item.duration) : ""
        }
      }
      Item {
        id: subtitle_track_select
        visible: page.width > extended_view_threshold
                 && (type === "video" || type === "audio")
                 && player.item && player.item.subtitle_track_count > 0
        width: height
        height: parent.height
        anchors.margins: 10
        Kirigami.Icon {
          id: subtitle_track_icon
          source: "media-view-subtitles-symbolic"
          color: button_color
          anchors.fill: parent
          MouseArea {
            anchors.fill: parent
            onPressed: {
              subtitle_track_list.item.shown ^= 1;
              audio_track_list.item.shown = false;
            }
          }
        }
        Loader {
          property int selected_track: 0

          id: subtitle_track_list
          sourceComponent: slide_out_menu
          anchors.horizontalCenter: subtitle_track_icon.horizontalCenter
          anchors.bottom: subtitle_track_icon.top
          onLoaded: {
            item.model = Qt.binding(function() { return player.item ? player.item.subtitle_tracks : []; });
            selected_track = Qt.binding(function() { return item.currentSelection; });
          }
          onSelected_trackChanged: {
            if (selected_track >= 0) {
              player.item.set_subtitle_track(selected_track);
              subtitle_track_list.item.shown = false;
            }
          }
        }
      }
      Kirigami.Icon {
        id: autoplay_icon
        anchors.margins: 10
        isMask: true
        width: height
        height: parent.height
        source: type === "audio" || type === "video" ? "media-playlist-shuffle" : ""
        color: autoplay ? button_color : tint_color

        MouseArea {
          anchors.fill: parent
          enabled: autoplay_icon.source !== ""
          onClicked: {
            autoplay ^= 1;
            timer.cnt = 0;
          }
        }
      }
      Kirigami.Icon {
        id: fullscreen
        anchors.margins: 10
        width: height
        height: parent.height
        color: button_color
        isMask: true
        source: "view-fullscreen"

        MouseArea {
          anchors.fill: parent
          onClicked: {
            page.fullscreen ^= 1;
            timer.cnt = 0;
          }
        }
      }
    }
  }

  Controls.BusyIndicator {
    id: indicator
    anchors.centerIn: parent
    enabled: false
    running: player.item ? player.item.buffering : true
    width: 200
    height: 200
  }

  Component {
    id: slide_out_menu
    Item {
      property bool shown: false
      property alias model: list.model
      property alias currentSelection: list.currentIndex
      width: childrenRect.width
      height: list.height
      clip: true
      Item {
        id: slide_out
        width: childrenRect.width
        height: list.height
        state: (shown && page.state == "overlay_visible") ? "list_visible" : "list_invisible"
        states: [
          State {
            name: "list_visible"
            PropertyChanges {
              target: slide_out
              y: -height + parent.height
            }
          },
          State {
            name: "list_invisible"
            PropertyChanges {
              target: slide_out
              y: parent.height
            }
          }
        ]
        transitions: [
          Transition {
            from: "list_visible"
            to: "list_invisible"
            PropertyAnimation {
              properties: "y"
              duration: 200
            }
          },
          Transition {
            from: "list_invisible"
            to: "list_visible"
            PropertyAnimation {
              properties: "y"
              duration: 200
            }
          }
        ]
        ListView {
          id: list
          rotation: 180
          implicitWidth: 250
          height: 300
          interactive: false
          delegate: Kirigami.BasicListItem {
            rotation: 180
            backgroundColor: ListView.isCurrentItem ? Kirigami.Theme.highlightColor : "black"
            opacity: 0.65
            height: 40
            reserveSpaceForIcon: false
            Text {
              id: text
              opacity: 1.5
              text: modelData
              color: "white"
            }
            onPressed: {
              list.currentIndex = index;
            }
          }
        }
      }
    }
  }

  onShowCursor: cursor_area.cursorShape = Qt.ArrowCursor
  onHideCursor: cursor_area.cursorShape = Qt.BlankCursor

  MouseArea {
    id: cursor_area
    anchors.fill: parent
    enabled: false
  }

  Keys.onPressed: {
    if (event.key === Qt.Key_N) {
      event.accepted = true;
      page.state = "overlay_visible";
      timer.cnt = 0;
      invoke_next();
    } else if (event.key === Qt.Key_Space || event.key === Qt.Key_K) {
      event.accepted = true;
      page.state = "overlay_visible";
      timer.cnt = 0;
      playing ^= 1;
    } else if (event.key === Qt.Key_M) {
      event.accepted = true;
      page.state = "overlay_visible";
      timer.cnt = 0;
      if (volume_slider.value === 0)
        volume_slider.value = last_volume;
      else {
        last_volume = volume_slider.value;
        volume_slider.value = 0;
      }
      volume_slider.onMoved();
    } else if (event.key === Qt.Key_F) {
      event.accepted = true;
      page.state = "overlay_visible";
      timer.cnt = 0;
      page.fullscreen ^= 1;
    } else if (event.key === Qt.Key_Left) {
      event.accepted = true;
      page.state = "overlay_visible";
      timer.cnt = 0;
      player.item.position = Math.max(((player.item.position * player.item.duration) - 5000) / player.item.duration, 0);
    } else if (event.key === Qt.Key_Right) {
      event.accepted = true;
      page.state = "overlay_visible";
      timer.cnt = 0;
      player.item.position = Math.min(((player.item.position * player.item.duration) + 5000) / player.item.duration, 1);
    } else if (event.key === Qt.Key_J) {
      event.accepted = true;
      page.state = "overlay_visible";
      timer.cnt = 0;
      player.item.position = Math.max(((player.item.position * player.item.duration) - 10000) / player.item.duration, 0);
    } else if (event.key === Qt.Key_L) {
      event.accepted = true;
      page.state = "overlay_visible";
      timer.cnt = 0;
      player.item.position = Math.min(((player.item.position * player.item.duration) + 10000) / player.item.duration, 1);
    } else if (event.key === Qt.Key_A) {
      event.accepted = true;
      page.state = "overlay_visible";
      timer.cnt = 0;
      page.autoplay ^= 1;
    }
  }
}
