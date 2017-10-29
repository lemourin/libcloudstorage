import libcloudstorage 1.0

AndroidFileDialog {
  property string url: fileUrl
  
  signal ready()

  onAccepted: ready()
}
