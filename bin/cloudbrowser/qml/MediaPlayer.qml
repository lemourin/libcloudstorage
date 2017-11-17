import QtQuick 2.7
import QtQuick.Controls 2.0 as Controls
import org.kde.kirigami 2.0 as Kirigami
import libcloudstorage 1.0

Kirigami.Page {
  property CloudItem item
  property Kirigami.Page item_page
  property bool handle_state
  property bool playing: true
  property bool autoplay: false

  id: page
  leftPadding: 0
  rightPadding: 0
  topPadding: 0
  bottomPadding: 0
  title: item.filename
  onIsCurrentPageChanged: {
    if (!isCurrentPage) {
      root.visible_player = false;
      root.globalDrawer.handleVisible = handle_state;
    } else {
      handle_state = root.globalDrawer.handleVisible;
      root.globalDrawer.handleVisible = false;
    }
  }
  onPlayingChanged: {
    if (playing)
      player.item.play();
    else
      player.item.pause();
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

  function ended() {
    if (autoplay) {
      var next = item_page.nextRequested();
      if (next)
        item = next;
      else
        root.pageStack.pop();
    } else {
      root.pageStack.pop();
    }
  }

  onItemChanged: url_request.update(cloud, item)

  GetUrlRequest {
    id: url_request
    onDoneChanged: {
      if (done && source === "")
        ended();
    }
  }

  Connections {
    id: connections
    target: null
    onEndedChanged: {
      if (target.ended) {
        ended();
      }
    }
  }

  MouseArea {
    id: mouse_area

    anchors.fill: parent
    hoverEnabled: true

    onClicked: {
      if (page.state === "overlay_visible" &&
          Date.now() - timer.last_visible >= 2 * timer.interval)
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
      source: vlcqt ? "VlcPlayer.qml" : "QtPlayer.qml"
      onStatusChanged: {
        if (status === Loader.Ready) {
          item.source = Qt.binding(function() { return url_request.source; });
          connections.target = item;
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
          if (!controls.containsMouse && page.playing)
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
        y: page.height - controls.height
      }
    },
    State {
      name: "overlay_invisible"
      PropertyChanges {
        target: controls
        y: page.height
      }
    }
  ]

  transitions: [
    Transition {
      from: "overlay_invisible"
      to: "overlay_visible"
      PropertyAnimation {
        properties: "y"
        duration: 250
      }
    },
    Transition {
      from: "overlay_visible"
      to: "overlay_invisible"
      PropertyAnimation {
        properties: "y"
        duration: 250
      }
    }
  ]

  MouseArea {
    id: controls
    hoverEnabled: true
    anchors.left: parent.left
    anchors.right: parent.right
    height: 50
    onClicked: timer.cnt = 0
    Rectangle {
      anchors.fill: parent
      color: "black"
      opacity: 0.5
    }
    Row {
      anchors.fill: parent
      Kirigami.Icon {
        id: play_button
        width: height
        height: parent.height
        source: playing ? "media-playback-pause" : "media-playback-start"
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
        MouseArea {
          anchors.fill: parent
          onClicked: {
            var next = item_page.nextRequested();
            if (next)
              item = next;
            else
              root.pageStack.pop();
            timer.cnt = 0;
          }
        }
      }
      Item {
        id: current_time
        width: 1.5 * height
        height: parent.height
        Text {
          visible: progress.visible
          anchors.centerIn: parent
          color: "white"
          text: player.item ? print_timestamp(player.item.time) : ""
        }
      }
      Item {
        height: parent.height
        width: parent.width - fullscreen.width - play_button.width - 
               current_time.width - total_time.width - autoplay_icon.width - next_button.width
        Controls.ProgressBar {
          id: progress
          anchors.verticalCenter: parent.verticalCenter
          height: parent.height * 0.5
          width: parent.width
          from: 0
          to: 1
          value: player.item ? player.item.position : 0
          visible: width > 200
        }
        MouseArea {
          anchors.fill: parent
          onClicked: {
            player.item.set_position(mouse.x / width);
            timer.cnt = 0;
          }
          enabled: progress.visible
        }
      }
      Item {
        id: total_time
        width: 1.5 * height
        height: parent.height
        Text {
          visible: progress.visible
          anchors.centerIn: parent
          color: "white"
          text: player.item ? print_timestamp(player.item.duration) : ""
        }
      }
      Kirigami.Icon {
        id: autoplay_icon
        anchors.margins: 10
        width: height
        height: parent.height
        source: "media-playlist-shuffle"
        selected: autoplay
        opacity: autoplay ? 1 : 0.5

        MouseArea {
          anchors.fill: parent
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
        source: "view-fullscreen"

        MouseArea {
          anchors.fill: parent
          onClicked: {
            root.visible_player ^= 1;
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
}
