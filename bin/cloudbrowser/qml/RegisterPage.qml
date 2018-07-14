import QtQuick 2.7
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.1 as Kirigami

Kirigami.ScrollablePage {
  id: page
  anchors.fill: parent
  title: "Add Provider"

  function open_auth(url) {
    root.globalDrawer.drawerOpen = false;
    if (qtwebview)
      root.pageStack.push(webview, {url: url});
    else
      platform.openWebPage(url);
  }

  Component {
    id: webview
    Kirigami.Page {
      property string url: ""
      id: webview_page
      title: "Authorization"
      anchors.fill: parent
      onUrlChanged: if (view.item) view.item.url = url
      Loader {
        id: view
        anchors.fill: parent
        source: "WebView.qml"
        onStatusChanged: if (status === Loader.Ready) item.url = webview_page.url;
      }
    }
  }

  Connections {
    target: cloud
    onReceivedCode: {
      platform.closeWebPage();
      if (root.pageStack.currentItem.title === "Authorization")
        root.pageStack.pop();
    }
  }

  ListView {
    id: list
    model: cloud.providers
    delegate: Kirigami.BasicListItem {
      reserveSpaceForIcon: false
      backgroundColor: ListView.isCurrentItem ? Kirigami.Theme.highlightColor :
                                                Kirigami.Theme.backgroundColor
      onClicked: {
        open_auth(cloud.authorizationUrl(modelData));
        list.currentIndex = index;
      }
      height: 60
      contentItem: ProviderEntry {
        image_width: 120
        image_height: 35
        provider: modelData
      }
    }
  }
}
