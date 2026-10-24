#include "ChatWindow.h"

#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

namespace {
fluent::basicinput::Button* button(const QString& text, QWidget* parent,
                                    bool accent = false) {
    auto* result = new fluent::basicinput::Button(text, parent);
    result->setFluentStyle(accent ? fluent::basicinput::Button::Accent
                                   : fluent::basicinput::Button::Subtle);
    result->setMinimumHeight(39);
    return result;
}
QLabel* label(const QString& text, const QString& id, QWidget* parent) {
    auto* result = new QLabel(text, parent);
    result->setObjectName(id);
    result->setWordWrap(true);
    return result;
}
QString userDisplay(const QJsonObject& object) {
    return object.value(QStringLiteral("displayName")).toString(
        object.value(QStringLiteral("username")).toString(QStringLiteral("未知用户")));
}
void clearLayout(QVBoxLayout* layout) {
    while (auto* item = layout->takeAt(0)) {
        if (auto* widget = item->widget()) delete widget;
        delete item;
    }
}
}

ChatWindow::ChatWindow() {
    setWindowTitle(QStringLiteral("FluentChat · 原生聊天客户端"));
    setMinimumSize(980, 620);
    resize(1260, 790);
    buildUi();
    applyTheme(QSettings().value(QStringLiteral("fluentChatDark"), false).toBool());
    m_poll = new QTimer(this);
    m_poll->setInterval(2500);
    connect(m_poll, &QTimer::timeout, this, [this] {
        if (m_api.token().isEmpty()) return;
        refreshList();
        if (!m_conversationId.isEmpty()) refreshMessages();
    });
}

