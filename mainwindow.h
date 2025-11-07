#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Main navigation slots
    void on_generalButton_clicked();
    void on_wifiButton_clicked();
    void on_appsButton_clicked();
    void on_cleanerButton_clicked();
    void on_networkButton_clicked();
    void on_hardwareButton_clicked();
    void on_optionsButton_clicked();

    // Cleaner tab slots
    void on_scanQuickButton_clicked();
    void on_cleanQuickButton_clicked();
    void on_scanSystemButton_clicked();
    void on_cleanSystemButton_clicked();
    void on_selectAllSystemButton_clicked();
    void on_scanBrowsersButton_clicked();
    void on_cleanBrowsersButton_clicked();

    // Software Uninstaller slots
    void on_refreshSoftwareButton_clicked();
    void on_searchSoftwareInput_textChanged(const QString &text);
    void on_softwareTable_itemSelectionChanged();
    void on_uninstallSoftwareButton_clicked();
    void on_forceUninstallButton_clicked();

private:
    Ui::MainWindow *ui;
    void setupConnections();
    void updateContent(const QString &title);
    void setActiveButton(QPushButton *activeButton);
    void resetAllButtons();
    void showCleanerPage();
    void populateSoftwareTable();
    // General tab slots
    void on_refreshSystemInfoButton_clicked();
    void on_generateReportButton_clicked();
    void on_disableStartupButton_clicked();
    void on_enableStartupButton_clicked();
    void on_startupTable_itemSelectionChanged();
    void on_saveSettingsButton_clicked();
    void on_resetSettingsButton_clicked();

    // WiFi Management slots
    void on_scanNetworksButton_clicked();
    void on_refreshNetworksButton_clicked();
    void on_connectNetworkButton_clicked();
    void on_disconnectNetworkButton_clicked();
    void on_networksTable_itemSelectionChanged();
    void on_enableAdapterButton_clicked();
    void on_disableAdapterButton_clicked();
    void on_refreshAdaptersButton_clicked();
    void on_adaptersList_itemSelectionChanged();
    void on_runDiagnosticsButton_clicked();
    void on_resetNetworkButton_clicked(); // Make sure this matches
    void on_flushDnsButton_clicked();
    void on_restartWifiServiceButton_clicked();
    void on_renewIpButton_clicked();
    void on_forgetNetworkButton_clicked();
    void on_driverUpdateButton_clicked();

    // Apps page slots
    void on_addAppButton_clicked();
    void on_refreshAppsButton_clicked();
    void on_launchAppButton_clicked();
    void on_removeAppButton_clicked();
    void on_searchAppsInput_textChanged(const QString &text);

    // Options page slots
    void on_checkUpdatesButton_clicked();
    void on_exportSettingsButton_clicked();
    void on_importSettingsButton_clicked();
    // Options page slots
    void on_pushButton_saveSettings_clicked();
    void on_pushButton_resetSettings_clicked();
    void on_pushButton_checkUpdates_clicked();
    void on_pushButton_refreshNetwork_clicked();
    void on_pushButton_testConnection_clicked();
    void on_pushButton_startPing_clicked();
    void on_pushButton_stopPing_clicked();
    void simulatePing();
    void on_pushButton_startTraceroute_clicked();
    // Hardware page slots
    void on_pushButton_refreshHardware_clicked();
    void on_pushButton_exportHardware_clicked();

    void on_pushButton_stopTraceroute_clicked();
    void simulateTraceroute();
    void on_pushButton_startScan_clicked();
    void simulatePortScan();
    void on_pushButton_stopScan_clicked();

    QTimer *m_pingTimer;
    QTimer *m_tracerouteTimer;
    QTimer *m_scanTimer;
    int m_pingCount;
    int m_tracerouteHop;
    int m_currentScanPort;
    int m_scanEndPort;
    int m_openPortsFound;
};
#endif // MAINWINDOW_H
