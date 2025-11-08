#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QTreeWidgetItem>
#include <QCompleter>  // Add this line
#include <QMouseEvent> // ADD THIS
#include <QEvent>      // ADD THIS

// Include the actual headers instead of forward declarations
#include "modules/systemcleaner.h"
#include "modules/networkmanager.h"
#include "modules/hardwareinfo.h"
#include "modules/appmanager.h"
#include "modules/softwaremanager.h"
#include "modules/wifimanager.h"
#include "modules/fileschecker.h"

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
    void closeEvent(QCloseEvent *event) override;

    // Make UI accessible to modules
    Ui::MainWindow *ui;

private slots:

    void onLargeFilesTableItemClicked(QTableWidgetItem *item);
    bool eventFilter(QObject *obj, QEvent *event);

    void on_largeFilesTable_itemDoubleClicked(QTableWidgetItem *item); // Add this
    // Add these to your existing slots
    void on_filesCheckerButton_clicked();
    void on_refreshDiskSpaceButton_clicked();
    void on_scanLargeFilesButton_clicked();
    void on_openFileLocationButton_clicked();
    void on_deleteLargeFilesButton_clicked();
    void on_scanDuplicateFilesButton_clicked();
    void on_deleteDuplicateFilesButton_clicked();
    void on_largeFilesTable_itemChanged(QTableWidgetItem *item);
    void on_duplicateFilesTree_itemChanged(QTreeWidgetItem *item, int column);

    // Main navigation slots
    void on_generalButton_clicked();
    void on_wifiButton_clicked();
    void on_appsButton_clicked();
    void on_cleanerButton_clicked();
    void on_networkButton_clicked();
    void on_hardwareButton_clicked();
    void on_optionsButton_clicked();

    void onSystemCleanerItemClicked(QListWidgetItem *item);

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
    void on_searchSoftwareInput_textChanged(const QString &searchText);
    void on_softwareTable_itemSelectionChanged();
    void on_uninstallSoftwareButton_clicked();
    void on_forceUninstallButton_clicked();

    // Apps page slots
    void on_addAppButton_clicked();
    void on_refreshAppsButton_clicked();
    void on_launchAppButton_clicked();
    void on_removeAppButton_clicked();
    void on_searchAppsInput_textChanged(const QString &searchText);

    // General tab slots
    void on_refreshSystemInfoButton_clicked();
    void on_generateReportButton_clicked();
    void on_disableStartupButton_clicked();
    void on_enableStartupButton_clicked();
    void on_startupTable_itemSelectionChanged();

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
    void on_resetNetworkButton_clicked();
    void on_flushDnsButton_clicked();
    void on_restartWifiServiceButton_clicked();
    void on_renewIpButton_clicked();
    void on_forgetNetworkButton_clicked();
    void on_driverUpdateButton_clicked();

    // Options page slots
    void on_pushButton_saveSettings_clicked();
    void on_pushButton_resetSettings_clicked();
    void on_pushButton_checkUpdates_clicked();
    void on_pushButton_refreshHardware_clicked();
    void on_pushButton_exportHardware_clicked();

    // Network page slots
    void on_pushButton_refreshNetwork_clicked();
    void on_pushButton_testConnection_clicked();
    void on_pushButton_startPing_clicked();
    void on_pushButton_stopPing_clicked();
    void on_pushButton_startTraceroute_clicked();
    void on_pushButton_stopTraceroute_clicked();
    void on_pushButton_startScan_clicked();
    void on_pushButton_stopScan_clicked();

    void on_cancelLargeFilesButton_clicked();
    void on_cancelDuplicateFilesButton_clicked();
    void on_browseLargeFilesPathButton_clicked();
    void on_browseDuplicateFilesPathButton_clicked();
    void on_largeFilesPathInput_textChanged(const QString &text);
    void on_duplicateFilesPathInput_textChanged(const QString &text);

private:
    // Modular managers
    SystemCleaner *m_systemCleaner;
    NetworkManager *m_networkManager;
    HardwareInfo *m_hardwareInfo;
    AppManager *m_appManager;
    SoftwareManager *m_softwareManager;
    WiFiManager *m_wifiManager;
    FilesChecker *m_filesChecker;

    // Private methods
    void setupConnections();
    void resetAllButtons();
    void setActiveButton(QPushButton *activeButton);
    void updateContent(const QString &title);
    void showCleanerPage();
    void populateSoftwareTable();

    void cancelAllOperations();
    void debugTableState();

    void updateDeleteButtonState();
};

#endif // MAINWINDOW_H