void ChatWindow::buildUi() {
    m_root = new QWidget;
    m_root->setObjectName(QStringLiteral("chatRoot"));
    auto* all = new QHBoxLayout(m_root);
    all->setContentsMargins(0, 0, 0, 0);
    all->setSpacing(0);

    auto* rail = new QWidget(m_root);
    rail->setObjectName(QStringLiteral("rail"));
    rail->setFixedWidth(96);
    auto* railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(10, 24, 10, 20);
    railLayout->setSpacing(9);
    railLayout->addWidget(label(QStringLiteral("WC"), QStringLiteral("logo"), rail),
                          0, Qt::AlignHCenter);
    railLayout->addSpacing(28);
    for (const auto& entry : {qMakePair(QStringLiteral("消息"), View::Conversations),
                              qMakePair(QStringLiteral("通讯录"), View::Contacts),
                              qMakePair(QStringLiteral("动态"), View::Moments)}) {
        auto* action = button(entry.first, rail);
        railLayout->addWidget(action);
        connect(action, &QPushButton::clicked, this, [this, view = entry.second] {
            switchView(view);
        });
    }
    railLayout->addStretch();
    auto* themeButton = button(QStringLiteral("主题"), rail);
    connect(themeButton, &QPushButton::clicked, this, [this] {
        applyTheme(!m_dark);
        QSettings().setValue(QStringLiteral("fluentChatDark"), m_dark);
    });
    railLayout->addWidget(themeButton);
    auto* logout = button(QStringLiteral("退出"), rail);
    connect(logout, &QPushButton::clicked, this, [this] { signOut(); });
    railLayout->addWidget(logout);
    all->addWidget(rail);

    auto* sidebar = new QWidget(m_root);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(306);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(16, 22, 16, 18);
    sidebarLayout->setSpacing(12);
    m_listTitle = label(QStringLiteral("消息"), QStringLiteral("panelTitle"), sidebar);
    sidebarLayout->addWidget(m_listTitle);
    m_filter = new QLineEdit(sidebar);
    m_filter->setPlaceholderText(QStringLiteral("搜索会话或联系人"));
    m_filter->setClearButtonEnabled(true);
    sidebarLayout->addWidget(m_filter);
    connect(m_filter, &QLineEdit::textChanged, this, [this] { refreshList(); });

    auto* actions = new QHBoxLayout;
    actions->setSpacing(5);
    auto* addFriendButton = button(QStringLiteral("加好友"), sidebar);
    auto* requestsButton = button(QStringLiteral("申请"), sidebar);
    auto* groupButton = button(QStringLiteral("建群"), sidebar);
    actions->addWidget(addFriendButton);
    actions->addWidget(requestsButton);
    actions->addWidget(groupButton);
    sidebarLayout->addLayout(actions);
    connect(addFriendButton, &QPushButton::clicked, this, [this] { addFriend(); });
    connect(requestsButton, &QPushButton::clicked, this, [this] { handleRequests(); });
    connect(groupButton, &QPushButton::clicked, this, [this] { createGroup(); });

    m_list = new QListWidget(sidebar);
    m_list->setObjectName(QStringLiteral("conversationList"));
    m_list->setSpacing(6);
    m_list->setWordWrap(true);
    m_list->setFrameShape(QFrame::NoFrame);
    sidebarLayout->addWidget(m_list, 1);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const QString id = item->data(Qt::UserRole).toString();
        const QString title = item->data(Qt::UserRole + 1).toString();
        if (m_view == View::Contacts) openDirect(id, title);
        else if (m_view == View::Conversations) openConversation(id, title);
    });
    m_status = label(QStringLiteral("正在连接..."), QStringLiteral("status"), sidebar);
    sidebarLayout->addWidget(m_status);
    all->addWidget(sidebar);

    m_mainStack = new QStackedWidget(m_root);
    m_mainStack->setObjectName(QStringLiteral("mainStack"));
    m_blankPage = new QWidget(m_mainStack);
    auto* blank = new QVBoxLayout(m_blankPage);
    blank->addStretch();
    auto* mark = label(QStringLiteral("FluentChat"), QStringLiteral("hero"), m_blankPage);
    mark->setAlignment(Qt::AlignCenter);
    blank->addWidget(mark);
    auto* description = label(QStringLiteral("选择一个会话开始聊天\nC++ / Qt 6 / FluentQt"), QStringLiteral("hint"), m_blankPage);
    description->setAlignment(Qt::AlignCenter);
    blank->addWidget(description);
    blank->addStretch();
    m_mainStack->addWidget(m_blankPage);

    m_chatPage = new QWidget(m_mainStack);
    auto* chatLayout = new QVBoxLayout(m_chatPage);
    chatLayout->setContentsMargins(24, 18, 24, 18);
    chatLayout->setSpacing(12);
    m_chatTitle = label(QStringLiteral("会话"), QStringLiteral("chatTitle"), m_chatPage);
    chatLayout->addWidget(m_chatTitle);
    auto* separator = new QFrame(m_chatPage);
    separator->setFrameShape(QFrame::HLine);
    chatLayout->addWidget(separator);
    m_history = new QScrollArea(m_chatPage);
    m_history->setObjectName(QStringLiteral("history"));
    m_history->setWidgetResizable(true);
    m_history->setFrameShape(QFrame::NoFrame);
    auto* historyHost = new QWidget(m_history);
    historyHost->setObjectName(QStringLiteral("historyHost"));
    m_messageLayout = new QVBoxLayout(historyHost);
    m_messageLayout->setContentsMargins(8, 12, 8, 20);
    m_messageLayout->setSpacing(14);
    m_messageLayout->addStretch();
    m_history->setWidget(historyHost);
    chatLayout->addWidget(m_history, 1);
    m_composer = new QPlainTextEdit(m_chatPage);
    m_composer->setObjectName(QStringLiteral("composer"));
    m_composer->setPlaceholderText(QStringLiteral("输入消息（Ctrl+Enter 发送）"));
    m_composer->setMaximumHeight(110);
    chatLayout->addWidget(m_composer);
    auto* sendRow = new QHBoxLayout;
    sendRow->addWidget(label(QStringLiteral("仅支持文本 · 最多 4000 字"), QStringLiteral("hint"), m_chatPage));
    sendRow->addStretch();
    auto* send = button(QStringLiteral("发送"), m_chatPage, true);
    send->setMinimumWidth(110);
    sendRow->addWidget(send);
    chatLayout->addLayout(sendRow);
    connect(send, &QPushButton::clicked, this, [this] { sendMessage(); });
    auto* shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), m_chatPage);
    connect(shortcut, &QShortcut::activated, this, [this] { sendMessage(); });
    m_mainStack->addWidget(m_chatPage);

    m_momentsPage = new QWidget(m_mainStack);
    auto* momentsOuter = new QVBoxLayout(m_momentsPage);
    momentsOuter->setContentsMargins(26, 22, 26, 20);
    auto* momentsHeader = new QHBoxLayout;
    momentsHeader->addWidget(label(QStringLiteral("朋友动态"), QStringLiteral("panelTitle"), m_momentsPage));
    momentsHeader->addStretch();
    auto* publishButton = button(QStringLiteral("发表动态"), m_momentsPage, true);
    momentsHeader->addWidget(publishButton);
    connect(publishButton, &QPushButton::clicked, this, [this] { publishMoment(); });
    momentsOuter->addLayout(momentsHeader);
    auto* momentsScroll = new QScrollArea(m_momentsPage);
    momentsScroll->setFrameShape(QFrame::NoFrame);
    momentsScroll->setWidgetResizable(true);
    auto* momentsHost = new QWidget(momentsScroll);
    m_momentsLayout = new QVBoxLayout(momentsHost);
    m_momentsLayout->setContentsMargins(0, 8, 8, 16);
    momentsScroll->setWidget(momentsHost);
    momentsOuter->addWidget(momentsScroll);
    m_mainStack->addWidget(m_momentsPage);

    all->addWidget(m_mainStack, 1);
    m_mainStack->setCurrentWidget(m_blankPage);
    setContentWidget(m_root);
}

