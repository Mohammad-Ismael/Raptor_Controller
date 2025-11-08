#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTimer>
#include <QMap>

class MainWindow;

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(MainWindow *mainWindow, QObject *parent = nullptr);
    ~NetworkManager();

    void refreshNetworkStatus();
    void testConnection();
    void startPing();
    void stopPing();
    void startTraceroute();
    void stopTraceroute();
    void startPortScan();
    void stopPortScan();

private slots:
    void simulatePing();
    void simulateTraceroute();
    void simulatePortScan();

private:
    MainWindow *m_mainWindow;
    QTimer *m_pingTimer;
    QTimer *m_tracerouteTimer;
    QTimer *m_scanTimer;
    int m_pingCount;
    int m_tracerouteHop;
    int m_currentScanPort;
    int m_scanEndPort;
    int m_openPortsFound;
};

#endif // NETWORKMANAGER_H