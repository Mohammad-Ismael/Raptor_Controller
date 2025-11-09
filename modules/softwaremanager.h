#ifndef SOFTWAREUNINSTALLER_H
#define SOFTWAREUNINSTALLER_H

#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QRegularExpression>
#include <QFutureWatcher>
#include <QAtomicInteger>

class MainWindow;

class SoftwareManager : public QObject
{
    Q_OBJECT

public:
    explicit SoftwareManager(MainWindow *mainWindow, QObject *parent = nullptr);
    ~SoftwareManager();
    
    void refreshSoftware();
    void searchSoftware(const QString &searchText);
    void onSoftwareSelectionChanged();
    void uninstallSoftware();
    void forceUninstallSoftware();
    void populateSoftwareTable();
    void cancelScan();

signals:
    void scanProgressUpdated(int progress, const QString &status);
    void scanFinished(bool success, const QString &message);

private:
    struct InstalledSoftware {
        QString name;
        QString version;
        QString publisher;
        qint64 size;
        QString installDate;
        QString uninstallString;
        QString quietUninstallString;
        QString guid;
        bool isSystemComponent;
    };

    MainWindow *m_mainWindow;
    QList<InstalledSoftware> m_installedSoftware;
    QFutureWatcher<QList<InstalledSoftware>> *m_scanWatcher;
    QAtomicInteger<bool> m_cancelScan;
    
    void scanInstalledSoftware();
    QList<InstalledSoftware> getInstalledSoftwareFromRegistry();
    QString getSoftwareSize(const QString &uninstallPath);
    QString formatFileSize(qint64 bytes);
    void executeUninstall(const QString &uninstallString, bool force = false);
    bool isSoftwareRunning(const QString &softwareName);
    void terminateProcess(const QString &processName);
    
    void setupConnections();
    void onScanFinished();

private slots:
    void updateScanProgress();
};

#endif // SOFTWAREUNINSTALLER_H