#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableView;
class QStandardItemModel;
class QLabel;
class ChatSearchWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatSearchWidget(QWidget *parent=nullptr);
    void setContext(const QString &cameraId,bool available);
    void beginRequest(const QString &id);
    void acceptResponse(const QString &id,const QJsonObject &data);
    void requestError(const QString &id,const QString &message);
signals:
    void searchRequested(const QString &message);
    void recordPlaybackRequested(const QJsonObject &record);
    void selectedPlaybackReceived(const QJsonObject &playback);
private:
    void submit(const QString &message);
    void openSelected();
    void updateButtons();
    QString m_cameraId,m_requestId;
    bool m_available=false;
    QJsonArray m_records;
    QLineEdit *m_input=nullptr;
    QPlainTextEdit *m_history=nullptr;
    QTableView *m_table=nullptr;
    QStandardItemModel *m_model=nullptr;
    QPushButton *m_send=nullptr,*m_next=nullptr,*m_play=nullptr;
    QLabel *m_status=nullptr;
};
