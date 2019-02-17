#ifndef CLOUD_CONTEXT_H
#define CLOUD_CONTEXT_H

#include "CloudItem.h"
#include "Exec.h"
#include "HttpServer.h"
#include "ICloudProvider.h"
#include "Request/CloudRequest.h"
#include "Request/ListDirectory.h"

#include <QAbstractListModel>
#include <QJsonDocument>
#include <QMap>
#include <QObject>
#include <QQmlListProperty>
#include <QVariantList>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <streambuf>
#include <unordered_set>

class CloudContext;

extern CloudContext* gCloudContext;
extern std::mutex gMutex;

class CLOUDBROWSER_API ProviderListModel : public QAbstractListModel {
 public:
  int rowCount(const QModelIndex& = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  void add(const Provider&);
  void remove(QVariant provider);
  Provider provider(QVariant provider) const;
  Provider provider(int index) const;

  QVariantList dump() const;
  Q_INVOKABLE QVariantList variant() const;

 signals:
  void updated();

 private:
  std::vector<Provider> provider_;
  Q_OBJECT
};

class CLOUDBROWSER_API CloudContext : public QObject {
 public:
  Q_PROPERTY(QStringList providers READ providers CONSTANT)
  Q_PROPERTY(ProviderListModel* userProviders READ userProviders CONSTANT)
  Q_PROPERTY(bool includeAds READ includeAds CONSTANT)
  Q_PROPERTY(bool isFree READ isFree CONSTANT)
  Q_PROPERTY(qint64 cacheSize READ cacheSize NOTIFY cacheSizeChanged)
  Q_PROPERTY(QString playerBackend READ playerBackend WRITE setPlayerBackend
                 NOTIFY playerBackendChanged)
  Q_PROPERTY(bool httpServerAvailable READ httpServerAvailable CONSTANT)

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

  CloudContext(QObject* parent = nullptr);
  ~CloudContext();

  static QString sanitize(const QString& filename);

  QStringList providers() const;
  ProviderListModel* userProviders();
  bool includeAds() const;
  bool isFree() const;
  bool httpServerAvailable() const;
  QString playerBackend() const;

  void setPlayerBackend(QString);

  Q_INVOKABLE QString authorizationUrl(QString provider) const;
  Q_INVOKABLE QObject* root(QVariant provider);
  Q_INVOKABLE void removeProvider(QVariant label);
  Q_INVOKABLE QString pretty(QString provider) const;
  Q_INVOKABLE QVariantMap readUrl(QString url) const;
  Q_INVOKABLE QString home() const;
  Q_INVOKABLE void showCursor() const;
  Q_INVOKABLE void hideCursor() const;
  Q_INVOKABLE QString supportUrl(QString name) const;
  qint64 cacheSize() const;
  Q_INVOKABLE void clearCache();

  void addCacheSize(size_t);
  qint64 updatedCacheSize() const;

  void add(std::shared_ptr<cloudstorage::ICloudProvider> p,
           std::shared_ptr<cloudstorage::IGenericRequest> r);
  void add(const std::string& provider_name, const std::string& provider_label,
           const cloudstorage::Token& token);

  void cacheDirectory(ListDirectoryCacheKey directory,
                      const std::vector<cloudstorage::IItem::Pointer>&);
  cloudstorage::IItem::List cachedDirectory(ListDirectoryCacheKey);
  void schedule(std::function<void()>);
  std::shared_ptr<cloudstorage::IThreadPool> thumbnailer_thread_pool() const;
  std::shared_ptr<std::atomic_bool> interrupt() const;
  std::shared_ptr<RequestPool> request_pool() const;

 signals:
  void receivedCode(QString provider);
  void errorOccurred(QString operation, QVariantMap provider, int code,
                     QString description);
  void cacheSizeChanged();
  void playerBackendChanged();

 private:
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

  void loadCachedDirectories();
  void saveCachedDirectories();
  void saveProviders();
  void receivedCode(std::string provider, std::string code);
  cloudstorage::ICloudProvider::Pointer provider(
      const std::string& name, const cloudstorage::Token& token) const;
  cloudstorage::ICloudProvider::InitData init_data(
      const std::string& name) const;

  mutable std::mutex mutex_;
  QJsonDocument config_;
  DebugStream debug_stream_;
  std::shared_ptr<ServerWrapperFactory> http_server_factory_;
  std::vector<std::shared_ptr<cloudstorage::IHttpServer>> auth_server_;
  std::shared_ptr<cloudstorage::IHttp> http_;
  std::shared_ptr<cloudstorage::IThreadPool> thread_pool_;
  cloudstorage::IThreadPool::Pointer context_thread_pool_;
  std::shared_ptr<cloudstorage::IThreadPool> thumbnailer_thread_pool_;
  std::shared_ptr<RequestPool> pool_;
  std::unordered_map<ListDirectoryCacheKey,
                     std::vector<cloudstorage::IItem::Pointer>>
      list_directory_cache_;
  qint64 cache_size_;
  ProviderListModel user_provider_model_;
  std::shared_ptr<std::atomic_bool> interrupt_;
  mutable int provider_index_;

  Q_OBJECT
};

#endif  // CLOUD_CONTEXT
