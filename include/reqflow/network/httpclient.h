#pragma once

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>

namespace reqflow::network {

struct HttpResponse {
    QUrl url;
    int status = 0;
    QString reason;
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;
    qint64 elapsedMs = 0;
    QString error;

    [[nodiscard]] qint64 sizeBytes() const noexcept { return body.size(); }
    [[nodiscard]] QByteArray header(const QByteArray &name) const;
};

struct HttpRequest {
    QByteArray method = "GET";
    QUrl url;
    QMap<QByteArray, QByteArray> headers;
    QByteArray body;
    int timeoutMs = 20000;
};

class HttpClient final : public QObject {
    Q_OBJECT
public:
    explicit HttpClient(QObject *parent = nullptr);
    [[nodiscard]] QNetworkReply *send(const HttpRequest &request);
    void abortAll();

signals:
    void finished(const reqflow::network::HttpResponse &response);

private:
    QNetworkAccessManager manager_;
    QList<QNetworkReply *> activeReplies_;
};

} // namespace reqflow::network

Q_DECLARE_METATYPE(reqflow::network::HttpResponse)
