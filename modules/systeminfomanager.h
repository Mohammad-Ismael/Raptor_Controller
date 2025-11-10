#ifndef SYSTEMINFOMANAGER_H
#define SYSTEMINFOMANAGER_H

#include <QObject>
#include <QTimer>
#include <QFutureWatcher>
#include <windows.h>
#include <dxgi.h>
#include <comdef.h>

class MainWindow;

class SystemInfoManager : public QObject
{
    Q_OBJECT

public:
    explicit SystemInfoManager(MainWindow *mainWindow, QObject *parent = nullptr);
    ~SystemInfoManager();

    void refreshSystemInfo();
    void startRealTimeMonitoring();
    void stopRealTimeMonitoring();
    void generateSystemReport();

signals:
    void systemInfoUpdated(const QString &osInfo, const QString &cpuInfo, const QString &ramInfo,
                           const QString &storageInfo, const QString &gpuInfo);
    void performanceUpdated(int cpuUsage, int ramUsage, int diskUsage);
    void updateFinished(bool success, const QString &message);

private:
    QString getOSInfoFromRegistry();
    QString getDisplayVersion();
    QString getBuildNumber();
    struct SystemSpecs
    {
        QString osName;
        QString osVersion;
        QString cpuName;
        QString cpuCores;
        QString ramTotal;
        QString ramSpeed;
        QString storageTotal;
        QString storageFree;
        QString gpuName;
        QString gpuVRAM;
    };

    MainWindow *m_mainWindow;
    QTimer *m_monitorTimer;
    QFutureWatcher<SystemSpecs> *m_specsWatcher;
    bool m_isMonitoring;

    // For CPU usage calculation
    ULARGE_INTEGER m_lastIdleTime;
    ULARGE_INTEGER m_lastKernelTime;
    ULARGE_INTEGER m_lastUserTime;

    SystemSpecs getSystemSpecs();
    void updatePerformanceMetrics();
    int getCPUUsage();
    int getRAMUsage();
    int getDiskUsage();
    QString getGPUInfo();
    QString getGPUVramInfo();
    QString getRamSpeed();
    QString getBootDriveInfo();
    bool isWindows11();
    
    bool m_initialLoadDone; 

private slots:
    void onSpecsScanFinished();
    void onMonitorTimeout();
};

#endif // SYSTEMINFOMANAGER_H
