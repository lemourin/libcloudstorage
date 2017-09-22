import QtQuick 2.7
import QtQuick.Controls 2.0 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.0 as Kirigami
import libcloudstorage 1.0

Kirigami.ApplicationWindow {
  id: root
  header: Kirigami.ApplicationHeader {}

  CloudContext {
    property var list_request
    property var currently_moved
    property variant request: []

    id: cloud
    onUserProvidersChanged: {
      root.globalDrawer.actions = root.actions();
    }

    function list(title, item) {
      pageStack.push(listDirectoryPage, {title: title, item: item});
    }
  }

  function actions() {
    var ret = [settings.createObject(root.globalDrawer)], i;
    for (i = 0; i < cloud.userProviders.length; i++) {
      var props = {
        provider: cloud.userProviders[i],
        iconName: "qrc:/resources/providers/" + cloud.userProviders[i].type + ".png"
      };
      ret.push(providerAction.createObject(root.globalDrawer, props));
    }
    return ret;
  }

  globalDrawer: Kirigami.GlobalDrawer {
    Component {
      id: settings
      Kirigami.Action {
        text: "Settings"
        Kirigami.Action {
          text: "Add Cloud Provider"
          onTriggered: {
            pageStack.clear();
            pageStack.push(addProviderPage)
          }
        }
        Kirigami.Action {
          text: "Remove Cloud Provider"
          onTriggered: {
            pageStack.clear();
            pageStack.push(removeProviderPage);
          }
        }
      }
    }
    Component {
      id: providerAction
      Kirigami.Action {
        property variant provider
        text: provider.label
        onTriggered: {
          pageStack.clear();
          cloud.currently_moved = null;
          cloud.list(provider.label, cloud.root(provider));
        }
      }
    }

    title: "Cloud Browser"
    titleIcon: "qrc:/resources/cloud.png"
    actions: root.actions()
  }
  contextDrawer: Kirigami.ContextDrawer {
    id: contextDrawer
  }
  pageStack.initialPage: mainPageComponent

  Component {
    id: addProviderPage
    RegisterPage {
    }
  }

  Component {
    id: removeProviderPage
    RemoveProvider {
    }
  }

  Component {
    id: listDirectoryPage
    ItemPage {
    }
  }

  Component {
    id: mainPageComponent
    Kirigami.ScrollablePage {
      anchors.fill: parent
      title: "Select Cloud Provider"
    }
  }
}
