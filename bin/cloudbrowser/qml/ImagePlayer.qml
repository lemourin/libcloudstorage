import QtQuick 2.0

AnimatedImage {
  property real position
  property bool ended
  property bool buffering: status !== AnimatedImage.Ready
  property int duration: 0

  function play() {}
  function pause() {}

  anchors.fill: parent
  fillMode: AnimatedImage.PreserveAspectFit
  playing: status == AnimatedImage.Ready
}
