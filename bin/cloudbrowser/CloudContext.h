#ifndef CLOUD_CONTEXT_H
#define CLOUD_CONTEXT_H

#include <QAbstractListModel>
#include <QMap>
#include <QObject>
#include <QQmlListProperty>
#include <QVariantList>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <streambuf>
#include "ICloudProvider.h"
#include "IHttpServer.h"

class CloudContext;

class RequestNotifier : public QObject {
 signals:
  void addedItem(cloudstorage::IItem::Pointer);
  void finishedList(
      cloudstorage::EitherError<std::vector<cloudstorage::IItem::Pointer>>);
  void finishedVoid(cloudstorage::EitherError<void>);
  void finishedString(cloudstorage::EitherError<std::string>);
  void finishedItem(cloudstorage::EitherError<cloudstorage::IItem>);
  void progressChanged(qint64 total, qint64 now);

 private:
  Q_OBJECT
};

class CloudItem : public QObject {
 public:
  Q_PROPERTY(QString filename READ filename CONSTANT)
  Q_PROPERTY(uint64_t size READ size CONSTANT)
  Q_PROPERTY(QString type READ type CONSTANT)

  CloudItem(std::shared_ptr<cloudstorage::ICloudProvider>,
            cloudstorage::IItem::Pointer, QObject* parent = nullptr);

  cloudstorage::IItem::Pointer item() const { return item_; }
  std::shared_ptr<cloudstorage::ICloudProvider> provider() const {
    return provider_;
  }
  QString filename() const;
  uint64_t size() const { return item_->size(); }
  QString type() const;

 private:
  std::shared_ptr<cloudstorage::ICloudProvider> provider_;
  cloudstorage::IItem::Pointer item_;
  Q_OBJECT
};

class Request : public QObject {
 public:
  Q_PROPERTY(bool done READ done WRITE set_done NOTIFY doneChanged)

  bool done() const { return done_; }
  void set_done(bool);

 signals:
  void doneChanged();

 private:
  bool done_;

  Q_OBJECT
};

class ListDirectoryModel : public QAbstractListModel {
 public:
  void set_provider(std::shared_ptr<cloudstorage::ICloudProvider>);
  void add(cloudstorage::IItem::Pointer);
  void clear();

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int) const override;
  QHash<int, QByteArray> roleNames() const override;

 private:
  std::shared_ptr<cloudstorage::ICloudProvider> provider_;
  std::vector<cloudstorage::IItem::Pointer> list_;
  Q_OBJECT
};

class ListDirectoryRequest : public Request {
 public:
  Q_PROPERTY(ListDirectoryModel* list READ list CONSTANT)

  ListDirectoryModel* list();

  Q_INVOKABLE void update(CloudContext*, CloudItem*);

 signals:
  void itemChanged();

 private:
  bool first_listed_ = false;
  ListDirectoryModel list_;

  Q_OBJECT
};

class GetThumbnailRequest : public Request {
 public:
  Q_PROPERTY(QString source READ source NOTIFY sourceChanged)

  QString source() const { return source_; }
  Q_INVOKABLE void update(CloudContext* context, CloudItem* item);

 signals:
  void sourceChanged();

 private:
  QString source_;

  Q_OBJECT
};

class GetUrlRequest : public Request {
 public:
  Q_PROPERTY(QString source READ source NOTIFY sourceChanged)

  QString source() const { return source_; }
  Q_INVOKABLE void update(CloudContext*, CloudItem*);

 signals:
  void sourceChanged();

 private:
  QString source_;

  Q_OBJECT
};

class CreateDirectoryRequest : public Request {
 public:
  Q_INVOKABLE void update(CloudContext*, CloudItem* parent, QString name);

 signals:
  void createdDirectory();

 private:
  Q_OBJECT
};

class DeleteItemRequest : public Request {
 public:
  Q_INVOKABLE void update(CloudContext*, CloudItem*);

 signals:
  void itemDeleted();

 private:
  Q_OBJECT
};

class RenameItemRequest : public Request {
 public:
  Q_INVOKABLE void update(CloudContext*, CloudItem* item, QString name);

 signals:
  void itemRenamed();

 private:
  Q_OBJECT
};

class MoveItemRequest : public Request {
 public:
  Q_INVOKABLE void update(CloudContext*, CloudItem* source,
                          CloudItem* destination);

 signals:
  void itemMoved();

 private:
  Q_OBJECT
};

class UploadItemRequest : public Request {
 public:
  Q_PROPERTY(qint64 total READ total NOTIFY progressChanged)
  Q_PROPERTY(qint64 now READ now NOTIFY progressChanged)

  qint64 total() const { return total_; }
  qint64 now() const { return now_; }

  Q_INVOKABLE void update(CloudContext* context, CloudItem* parent,
                          QString path, QString filename);

 signals:
  void progressChanged();
  void uploadComplete();

 private:
  qint64 total_ = 0;
  qint64 now_ = 0;

  Q_OBJECT
};

class DownloadItemRequest : public Request {
 public:
  Q_PROPERTY(qint64 total READ total NOTIFY progressChanged)
  Q_PROPERTY(qint64 now READ now NOTIFY progressChanged)

  qint64 total() const { return total_; }
  qint64 now() const { return now_; }

