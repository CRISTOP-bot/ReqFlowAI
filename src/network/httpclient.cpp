#include "reqflow/network/httpclient.h"

#include <QElapsedTimer>
#include <QTimer>

namespace reqflow::network {

QByteArray HttpResponse::header(const QByteArray &name) const {
    for (const auto &entry : headers) {
        if (entry.first.compare(name, Qt::CaseInsensitive) == 0)
            return entry.second;
    }
    return {};
}

HttpClient::HttpClient(QObject *parent) : QObject(parent) {
    qRegisterMetaType<HttpResponse>();
}

QNetworkReply *HttpClient::send(const HttpRequest &request) {
    const QByteArray method = request.method.trimmed().toUpper();
    static const QList<QByteArray> allowed = {"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"};
    if (!allowed.contains(method) || !request.url.isValid() ||
        (request.url.scheme() != QStringLiteral("http") && request.url.scheme() != QStringLiteral("https"))) {
        HttpResponse response;
        response.url = request.url;
        response.error = QStringLiteral("Método o URL no válido.");
        QTimer::singleShot(0, this, [this, response] { emit finished(response); });
        return nullptr;
    }

    QNetworkRequest networkRequest(request.url);
    const int timeoutMs = qBound(1000, request.timeoutMs, 120000);
    auto *timeoutTimer = new QTimer;
    timeoutTimer->setSingleShot(true);
    timeoutTimer->start(timeoutMs);
    for (auto it = request.headers.cbegin(); it != request.headers.cend(); ++it)
        networkRequest.setRawHeader(it.key(), it.value());

    QNetworkReply *reply = nullptr;
    if (method == "GET") reply = manager_.get(networkRequest);
    else if (method == "HEAD") reply = manager_.head(networkRequest);
    else if (method == "POST") reply = manager_.post(networkRequest, request.body);
    else if (method == "PUT") reply = manager_.put(networkRequest, request.body);
    else if (method == "DELETE") reply = manager_.sendCustomRequest(networkRequest, method, request.body);
    else reply = manager_.sendCustomRequest(networkRequest, method, request.body);

    activeReplies_.append(reply);
    connect(timeoutTimer, &QTimer::timeout, reply, [reply] { reply->setProperty("reqflowTimedOut", true); reply->abort(); });
    connect(reply, &QNetworkReply::finished, timeoutTimer, &QObject::deleteLater);
    auto *timer = new QElapsedTimer;
    timer->start();
    connect(reply, &QNetworkReply::finished, this, [this, reply, timer] {
        HttpResponse response;
        response.url = reply->url();
        response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        response.reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        response.body = reply->readAll();
        response.elapsedMs = timer->elapsed();
        response.error = reply->property("reqflowTimedOut").toBool()
                             ? QStringLiteral("La solicitud excedió el tiempo límite.")
                             : (reply->error() == QNetworkReply::NoError || response.status > 0
                                    ? QString{} : reply->errorString());
        const auto rawHeaders = reply->rawHeaderPairs();
        for (const auto &header : rawHeaders)
            response.headers.append(qMakePair(header.first, header.second));
        activeReplies_.removeAll(reply);
        delete timer;
        reply->deleteLater();
        emit finished(response);
    });
    return reply;
}

void HttpClient::abortAll() {
    const auto replies = activeReplies_;
    for (QNetworkReply *reply : replies)
        if (reply && reply->isRunning()) reply->abort();
}

} // namespace reqflow::network
