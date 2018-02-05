#ifndef CLOUD_CONTEXT_H
#define CLOUD_CONTEXT_H

#include "CloudItem.h"
#include "HttpServer.h"
#include "ICloudProvider.h"
#include "Request/ListDirectory.h"
#include "Request/CloudRequest.h"

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

class CloudContext : public QObject {
 public:
  Q_PROPERTY(QStringList providers READ providers CONSTANT)
  Q_PROPERTY(
      QVariantList userProviders READ userProviders NOTIFY userProvidersChanged)
  Q_PROPERTY(bool includeAds READ includeAds CONSTANT)
  Q_PROPERTY(bool isFree READ isFree CONSTANT)
  Q_PROPERTY(qint64 cacheSize READ cacheSize NOTIFY cacheSizeChanged)
  Q_PROPERTY(bool httpServerAvailable READ httpServerAvailable CONSTANT)

  CloudContext(QObject* parent = nullptr);
  ~CloudContext();

  void loadCachedDirectories();
  void saveCachedDirectories();

  void save();

  QStringList providers() const;
  QVariantList userProviders() const;
  bool includeAds() const;
  bool isFree() const;
  bool httpServerAvailable() const;

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

  void cacheDirectory(ListDirectoryCacheKey directory,
                      const std::vector<cloudstorage::IItem::Pointer>&);
  std::vector<cloudstorage::IItem::Pointer> cachedDirectory(
      ListDirectoryCacheKey);
  void schedule(std::function<void()>);

  static QString thumbnail_path(const QString& filename);

 signals:
  void userProvidersChanged();
  void receivedCode(QString provider);
  void errorOccurred(QString operation, QVariantMap provider, int code,
                     QString description);
  void cacheSizeChanged();

 private:
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
  cloudstorage::ICloudProvider::InitData init_data(
      const std::string& name) const;

  mutable std::mutex mutex_;
  QJsonDocument config_;
  DebugStream debug_stream_;
  std::shared_ptr<ServerWrapperFactory> http_server_factory_;
  std::vector<std::shared_ptr<cloudstorage::IHttpServer>> auth_server_;
  std::shared_ptr<cloudstorage::IHttp> http_;
  std::shared_ptr<cloudstorage::IThreadPool> thread_pool_;
  RequestPool pool_;
  std::vector<Provider> provider_;
  std::unordered_map<ListDirectoryCacheKey,
                     std::vector<cloudstorage::IItem::Pointer>>
      list_directory_cache_;
  qint64 cache_size_;

  Q_OBJECT
};

#endif  // CLOUD_CONTEXT
