#pragma once

#include "reqflow/data/projectstore.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

namespace reqflow::ui {

class AuthDialog final : public QDialog {
    Q_OBJECT
public:
    explicit AuthDialog(data::ProjectStore &store, bool registration, QWidget *parent = nullptr);

private slots:
    void submit();

private:
    data::ProjectStore &store_;
    bool registration_ = false;
    QLineEdit *username_ = nullptr;
    QLineEdit *password_ = nullptr;
    QLineEdit *confirmation_ = nullptr;
    QLabel *error_ = nullptr;
    QPushButton *submitButton_ = nullptr;
};

} // namespace reqflow::ui