void ChatWindow::authenticate() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("登录 / 注册 · FluentChat"));
    dialog.setMinimumWidth(390);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->addWidget(label(QStringLiteral("欢迎使用 FluentChat"), QStringLiteral("panelTitle"), &dialog));
    auto* form = new QFormLayout;
    auto* username = new QLineEdit(&dialog);
    username->setPlaceholderText(QStringLiteral("用户名（字母、数字、下划线）"));
    auto* password = new QLineEdit(&dialog);
    password->setEchoMode(QLineEdit::Password);
    password->setPlaceholderText(QStringLiteral("至少 8 个字符"));
    auto* display = new QLineEdit(&dialog);
    display->setPlaceholderText(QStringLiteral("注册时使用的昵称"));
    form->addRow(QStringLiteral("账号"), username);
    form->addRow(QStringLiteral("密码"), password);
    form->addRow(QStringLiteral("昵称"), display);
    layout->addLayout(form);
    auto* login = button(QStringLiteral("登录"), &dialog, true);
    auto* registerButton = button(QStringLiteral("注册"), &dialog);
    layout->addWidget(login);
    layout->addWidget(registerButton);
    auto submit = [this, &dialog, username, password, display, login, registerButton](bool registerUser) {
        login->setEnabled(false);
        registerButton->setEnabled(false);
        QJsonObject payload{{QStringLiteral("username"), username->text().trimmed()},
                            {QStringLiteral("password"), password->text()}};
        if (registerUser) payload.insert(QStringLiteral("displayName"), display->text().trimmed());
        const QString path = registerUser ? QStringLiteral("/auth/register") : QStringLiteral("/auth/login");
        QPointer<QDialog> safeDialog(&dialog);
        m_api.post(path, payload, [this, safeDialog, login, registerButton](const QJsonObject& data, const QString& error) {
            if (!safeDialog) return;
            login->setEnabled(true);
            registerButton->setEnabled(true);
            if (!error.isEmpty()) {
                QMessageBox::warning(safeDialog, QStringLiteral("登录失败"), error);
                return;
            }
            m_api.setToken(data.value(QStringLiteral("token")).toString());
            const QJsonObject user = data.value(QStringLiteral("user")).toObject();
            m_userId = user.value(QStringLiteral("id")).toString();
            m_displayName = userDisplay(user);
            safeDialog->accept();
        });
    };
    connect(login, &QPushButton::clicked, &dialog, [submit] { submit(false); });
    connect(registerButton, &QPushButton::clicked, &dialog, [submit] { submit(true); });
    if (dialog.exec() != QDialog::Accepted) {
        close();
        return;
    }
    m_status->setText(QStringLiteral("已登录 · ") + m_displayName);
    switchView(View::Conversations);
    m_poll->start();
}

