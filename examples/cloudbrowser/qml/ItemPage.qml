import QtQuick 2.7
import org.kde.kirigami 2.1 as Kirigami
import QtQuick.Controls 2.0 as Controls
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.2
import libcloudstorage 1.0

Kirigami.ScrollablePage {
  property CloudItem item

  id: page
  anchors.fill: parent

  Component {
    id: media_player
    MediaPlayer {
    }
  }

  Component {
    id: upload_component
    UploadItemRequest {
      property bool upload: true

      id: upload_request
      context: cloud
      onUploadComplete: {
        var lst = [], i;
        for (i = 0; i < cloud.request.length; i++)
          if (cloud.request[i] !== upload_request)
            lst.push(cloud.request[i]);
        cloud.request = lst;
        list.update();
      }
    }
  }

  Component {
    id: download_component
    DownloadItemRequest {
      property bool upload: false

      id: download_request
      context: cloud
      onDownloadComplete: {
        var lst = [], i;
        for (i = 0; i < cloud.request.length; i++)
          if (cloud.request[i] !== download_request)
            lst.push(cloud.request[i]);
        cloud.request = lst;
      }
    }
  }

  contextualActions: [
    Kirigami.Action {
      visible: list_view.currentItem && (list_view.currentItem.item.type === "audio" ||
                                         list_view.currentItem.item.type === "video" ||
                                         list_view.currentItem.item.type === "image")
      text: "Play"
      onTriggered: {
        contextDrawer.drawerOpen = false;
        pageStack.push(media_player, {item: list_view.currentItem.item});
      }
    },
    Kirigami.Action {
      visible: list_view.currentItem
      text: "Rename"
      onTriggered: {
        list_view.currentEdit = list_view.currentIndex;
      }
    },
    Kirigami.Action {
      iconName: "folder"
      text: "Create Directory"
      onTriggered: {
        list_view.currentIndex = -1;
        create_directory_sheet.open();
      }
    },
    Kirigami.Action {
      visible: list_view.currentItem
      text: "Delete"
      onTriggered: {
        list.done = false;
        delete_request.item = list_view.currentItem.item;
      }
    },
    Kirigami.Action {
      visible: list_view.currentItem && !cloud.currently_moved
      text: "Move"
      onTriggered: {
        cloud.list_request = list;
        cloud.currently_moved = list_view.currentItem.item;
      }
    },
    Kirigami.Action {
      visible: cloud.currently_moved
      text: "Move " + (cloud.currently_moved ? cloud.currently_moved.filename : "") + " here"
      onTriggered: {
        move_request.source = cloud.currently_moved;
        move_request.destination = page.item;
        list.done = false;
        if (cloud.list_request)
          cloud.list_request.done = false;
        cloud.currently_moved = null;
      }
    },
    Kirigami.Action {
      visible: cloud.currently_moved
      text: "Cancel move"
      onTriggered: {
        cloud.currently_moved = null;
        cloud.list_request = null;
      }
    },
    Kirigami.Action {
      text: "Upload File"
      iconName: "go-up"
      onTriggered: upload_dialog.open()
    },
    Kirigami.Action {
      visible: list_view.currentItem && list_view.currentItem.item.type !== "directory"
      iconName: "go-down"
      text: "Download " + (list_view.currentItem ? list_view.currentItem.item.filename : "")
      onTriggered: download_dialog.open();
    }
  ]

  Kirigami.OverlaySheet {
    id: create_directory_sheet
    ColumnLayout {
      anchors.centerIn: parent
      Controls.TextField {
        id: directory_name
        placeholderText: "New Directory"
      }
      Controls.Button {
        CreateDirectoryRequest {
          id: create_directory
          context: cloud
          parent: item
          onCreatedDirectory: list.update()
        }

        anchors.horizontalCenter: parent.horizontalCenter
        text: "Create"
        onClicked: {
          create_directory.name = directory_name.text;
          list.done = false;
          create_directory_sheet.close();
        }
      }
    }
  }

  Controls.BusyIndicator {
    anchors.centerIn: parent
    running: !list.done
    width: 200
    height: 200
  }

  ListView {
    property int currentEdit: -1

    ListDirectoryRequest {
      id: list
      context: cloud
      item: page.item
    }

    DeleteItemRequest {
      id: delete_request
      context: cloud
      onItemDeleted: list.update()
    }

    RenameItemRequest {
      id: rename_request
      context: cloud
      onItemRenamed: list.update()
    }

    MoveItemRequest {
      id: move_request
      context: cloud
      onItemMoved: {
        if (cloud.list_request) {
          cloud.list_request.update();
          cloud.list_request = null;
        }
        list.update();
      }
    }

    FileDialog {
      id: upload_dialog
      selectExisting: true
      selectFolder: false
      onAccepted: {
        var r = upload_component.createObject(cloud, {parent: page.item, path: fileUrl});
        var req = cloud.request;
        req.push(r);
        cloud.request = req;
      }
    }

    FileDialog {
      id: download_dialog
      selectFolder: true
      onAccepted: {
        var r = download_component.createObject(cloud, {
                                                  item: list_view.currentItem.item,
                                                  path: fileUrl + "/" + list_view.currentItem.item.filename
                                                });
        var req = cloud.request;
        req.push(r);
        cloud.request = req;
      }
    }

    id: list_view
    model: list.list
    onCurrentIndexChanged: currentEdit = -1
    footerPositioning: ListView.OverlayFooter
    footer: Kirigami.ItemViewHeader {
      title: ""
      visible: cloud.request.length > 0
      height: 150
      ListView {
        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        model: cloud.request
        delegate: RowLayout {
          width: parent.width
          height: 40
          Item {
            width: 300
            height: parent.height
            clip: true
            Kirigami.Label {
              anchors.fill: parent
              anchors.margins: 10
              text: modelData.filename
            }
          }
          Kirigami.Icon {
            anchors.right: progress.left
            anchors.margins: 10
            width: 20
            height: 20
            source: modelData.upload ? "go-up" : "go-down"
          }
          Controls.ProgressBar {
            id: progress
            from: 0
            to: modelData.total
            value: modelData.now
          }
        }
      }
    }
    delegate: Kirigami.BasicListItem {
      property var item: modelData
      property alias text_control: text

      id: item
      highlighted: focus && ListView.isCurrentItem
      onClicked: {
        list_view.currentIndex = index;
        if (modelData.type === "directory")
          cloud.list(modelData.filename, modelData);
      }
      Row {
        anchors.left: parent.left
        GetThumbnailRequest {
          id: thumbnail
          context: cloud
          item: modelData.type !== "directory" ? modelData : null
        }
        Item {
          id: image
          width: 50
          height: 50
          Controls.BusyIndicator {
            anchors.fill: parent
            running: thumbnail.item && !thumbnail.done
          }
          Image {
            anchors.fill: parent
            anchors.margins: 5
            source: modelData.type === "directory" ?
                      "qrc:/resources/" + modelData.type + ".png" :
                      (!thumbnail.done ? "" :
                                         (thumbnail.source ?
                                            thumbnail.source :
                                            "qrc:/resources/" + modelData.type + ".png"))
          }
          MouseArea {
            anchors.fill: parent
            onClicked: list_view.currentIndex = index;
          }
        }
        Kirigami.Label {
          anchors.verticalCenter: parent.verticalCenter
          text: modelData.filename
          visible: list_view.currentEdit !== index
        }
        RowLayout {
          visible: list_view.currentEdit === index
          onVisibleChanged: if (visible) text.text = modelData.filename
          width: parent.width - image.width
          Controls.TextField {
            id: text
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 10
            anchors.left: parent.left
            anchors.right: rename_button.left
            Layout.alignment: Qt.AlignLeft
          }
          Controls.Button {
            id: rename_button
            anchors.margins: 10
            Layout.alignment: Qt.AlignRight
            anchors.right: parent.right
            text: "Rename"
            onClicked: {
              list_view.currentEdit = -1;
              list.done = false;
              rename_request.item = modelData;
              rename_request.name = text.text;
            }
          }
        }
      }
    }
  }
}
