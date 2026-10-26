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

#include <functional>

namespace reqflow::network {

// M1: tope de tamaño de respuesta; ver docs/SECURITY_AUDIT.md.
inline constexpr qint64 kMaxResponseBytes = 10 * 1024 * 1024;
// B2: máximo de saltos de redirección por petición.
inline constexpr int kMaxRedirects = 5;

// Decide si se permite una redirección de `from` a `to`. Sin verificador,
// HttpClient solo permite saltos al mismo origen (esquema/host/puerto).
using RedirectVerifier = std::function<bool(const QUrl &from, const QUrl &to)>;

[[nodiscard]] bool isSameOrigin(const QUrl &left, const QUrl &right);
[[nodiscard]] bool isValidHeaderName(const QString &name);
[[nodiscard]] bool isValidHeaderValue(const QString &value);

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
    [[nodiscard]] QNetworkReply *send(const HttpRequest &request,
                                      const RedirectVerifier &verifier = {});
    void abortAll();

signals:
    void finished(const reqflow::network::HttpResponse &response);

private:
    QNetworkAccessManager manager_;
    QList<QNetworkReply *> activeReplies_;
};

} // namespace reqflow::network

Q_DECLARE_METATYPE(reqflow::network::HttpResponse)