void ChatWindow::applyTheme(bool dark) {
    m_dark = dark;
    fluent::FluentElement::setTheme(dark ? fluent::FluentElement::Dark : fluent::FluentElement::Light);
    const QString background = dark ? QStringLiteral("#151b25") : QStringLiteral("#f5f7fa");
    const QString side = dark ? QStringLiteral("#1d2733") : QStringLiteral("#edf2f7");
    const QString foreground = dark ? QStringLiteral("#ecf1f7") : QStringLiteral("#202b3b");
    const QString border = dark ? QStringLiteral("#344153") : QStringLiteral("#dce3eb");
    const QString muted = dark ? QStringLiteral("#95a5b9") : QStringLiteral("#68798d");
    m_root->setStyleSheet(QStringLiteral(R"(
      QWidget#chatRoot, QWidget#mainStack, QScrollArea#history, QWidget#historyHost { background: %1; }
      QWidget#rail, QWidget#sidebar { background: %2; border-right: 1px solid %3; }
      QLabel { color: %4; background: transparent; border: none; }
      QLabel#logo { font-size: 24px; font-weight: 800; color: #54a1fa; }
      QLabel#hero { font-size: 34px; font-weight: 700; }
      QLabel#panelTitle, QLabel#chatTitle { font-size: 19px; font-weight: 700; }
      QLabel#hint, QLabel#status { font-size: 12px; color: %5; }
      QLineEdit, QPlainTextEdit { color: %4; background: %1; border: 1px solid %3; border-radius: 8px; padding: 8px; }
      QListWidget { background: %2; color: %4; border: none; outline: none; }
      QListWidget::item { border-radius: 9px; padding: 12px 9px; }
      QListWidget::item:selected { background: %3; }
      QScrollArea, QScrollArea > QWidget > QWidget { border: none; }
      QScrollBar:vertical { width: 7px; background: transparent; }
      QScrollBar::handle:vertical { background: %3; border-radius: 3px; }
    )").arg(background, side, border, foreground, muted));
    if (!m_conversationId.isEmpty()) m_lastMessageMarker.clear();
    if (!m_conversationId.isEmpty()) refreshMessages();
}

void ChatWindow::switchView(View view) {
    m_view = view;
    m_filter->clear();
    m_list->clear();
    if (view == View::Moments) {
        m_listTitle->setText(QStringLiteral("发现"));
        m_mainStack->setCurrentWidget(m_momentsPage);
    } else {
        m_listTitle->setText(view == View::Contacts ? QStringLiteral("通讯录") : QStringLiteral("消息"));
        m_mainStack->setCurrentWidget(m_blankPage);
    }
    if (view != View::Conversations) m_conversationId.clear();
    refreshList();
}

void ChatWindow::showError(const QString& error) {
    if (!error.isEmpty()) QMessageBox::warning(this, QStringLiteral("操作失败"), error);
}

void ChatWindow::refreshList() {
    if (m_api.token().isEmpty() || m_listLoading) return;
    m_listLoading = true;
    const View current = m_view;
    QString path;
    if (current == View::Conversations) path = QStringLiteral("/conversations");
    else if (current == View::Contacts) path = QStringLiteral("/contacts");
    else path = QStringLiteral("/moments");
    m_api.get(path, [this, current](const QJsonObject& data, const QString& error) {
        m_listLoading = false;
        if (!error.isEmpty()) {
            m_status->setText(QStringLiteral("连接中断 · ") + error);
            return;
        }
        m_status->setText(QStringLiteral("已登录 · ") + m_displayName);
        if (m_view != current) return;
        if (current == View::Conversations)
            showConversations(data.value(QStringLiteral("conversations")).toArray());
        else if (current == View::Contacts)
            showContacts(data.value(QStringLiteral("contacts")).toArray());
        else
            showMoments(data.value(QStringLiteral("moments")).toArray());
    });
}

void ChatWindow::showConversations(const QJsonArray& conversations) {
    const QString query = m_filter->text().trimmed();
    const QSignalBlocker block(m_list);
    m_list->clear();
    for (const QJsonValue& v : conversations) {
        const QJsonObject c = v.toObject();
        const QString title = c.value(QStringLiteral("title")).toString(QStringLiteral("未命名会话"));
        if (!title.contains(query, Qt::CaseInsensitive)) continue;
        const QJsonObject last = c.value(QStringLiteral("lastMessage")).toObject();
        const QString preview = last.value(QStringLiteral("body")).toString().left(35);
        const int unread = c.value(QStringLiteral("unread")).toInt();
        const QString display = QStringLiteral("%1%2\n%3")
            .arg(title, unread ? QStringLiteral("  ·  %1 条未读").arg(unread) : QString(),
                 preview.isEmpty() ? QStringLiteral("开始聊天") : preview);
        auto* item = new QListWidgetItem(display, m_list);
        item->setData(Qt::UserRole, c.value(QStringLiteral("id")).toString());
        item->setData(Qt::UserRole + 1, title);
        item->setSizeHint(QSize(235, 67));
        if (c.value(QStringLiteral("id")).toString() == m_conversationId)
            m_list->setCurrentItem(item);
    }
}

void ChatWindow::showContacts(const QJsonArray& contacts) {
    m_contacts = contacts;
    const QString query = m_filter->text().trimmed();
    const QSignalBlocker block(m_list);
    m_list->clear();
    for (const QJsonValue& v : contacts) {
        const QJsonObject person = v.toObject();
        const QString name = userDisplay(person);
        const QString username = person.value(QStringLiteral("username")).toString();
        if (!name.contains(query, Qt::CaseInsensitive) &&
            !username.contains(query, Qt::CaseInsensitive)) continue;
        auto* item = new QListWidgetItem(name + QStringLiteral("\n@") + username, m_list);
        item->setData(Qt::UserRole, person.value(QStringLiteral("id")).toString());
        item->setData(Qt::UserRole + 1, name);
        item->setSizeHint(QSize(235, 64));
    }
}

void ChatWindow::openDirect(const QString& userId, const QString& title) {
    if (userId.isEmpty()) return;
    m_api.post(QStringLiteral("/conversations/direct"),
               QJsonObject{{QStringLiteral("userId"), userId}},
               [this, title](const QJsonObject& data, const QString& error) {
        if (!error.isEmpty()) { showError(error); return; }
        m_view = View::Conversations;
        m_listTitle->setText(QStringLiteral("消息"));
        openConversation(data.value(QStringLiteral("conversationId")).toString(), title);
        refreshList();
    });
}

void ChatWindow::openConversation(const QString& id, const QString& title) {
    if (id.isEmpty()) return;
    m_conversationId = id;
    m_conversationName = title;
    m_lastMessageMarker.clear();
    m_chatTitle->setText(title);
    m_mainStack->setCurrentWidget(m_chatPage);
    refreshMessages();
}

void ChatWindow::refreshMessages() {
    if (m_conversationId.isEmpty() || m_messagesLoading || m_api.token().isEmpty()) return;
    const QString id = m_conversationId;
    m_messagesLoading = true;
    m_api.get(QStringLiteral("/conversations/") + id + QStringLiteral("/messages?limit=50"),
              [this, id](const QJsonObject& data, const QString& error) {
        m_messagesLoading = false;
        if (id != m_conversationId) return;
        if (!error.isEmpty()) { m_status->setText(error); return; }
        const QJsonArray messages = data.value(QStringLiteral("messages")).toArray();
        const QString lastId = messages.isEmpty() ? QStringLiteral("empty")
            : QString::number(messages.last().toObject().value(QStringLiteral("id")).toInteger());
        const QString marker = lastId + QStringLiteral("/") + QString::number(messages.size());
        if (m_lastMessageMarker == marker) return;
        m_lastMessageMarker = marker;
        renderMessages(messages);
        m_api.post(QStringLiteral("/conversations/") + id + QStringLiteral("/read"), {},
                   [](const QJsonObject&, const QString&) {});
    });
}

void ChatWindow::renderMessages(const QJsonArray& messages) {
    clearLayout(m_messageLayout);
    if (messages.isEmpty()) {
        auto* empty = label(QStringLiteral("还没有消息。发送第一条消息吧。"),
                            QStringLiteral("hint"), m_history->widget());
        empty->setAlignment(Qt::AlignCenter);
        m_messageLayout->addWidget(empty);
    }
    for (const QJsonValue& value : messages) {
        const QJsonObject message = value.toObject();
        const bool mine = message.value(QStringLiteral("senderId")).toString() == m_userId;
        auto* row = new QWidget(m_history->widget());
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(8, 0, 8, 0);
        if (mine) rowLayout->addStretch(1);
        auto* bubble = new QFrame(row);
        bubble->setMaximumWidth(510);
        bubble->setObjectName(QStringLiteral("bubble"));
        const QString color = mine
            ? (m_dark ? QStringLiteral("#184c7e") : QStringLiteral("#d4edff"))
            : (m_dark ? QStringLiteral("#293444") : QStringLiteral("#ffffff"));
        bubble->setStyleSheet(QStringLiteral("QFrame#bubble { background: %1; border-radius: 12px; }")
                              .arg(color));
        auto* content = new QVBoxLayout(bubble);
        content->setContentsMargins(15, 11, 15, 12);
        content->setSpacing(5);
        auto* author = label(message.value(QStringLiteral("senderName")).toString(),
                             QStringLiteral("hint"), bubble);
        content->addWidget(author);
        auto* body = new QLabel(bubble);
        body->setTextFormat(Qt::PlainText);  // Never interpret untrusted message text as HTML.
        body->setText(message.value(QStringLiteral("body")).toString());
        body->setWordWrap(true);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->setStyleSheet(QStringLiteral("background:transparent; color:%1;")
            .arg(m_dark ? QStringLiteral("#f2f6fb") : QStringLiteral("#223047")));
        content->addWidget(body);
        rowLayout->addWidget(bubble);
        if (!mine) rowLayout->addStretch(1);
        m_messageLayout->addWidget(row);
    }
    m_messageLayout->addStretch(1);
    QTimer::singleShot(0, m_history, [this] {
        if (auto* bar = m_history->verticalScrollBar()) bar->setValue(bar->maximum());
    });
}

void ChatWindow::sendMessage() {
    const QString text = m_composer->toPlainText().trimmed();
    if (m_conversationId.isEmpty() || text.isEmpty()) return;
    if (text.size() > 4000) {
        QMessageBox::warning(this, QStringLiteral("消息太长"), QStringLiteral("不能超过 4000 字符"));
        return;
    }
    m_composer->setEnabled(false);
    const QString id = m_conversationId;
    m_api.post(QStringLiteral("/conversations/") + id + QStringLiteral("/messages"),
               QJsonObject{{QStringLiteral("text"), text},
                           {QStringLiteral("clientId"), QUuid::createUuid().toString(QUuid::WithoutBraces)}},
               [this, text, id](const QJsonObject&, const QString& error) {
        m_composer->setEnabled(true);
        if (!error.isEmpty()) { showError(error); return; }
        if (m_conversationId == id && m_composer->toPlainText().trimmed() == text)
            m_composer->clear();
        m_lastMessageMarker.clear();
        refreshMessages();
        refreshList();
    });
}

void ChatWindow::addFriend() {
    bool ok = false;
    const QString username = QInputDialog::getText(this, QStringLiteral("添加好友"),
        QStringLiteral("请输入对方用户名"), QLineEdit::Normal, {}, &ok).trimmed();
    if (!ok || username.isEmpty()) return;
    m_api.post(QStringLiteral("/friend-requests"),
               QJsonObject{{QStringLiteral("username"), username}},
               [this](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) { showError(error); return; }
        QMessageBox::information(this, QStringLiteral("已发送"), QStringLiteral("好友申请已发送"));
    });
}