  Q_INVOKABLE void update(CloudContext*, CloudItem* item, QString path);

 signals:
  void progressChanged();
  void downloadComplete();

 private:
  qint64 total_ = 0;
  qint64 now_ = 0;

  Q_OBJECT
};

class DispatchCallback : public cloudstorage::IHttpServer::ICallback {
 public:
  cloudstorage::IHttpServer::IResponse::Pointer handle(
      const cloudstorage::IHttpServer::IRequest&) override;

  void add(const std::string&, ICallback::Pointer);
  void remove(const std::string&);
  ICallback::Pointer callback(const std::string&) const;

 private:
  mutable std::mutex lock_;
  std::unordered_map<std::string, cloudstorage::IHttpServer::ICallback::Pointer>
      client_callback_;
};

class HttpServerWrapper : public cloudstorage::IHttpServer {
 public:
  HttpServerWrapper(std::shared_ptr<DispatchCallback>,
                    cloudstorage::IHttpServer::ICallback::Pointer callback,
                    const std::string& session);
  ~HttpServerWrapper();

  ICallback::Pointer callback() const override;

 private:
  std::shared_ptr<DispatchCallback> dispatch_;
  std::string session_;
};

class ServerWrapperFactory : public cloudstorage::IHttpServerFactory {
 public:
  ServerWrapperFactory(std::shared_ptr<cloudstorage::IHttpServerFactory>);
  cloudstorage::IHttpServer::Pointer create(
      cloudstorage::IHttpServer::ICallback::Pointer,
      const std::string& session_id, cloudstorage::IHttpServer::Type) override;

 private:
  std::shared_ptr<DispatchCallback> callback_;
  std::shared_ptr<cloudstorage::IHttpServer> http_server_;
};

class CloudContext : public QObject {
 public:
  Q_PROPERTY(QStringList providers READ providers CONSTANT)
  Q_PROPERTY(
      QVariantList userProviders READ userProviders NOTIFY userProvidersChanged)

  CloudContext(QObject* parent = nullptr);
  ~CloudContext();

  void save();

  QStringList providers() const;
  QVariantList userProviders() const;

  Q_INVOKABLE QString authorizationUrl(QString provider) const;
  Q_INVOKABLE QObject* root(QVariant provider);
  Q_INVOKABLE void removeProvider(QVariant provider);
  Q_INVOKABLE QString pretty(QString provider) const;
  Q_INVOKABLE QVariantMap readUrl(QString url) const;

  void add(std::shared_ptr<cloudstorage::ICloudProvider> p,
           std::shared_ptr<cloudstorage::IGenericRequest> r);

 signals:
  void userProvidersChanged();
  void receivedCode(QString provider);
  void errorOccurred(QString operation, int code, QString description);

 private:
  struct Provider {
    std::string label_;
    std::shared_ptr<cloudstorage::ICloudProvider> provider_;
  };

  class RequestPool {
   public:
    RequestPool();
    ~RequestPool();

    void add(std::shared_ptr<cloudstorage::ICloudProvider> p,
             std::shared_ptr<cloudstorage::IGenericRequest> r);

   private:
    struct RequestData {
      std::shared_ptr<cloudstorage::ICloudProvider> provider_;
      std::shared_ptr<cloudstorage::IGenericRequest> request_;
    };
    std::atomic_bool done_;
    std::shared_ptr<cloudstorage::IGenericRequest> current_request_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<RequestData> request_;
    std::future<void> cleanup_thread_;
  };

  class HttpServerCallback : public cloudstorage::IHttpServer::ICallback {
   public:
    HttpServerCallback(CloudContext*);
    cloudstorage::IHttpServer::IResponse::Pointer handle(
        const cloudstorage::IHttpServer::IRequest&) override;

   private:
    CloudContext* ctx_;
  };

  class DebugStream : public std::streambuf {
   public:
    std::streambuf::int_type overflow(int_type ch) override;

   private:
    QString current_line_;
  };

  void receivedCode(std::string provider, std::string code);
  cloudstorage::ICloudProvider::Pointer provider(
      const std::string& name, const std::string& label,
      cloudstorage::Token token) const;
  cloudstorage::ICloudProvider* provider(const std::string& label) const;

  mutable std::mutex mutex_;
  DebugStream debug_stream_;
  std::shared_ptr<cloudstorage::IHttpServerFactory> http_server_factory_;
  std::vector<std::shared_ptr<cloudstorage::IHttpServer>> auth_server_;
  std::shared_ptr<cloudstorage::IHttp> http_;
  std::shared_ptr<cloudstorage::IThreadPool> thread_pool_;
  RequestPool pool_;
  std::vector<Provider> provider_;

  Q_OBJECT
};

Q_DECLARE_METATYPE(CloudItem*)
Q_DECLARE_METATYPE(cloudstorage::IItem::Pointer)
Q_DECLARE_METATYPE(
    cloudstorage::EitherError<std::vector<cloudstorage::IItem::Pointer>>)
Q_DECLARE_METATYPE(cloudstorage::EitherError<void>)
Q_DECLARE_METATYPE(cloudstorage::EitherError<std::string>)
Q_DECLARE_METATYPE(cloudstorage::EitherError<cloudstorage::IItem>)

#endif  // CLOUD_CONTEXT
