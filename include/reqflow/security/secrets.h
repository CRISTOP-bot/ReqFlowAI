#pragma once

// Detección heurística de secretos compartida por ProjectStore, MainWindow,
// FlowEngine y FlowEditor. Antes cada componente usaba un patrón distinto y se
// colaban casos que uno filtraba y otro no. La redacción es heurística y no
// equivale a una bóveda segura (docs/SECURITY.md).

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

namespace reqflow::security {

// Patrón único de nombres que parecen contener secretos. `auth`, `authentication`
// y `authorization` solo se reconocen en límites de palabra para no rechazar
// cabeceras inocuas como `Author`, `Content` o `X-Oauth-Trial` mal formado.
inline const QRegularExpression &sensitiveNamePattern() {
    static const QRegularExpression pattern(
        QStringLiteral("pass(word)?|passwd|secret|token|api[_-]?key|access[_-]?key|"
                       "credential|private[_-]?key|cookie|session|"
                       "(?:^|[-_.])auth(?:entication|orization)?(?=$|[-_.])"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// Valores con forma de credencial: esquemas de autorización, claves conocidas
// por su prefijo y tokens JWT (eyJ….<cuerpo>.<firma>).
inline const QRegularExpression &credentialValuePattern() {
    static const QRegularExpression pattern(
        QStringLiteral("\\b(?:Bearer|Basic)\\s+[A-Za-z0-9._~+/=-]{8,}"
                       "|\\bsk-[A-Za-z0-9_-]{12,}"
                       "|\\bgh[pousr]_[A-Za-z0-9]{16,}"
                       "|\\bAKIA[A-Z0-9]{16}\\b"
                       "|\\beyJ[A-Za-z0-9_-]{8,}\\.[A-Za-z0-9_-]{4,}\\.[A-Za-z0-9_-]*"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

inline bool nameLooksSensitive(const QString &name) {
    return sensitiveNamePattern().match(name).hasMatch();
}

inline bool valueLooksLikeCredential(const QString &value) {
    return !value.isEmpty() && credentialValuePattern().match(value).hasMatch();
}

// Cuerpos que no son JSON: claves de form-urlencoded (`password=…`), etiquetas
// XML (`<token>`), atributos (`password="…"` y `name="password"`) y cualquier
// texto con forma de credencial.
inline bool textBodyLooksSensitive(const QString &text) {
    if (text.isEmpty()) return false;
    if (valueLooksLikeCredential(text)) return true;
    static const QRegularExpression formKey(
        QStringLiteral("(?:^|[&;?])\\s*([^=&;\\s]{1,64})\\s*="));
    auto formMatch = formKey.globalMatch(text);
    while (formMatch.hasNext())
        if (nameLooksSensitive(formMatch.next().captured(1))) return true;
    static const QRegularExpression xmlTag(
        QStringLiteral("<\\s*/?\\s*([A-Za-z_][A-Za-z0-9_.:-]{0,63})"));
    auto tagMatch = xmlTag.globalMatch(text);
    while (tagMatch.hasNext())
        if (nameLooksSensitive(tagMatch.next().captured(1))) return true;
    static const QRegularExpression attribute(
        QStringLiteral("([A-Za-z_][A-Za-z0-9_.:-]{0,63})\\s*=\\s*[\"']([^\"']{1,64})[\"']"));
    auto attributeMatch = attribute.globalMatch(text);
    while (attributeMatch.hasNext()) {
        const auto match = attributeMatch.next();
        if (nameLooksSensitive(match.captured(1))) return true;
        if (match.captured(1).compare(QStringLiteral("name"), Qt::CaseInsensitive) == 0 &&
            nameLooksSensitive(match.captured(2)))
            return true;
    }
    return false;
}

// JSON: claves sensibles O valores con forma de credencial (o con cuerpo
// sensible embebido como texto). Recursivo sobre objetos/arreglos.
inline bool jsonLooksSensitive(const QJsonValue &value) {
    if (value.isObject()) {
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            if (nameLooksSensitive(it.key()) || jsonLooksSensitive(it.value())) return true;
        return false;
    }
    if (value.isArray()) {
        for (const auto &item : value.toArray())
            if (jsonLooksSensitive(item)) return true;
        return false;
    }
    if (value.isString()) {
        const QString text = value.toString();
        if (valueLooksLikeCredential(text)) return true;
        return textBodyLooksSensitive(text);
    }
    return false;
}

// URL: parámetros de query con nombre sensible o valor con forma de credencial.
// No mira el esquema (eso lo decide cada capa: ver B4 en docs/SECURITY_AUDIT.md).
inline bool urlLooksSensitive(const QString &url) {
    const QUrlQuery query{QUrl(url)};
    for (const auto &item : query.queryItems(QUrl::FullyDecoded))
        if (nameLooksSensitive(item.first) || valueLooksLikeCredential(item.second)) return true;
    return false;
}

// Bloques de cabeceras `Nombre: valor` separados por salto de línea.
inline bool headerLinesLookSensitive(const QString &headersText) {
    for (const QString &line : headersText.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon <= 0) continue;
        if (nameLooksSensitive(line.left(colon).trimmed()) ||
            valueLooksLikeCredential(line.mid(colon + 1).trimmed()))
            return true;
    }
    return false;
}

} // namespace reqflow::security