void ChatWindow::handleRequests() {
    m_api.get(QStringLiteral("/friend-requests"), [this](const QJsonObject& data, const QString& error) {
        if (!error.isEmpty()) { showError(error); return; }
        const QJsonArray requests = data.value(QStringLiteral("requests")).toArray();
        if (requests.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("好友申请"), QStringLiteral("暂无待处理申请"));
            return;
        }
        QStringList choices;
        for (const QJsonValue& request : requests) {
            const QJsonObject user = request.toObject().value(QStringLiteral("from")).toObject();
            choices.append(userDisplay(user) + QStringLiteral(" (@")
                           + user.value(QStringLiteral("username")).toString() + QStringLiteral(")"));
        }
        bool selected = false;
        const QString choice = QInputDialog::getItem(this, QStringLiteral("好友申请"),
            QStringLiteral("选择要处理的申请"), choices, 0, false, &selected);
        if (!selected) return;
        const int index = choices.indexOf(choice);
        if (index < 0) return;
        const int answer = QMessageBox::question(this, QStringLiteral("通过申请"),
            QStringLiteral("接受来自 %1 的好友申请？").arg(choice),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (answer == QMessageBox::Cancel) return;
        const QString action = answer == QMessageBox::Yes ? QStringLiteral("accept") : QStringLiteral("reject");
        const QString id = QString::number(requests.at(index).toObject().value(QStringLiteral("id")).toInteger());
        m_api.post(QStringLiteral("/friend-requests/") + id + QStringLiteral("/") + action, {},
                   [this](const QJsonObject&, const QString& error) {
            if (!error.isEmpty()) { showError(error); return; }
            refreshList();
        });
    });
}

