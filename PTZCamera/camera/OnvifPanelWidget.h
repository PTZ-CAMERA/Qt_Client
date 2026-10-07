#pragma once

#include <QUrl>
#include <QWidget>

class QLabel;
class QComboBox;
class QLineEdit;
class OnvifClient;

class OnvifPanelWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OnvifPanelWidget(QWidget *parent = nullptr);
    void fetchStreamUri(bool useUdp);

signals:
    void fetchRequested();
    void streamUriReady(const QUrl &uri);
    void message(const QString &text);

private:
    OnvifClient *m_client = nullptr;
    QComboBox *m_devices = nullptr;
    QLineEdit *m_serviceUrl = nullptr;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_password = nullptr;
    QLineEdit *m_streamUrl = nullptr;
    QLabel *m_status = nullptr;
};
