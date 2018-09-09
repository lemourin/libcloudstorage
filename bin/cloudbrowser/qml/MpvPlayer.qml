import QtQuick 2.0
import libcloudstorage 1.0

MpvPlayer {
  property alias source: player.uri
  property int time: player.position * player.duration
  property bool buffering: !player.playing && !player.paused

  id: player
  anchors.fill: parent

  signal error(int error, string errorString)

  function set_volume(v) {
    player.volume = v * 100;
  }

  function set_position(p) {
    player.position = p;
  }
}