void ChatWindow::createGroup() {
    m_api.get(QStringLiteral("/contacts"), [this](const QJsonObject& data, const QString& error) {
        if (!error.isEmpty()) { showError(error); return; }
        const QJsonArray contacts = data.value(QStringLiteral("contacts")).toArray();
        if (contacts.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("创建群聊"), QStringLiteral("请先添加至少一名好友"));
            return;
        }
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("创建群聊"));
        dialog.setMinimumSize(360, 390);
        auto* form = new QVBoxLayout(&dialog);
        form->addWidget(label(QStringLiteral("群聊名称"), QStringLiteral("hint"), &dialog));
        auto* title = new QLineEdit(&dialog);
        title->setMaxLength(48);
        form->addWidget(title);
        auto* users = new QListWidget(&dialog);
        users->setSelectionMode(QAbstractItemView::MultiSelection);
        for (const QJsonValue& value : contacts) {
            const QJsonObject person = value.toObject();
            auto* item = new QListWidgetItem(userDisplay(person), users);
            item->setData(Qt::UserRole, person.value(QStringLiteral("id")).toString());
        }
        form->addWidget(users);
        auto* controls = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        form->addWidget(controls);
        connect(controls, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(controls, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return;
        QJsonArray members;
        for (QListWidgetItem* item : users->selectedItems()) members.append(item->data(Qt::UserRole).toString());
        if (title->text().trimmed().isEmpty() || members.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("资料不完整"), QStringLiteral("请输入群名称并选择成员"));
            return;
        }
        m_api.post(QStringLiteral("/conversations/groups"),
                   QJsonObject{{QStringLiteral("title"), title->text().trimmed()},
                               {QStringLiteral("memberIds"), members}},
                   [this, name = title->text().trimmed()](const QJsonObject& result, const QString& err) {
            if (!err.isEmpty()) { showError(err); return; }
            switchView(View::Conversations);
            openConversation(result.value(QStringLiteral("conversationId")).toString(), name);
        });
    });
}

