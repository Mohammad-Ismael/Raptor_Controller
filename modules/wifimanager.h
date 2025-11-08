#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <QObject>

class MainWindow;

class WiFiManager : public QObject
{
    Q_OBJECT

public:
    explicit WiFiManager(MainWindow *mainWindow, QObject *parent = nullptr);
    
    void scanNetworks();
    void refreshNetworks();
    void connectNetwork();
    void disconnectNetwork();
    void onNetworkSelectionChanged();
    void enableAdapter();
    void disableAdapter();
    void refreshAdapters();
    void onAdapterSelectionChanged();
    void runDiagnostics();
    void resetNetwork();
    void flushDns();
    void restartWifiService();
    void renewIp();
    void forgetNetwork();
    void updateDriver();

private:
    MainWindow *m_mainWindow;
};

#endif // WIFIMANAGER_H