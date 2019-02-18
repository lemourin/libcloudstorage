import QtQuick 2.0

AnimatedImage {
  property real position
  property bool ended
  property bool buffering: status !== AnimatedImage.Ready
  property int duration: 0

  signal error(int error, string errorString)

  function play() {}
  function pause() {}
  function set_volume(volume) {}

  anchors.fill: parent
  fillMode: AnimatedImage.PreserveAspectFit
  playing: status == AnimatedImage.Ready
  onStatusChanged: {
    if (status === AnimatedImage.Error)
      error(500, "Error occurred")
  }
}
