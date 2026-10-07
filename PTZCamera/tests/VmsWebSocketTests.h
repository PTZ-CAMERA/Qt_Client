#pragma once
#include <QObject>
class VmsWebSocketTests : public QObject {
    Q_OBJECT
private slots:
    void dummyConnectAndDiscoveryButtons();
    void connectionQueriesNotificationsAndErrors();
    void reconnectAndRequestTimeout();
    void uiClearsRealStateWhenDisconnectedOrDummyEnabled();
    void realVmsServer();
    void ptzAcceptanceAndPiConfirmationAreSeparate();
};
