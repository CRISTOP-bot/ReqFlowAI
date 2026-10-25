#include "reqflow/ui/authdialog.h"

#include <QFrame>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace reqflow::ui {

AuthDialog::AuthDialog(data::ProjectStore &store, bool registration, QWidget *parent)
    : QDialog(parent), store_(store), registration_(registration) {
    setWindowTitle(registration_ ? QStringLiteral("Configurar ReqFlow AI") : QStringLiteral("Iniciar sesión · ReqFlow AI"));
    setModal(true);
    setMinimumWidth(440);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setStyleSheet(QStringLiteral(
        "QDialog{background:#171816;color:#f0eee9;}"
        "QFrame#card{background:#222320;border:1px solid #373832;border-radius:14px;}"
        "QLabel{color:#f0eee9;} QLabel#eyebrow{color:#c77d5b;font-size:11px;font-weight:700;letter-spacing:2px;}"
        "QLabel#subtitle{color:#a7a69f;} QLabel#hint{color:#aaa99f;font-size:11px;} QLabel#localnote{color:#8da48b;font-size:11px;}"
        "QLineEdit{background:#191a18;color:#f0eee9;border:1px solid #41423b;border-radius:7px;padding:11px 12px;selection-background-color:#a85f44;}"
        "QLineEdit:focus{border:1px solid #bd7557;}"
        "QPushButton{background:#a85f44;color:#fffaf5;border:0;border-radius:7px;padding:11px 16px;font-weight:600;}"
        "QPushButton:hover{background:#bb7052;} QPushButton:pressed{background:#884d38;}"
        "QPushButton#cancel{background:#30312d;color:#dedbd3;} QPushButton#cancel:hover{background:#3c3d38;}"
        "QLabel#error{color:#e7a18b;background:#382923;border:1px solid #704632;border-radius:6px;padding:8px;}"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(26, 24, 26, 24);
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(13);

    auto *eyebrow = new QLabel(QStringLiteral("REQFLOW  /  AI"), card);
    eyebrow->setObjectName(QStringLiteral("eyebrow"));
    layout->addWidget(eyebrow);
    auto *title = new QLabel(registration_ ? QStringLiteral("Tu espacio, en este equipo") : QStringLiteral("Qué bueno verte"), card);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    layout->addWidget(title);
    auto *subtitle = new QLabel(registration_
        ? QStringLiteral("Crea una cuenta local para abrir ReqFlow AI.")
        : QStringLiteral("Ingresa tus datos para continuar."), card);
    subtitle->setObjectName(QStringLiteral("subtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);
    auto *localNote = new QLabel(QStringLiteral("●  Cuenta local · Solo en este dispositivo"), card);
    localNote->setObjectName(QStringLiteral("localnote"));
    layout->addWidget(localNote);

    auto addField = [card, layout](const QString &caption, const QString &placeholder, QLineEdit **target, QLineEdit::EchoMode mode) {
        auto *label = new QLabel(caption, card);
        layout->addWidget(label);
        auto *edit = new QLineEdit(card);
        edit->setPlaceholderText(placeholder);
        edit->setEchoMode(mode);
        edit->setClearButtonEnabled(mode == QLineEdit::Normal);
        edit->setMinimumHeight(42);
        label->setBuddy(edit);
        layout->addWidget(edit);
        *target = edit;
    };
    addField(QStringLiteral("Usuario"), QStringLiteral("Tu nombre de usuario"), &username_, QLineEdit::Normal);
    addField(QStringLiteral("Contraseña"), QStringLiteral("••••••••••••"), &password_, QLineEdit::Password);
    if (registration_)
        addField(QStringLiteral("Confirma tu contraseña"), QStringLiteral("Escríbela de nuevo"), &confirmation_, QLineEdit::Password);
    auto *hint = new QLabel(registration_
        ? QStringLiteral("Usa al menos 12 caracteres. No hay recuperación de contraseña; conserva una copia segura.")
        : QStringLiteral("No hay recuperación de contraseña. El acceso al archivo SQLite no está cifrado por este login."), card);
    hint->setObjectName(QStringLiteral("hint"));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    error_ = new QLabel(card);
    error_->setObjectName(QStringLiteral("error"));
    error_->setWordWrap(true);
    error_->hide();
    layout->addWidget(error_);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = new QPushButton(QStringLiteral("Cancelar"), card);
    cancel->setObjectName(QStringLiteral("cancel"));
    cancel->setAutoDefault(false);
    buttons->addWidget(cancel);
    submitButton_ = new QPushButton(registration_ ? QStringLiteral("Crear cuenta") : QStringLiteral("Entrar"), card);
    submitButton_->setDefault(true);
    buttons->addWidget(submitButton_);
    layout->addLayout(buttons);
    outer->addWidget(card);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(submitButton_, &QPushButton::clicked, this, &AuthDialog::submit);
    connect(username_, &QLineEdit::returnPressed, this, [this] { password_->setFocus(); });
    connect(password_, &QLineEdit::returnPressed, this, &AuthDialog::submit);
    if (confirmation_) connect(confirmation_, &QLineEdit::returnPressed, this, &AuthDialog::submit);
    username_->setFocus();
}

void AuthDialog::submit() {
    error_->hide();
    if (registration_) {
        if (password_->text() != confirmation_->text()) {
            error_->setText(QStringLiteral("Las contraseñas no coinciden."));
            error_->setAccessibleName(error_->text());
            error_->show();
            confirmation_->setFocus();
            return;
        }
        if (!store_.createLocalAccount(username_->text(), password_->text())) {
            error_->setText(store_.localAccountError());
            error_->setAccessibleName(error_->text());
            error_->show();
            password_->clear();
            confirmation_->clear();
            password_->setFocus();
            return;
        }
    } else if (!store_.verifyLocalAccount(username_->text(), password_->text())) {
        error_->setText(store_.localAccountError());
        error_->show();
        password_->clear();
        password_->setFocus();
        return;
    }
    password_->clear();
    if (confirmation_) confirmation_->clear();
    accept();
}

} // namespace reqflow::ui
