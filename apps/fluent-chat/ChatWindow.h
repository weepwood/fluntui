#pragma once
#include "ApiClient.h"
#include <FluentQt/FluentQt.h>
#include <QJsonArray>
#include <QJsonObject>
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QScrollArea;
class QStackedWidget;
class QTimer;
class QVBoxLayout;
class QWidget;

class ChatWindow final : public fluent::windowing::Window {
public:
    ChatWindow();
    void authenticate();

private:
    enum class View { Conversations, Contacts, Moments };
    void buildUi();
    void applyTheme(bool dark);
    void switchView(View view);
    void showError(const QString& error);
    void refreshList();
    void refreshMessages();
    void showConversations(const QJsonArray& conversations);
    void showContacts(const QJsonArray& contacts);
    void showMoments(const QJsonArray& moments);
    void openConversation(const QString& id, const QString& title);
    void openDirect(const QString& userId, const QString& title);
    void renderMessages(const QJsonArray& messages);
    void sendMessage();
    void addFriend();
    void handleRequests();
    void createGroup();
    void publishMoment();
    void signOut();

    ApiClient m_api;
    View m_view = View::Conversations;
    QString m_userId;
    QString m_displayName;
    QString m_conversationId;
    QString m_conversationName;
    QString m_lastMessageMarker;
    QJsonArray m_contacts;
    bool m_dark = false;
    bool m_listLoading = false;
    bool m_messagesLoading = false;
    QWidget* m_root = nullptr;
    QListWidget* m_list = nullptr;
    QLineEdit* m_filter = nullptr;
    QLabel* m_listTitle = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_chatTitle = nullptr;
    QStackedWidget* m_mainStack = nullptr;
    QWidget* m_blankPage = nullptr;
    QWidget* m_chatPage = nullptr;
    QWidget* m_momentsPage = nullptr;
    QScrollArea* m_history = nullptr;
    QVBoxLayout* m_messageLayout = nullptr;
    QVBoxLayout* m_momentsLayout = nullptr;
    QPlainTextEdit* m_composer = nullptr;
    QTimer* m_poll = nullptr;
};
