#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QUrl>
#include <functional>

class ApiClient : public QObject {
public:
    using Handler = std::function<void(const QJsonObject&, const QString&)>;
    explicit ApiClient(QObject* parent = nullptr) : QObject(parent) {}

    void request(bool post, const QString& path, const QJsonObject& data, Handler handler) {
        if (!path.startsWith('/') || path.startsWith("//")) {
            handler({}, QStringLiteral("无效的 API 路径"));
            return;
        }
        // Dev-only endpoint: credentials never sent off loopback.
        QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:8787/api") + path));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Cache-Control", "no-store");
        if (!m_token.isEmpty())
            request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
        QNetworkReply* reply = post
            ? m_manager.post(request, QJsonDocument(data).toJson(QJsonDocument::Compact))
            : m_manager.get(request);
        connect(reply, &QNetworkReply::finished, this, [reply, handler = std::move(handler)] {
            const QByteArray raw = reply->readAll();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QJsonParseError parseError{};
            const QJsonDocument parsed = QJsonDocument::fromJson(raw, &parseError);
            const QJsonObject object = parsed.isObject() ? parsed.object() : QJsonObject{};
            QString error;
            if (reply->error() != QNetworkReply::NoError || status >= 400) {
                error = object.value(QStringLiteral("error")).toString();
                if (error.isEmpty()) error = reply->errorString();
            } else if (!parsed.isObject() || parseError.error != QJsonParseError::NoError) {
                error = QStringLiteral("无法解析服务端返回的数据");
            }
            reply->deleteLater();
            handler(object, error);
        });
    }
    void get(const QString& path, Handler handler) { request(false, path, {}, std::move(handler)); }
    void post(const QString& path, const QJsonObject& body, Handler handler) {
        request(true, path, body, std::move(handler));
    }
    void setToken(const QString& token) { m_token = token; }
    QString token() const { return m_token; }
    void clearToken() { m_token.clear(); }

private:
    QNetworkAccessManager m_manager;
    QString m_token;
};