void ChatWindow::showMoments(const QJsonArray& moments) {
    clearLayout(m_momentsLayout);
    for (const QJsonValue& entry : moments) {
        const QJsonObject moment = entry.toObject();
        auto* frame = new fluent::layout::Card(m_momentsPage);
        auto* card = new QVBoxLayout(frame);
        card->setContentsMargins(20, 17, 20, 18);
        const QString author = moment.value(QStringLiteral("displayName")).toString();
        card->addWidget(label(author, QStringLiteral("panelTitle"), frame));
        auto* text = label(moment.value(QStringLiteral("body")).toString(), QStringLiteral("moment"), frame);
        text->setTextFormat(Qt::PlainText);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        card->addWidget(text);
        auto* like = button(QStringLiteral("%1  ·  %2 赞").arg(
            moment.value(QStringLiteral("liked")).toBool() ? QStringLiteral("取消赞") : QStringLiteral("点赞"))
            .arg(moment.value(QStringLiteral("likes")).toInt()), frame);
        card->addWidget(like, 0, Qt::AlignLeft);
        const auto id = moment.value(QStringLiteral("id")).toInteger();
        connect(like, &QPushButton::clicked, this, [this, id] {
            m_api.post(QStringLiteral("/moments/") + QString::number(id) + QStringLiteral("/like"), {},
                       [this](const QJsonObject&, const QString& error) {
                if (!error.isEmpty()) showError(error);
                refreshList();
            });
        });
        m_momentsLayout->addWidget(frame);
    }
    m_momentsLayout->addStretch();
}

void ChatWindow::publishMoment() {
    bool accepted = false;
    const QString text = QInputDialog::getMultiLineText(this, QStringLiteral("发表动态"),
        QStringLiteral("分享一些新鲜事（最多 1000 字）"), {}, &accepted).trimmed();
    if (!accepted || text.isEmpty()) return;
    if (text.size() > 1000) {
        QMessageBox::warning(this, QStringLiteral("内容太长"), QStringLiteral("动态不能超过 1000 字"));
        return;
    }
    m_api.post(QStringLiteral("/moments"), QJsonObject{{QStringLiteral("text"), text}},
               [this](const QJsonObject&, const QString& error) {
        if (!error.isEmpty()) { showError(error); return; }
        refreshList();
    });
}

void ChatWindow::signOut() {
    if (m_api.token().isEmpty()) return;
    m_poll->stop();
    m_api.post(QStringLiteral("/auth/logout"), {}, [this](const QJsonObject&, const QString&) {
        m_api.clearToken();
        m_userId.clear();
        m_conversationId.clear();
        m_lastMessageMarker.clear();
        m_list->clear();
        m_mainStack->setCurrentWidget(m_blankPage);
        authenticate();
    });
}
