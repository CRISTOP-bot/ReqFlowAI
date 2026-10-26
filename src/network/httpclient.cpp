#include "reqflow/network/httpclient.h"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QTimer>

namespace reqflow::network {
namespace {
int effectivePort(const QUrl &url) {
    const int port = url.port();
    if (port > 0) return port;
    const QString scheme = url.scheme().toLower();
    if (scheme == QStringLiteral("http")) return 80;
    if (scheme == QStringLiteral("https")) return 443;
    return -1;
}
} // namespace

bool isSameOrigin(const QUrl &left, const QUrl &right) {
    return left.isValid() && right.isValid() &&
           left.scheme().compare(right.scheme(), Qt::CaseInsensitive) == 0 &&
           QString::compare(left.host(), right.host(), Qt::CaseInsensitive) == 0 &&
           effectivePort(left) == effectivePort(right);
}

// B1: nombre de cabecera = token RFC 7230 (tchar). Qt 6.12 descarta en silencio
// los nombres inválidos y Qt 6.4 no los validaba; lo rechazamos de forma explícita.
bool isValidHeaderName(const QString &name) {
    static const QRegularExpression pattern(QStringLiteral("^[!#$%&'*+.^_`|~0-9A-Za-z-]+$"));
    return !name.isEmpty() && pattern.match(name).hasMatch();
}

// B1: valor de cabecera sin CR, LF, NUL, otros caracteres de control (<0x20,
// salvo HTAB) ni DEL; se admite obs-text (>=0x80) según RFC 7230.
bool isValidHeaderValue(const QString &value) {
    for (const QChar character : value) {
        const ushort code = character.unicode();
        if (code == QChar::Null || code == 0x7F) return false;
        if (code < 0x20 && character != QLatin1Char('\t')) return false;
    }
    return true;
}

QByteArray HttpResponse::header(const QByteArray &name) const {
    for (const auto &entry : headers) {
        if (entry.first.compare(name, Qt::CaseInsensitive) == 0)
            return entry.second;
    }
    return {};
}

HttpClient::HttpClient(QObject *parent) : QObject(parent) {
    qRegisterMetaType<HttpResponse>();
    // M1: el tope se vigila con downloadProgress (no existe búfer de lectura
    // configurable en QNetworkAccessManager de Qt 6; el abort diferido corta
    // la descarga aunque el servidor siga enviando).
}

QNetworkReply *HttpClient::send(const HttpRequest &request, const RedirectVerifier &verifier) {
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
    // B2: ninguna redirección se sigue de forma implícita; cada salto pasa por
    // el verificador (por defecto: solo mismo origen respecto de la URL original).
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                QNetworkRequest::UserVerifiedRedirectPolicy);
    networkRequest.setMaximumRedirectsAllowed(kMaxRedirects);
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

    // B2: cada redirección se decide antes de reenviarla.
    connect(reply, &QNetworkReply::redirected, reply,
            [reply, verifier, originalUrl = request.url, timeoutMs, timeoutTimer](const QUrl &target) {
                timeoutTimer->stop();
                const QUrl from = reply->url();
                const QUrl resolved = from.resolved(target);
                const bool allowed =
                    verifier ? verifier(from, resolved) : isSameOrigin(originalUrl, resolved);
                if (allowed) {
                    timeoutTimer->start(timeoutMs);
                    reply->redirectAllowed();
                    return;
                }
                reply->setProperty("reqflowRedirectDeclined", true);
                reply->setProperty("reqflowRedirectTarget", resolved.toString());
                reply->abort();
            });

    // M1: si el servidor supera el tope se aborta la descarga de forma diferida
    // (para no abortar dentro de la propia señal de progreso).
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
        if (reply->property("reqflowTooLarge").toBool()) return;
        if (total > kMaxResponseBytes || received > kMaxResponseBytes) {
            reply->setProperty("reqflowTooLarge", true);
            QTimer::singleShot(0, reply, [reply] { if (reply->isRunning()) reply->abort(); });
        }
    });

    auto *timer = new QElapsedTimer;
    timer->start();
    connect(reply, &QNetworkReply::finished, this, [this, reply, timer] {
        HttpResponse response;
        response.url = reply->url();
        response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        response.reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        const bool tooLarge = reply->property("reqflowTooLarge").toBool();
        const bool redirectDeclined = reply->property("reqflowRedirectDeclined").toBool();
        response.body = tooLarge ? QByteArray{} : reply->readAll();
        response.elapsedMs = timer->elapsed();
        response.error =
            tooLarge
                ? QStringLiteral("La respuesta supera el límite de 10 MiB; se abortó la descarga.")
                : (redirectDeclined
                       ? QStringLiteral("Redirección rechazada hacia %1: solo se permiten saltos al mismo origen o confirmados por el usuario.")
                             .arg(reply->property("reqflowRedirectTarget").toString())
                       : (reply->property("reqflowTimedOut").toBool()
                              ? QStringLiteral("La solicitud excedió el tiempo límite.")
                              : (reply->error() == QNetworkReply::NoError || response.status > 0
                                     ? QString{} : reply->errorString())));
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
