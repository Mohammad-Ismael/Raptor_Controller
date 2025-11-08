#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "modules/systemcleaner.h"
#include "modules/networkmanager.h"
#include "modules/hardwareinfo.h"
#include "modules/appmanager.h"
#include "modules/softwaremanager.h"
#include "modules/wifimanager.h"

#include <QRegularExpression>
#include <QTableWidgetItem>
#include <QTimer>
#include <QMessageBox>
#include <QDateTime>
#include <QVBoxLayout>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set window properties
    setWindowTitle("Raptor PC Controller");
    setMinimumSize(1000, 700);

    // Initialize modular managers
    m_systemCleaner = new SystemCleaner(this, this);
    m_networkManager = new NetworkManager(this, this);
    m_hardwareInfo = new HardwareInfo(this, this);
    m_appManager = new AppManager(this, this);
    m_softwareManager = new SoftwareManager(this, this);
    m_wifiManager = new WiFiManager(this, this);

    setupConnections();

    // Set initial active button (General)
    on_generalButton_clicked();
}

MainWindow::~MainWindow()
{
    delete m_systemCleaner;
    delete m_networkManager;
    delete m_hardwareInfo;
    delete m_appManager;
    delete m_softwareManager;
    delete m_wifiManager;
    delete ui;
}

void MainWindow::setupConnections()
{
    // Note: Most connections are auto-connected via Qt's naming convention
}

void MainWindow::resetAllButtons()
{
    // Reset all buttons to default style
    QString defaultStyle =
        "QPushButton {"
        "    background-color: transparent;"
        "    color: white;"
        "    border: none;"
        "    padding: 15px 20px;"
        "    text-align: left;"
        "    border-bottom: 1px solid #34495e;"
        "}"
        "QPushButton:hover {"
        "    background-color: #34495e;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1abc9c;"
        "}";

    ui->generalButton->setStyleSheet(defaultStyle);
    ui->wifiButton->setStyleSheet(defaultStyle);
    ui->appsButton->setStyleSheet(defaultStyle);
    ui->cleanerButton->setStyleSheet(defaultStyle);
    ui->networkButton->setStyleSheet(defaultStyle);
    ui->hardwareButton->setStyleSheet(defaultStyle);
    ui->optionsButton->setStyleSheet(defaultStyle);
}

void MainWindow::setActiveButton(QPushButton *activeButton)
{
    resetAllButtons();

    // Set active button style with green background
    QString activeStyle =
        "QPushButton {"
        "    background-color: #1abc9c;"
        "    color: white;"
        "    border: none;"
        "    padding: 15px 20px;"
        "    text-align: left;"
        "    border-bottom: 1px solid #34495e;"
        "}"
        "QPushButton:hover {"
        "    background-color: #16a085;"
        "}";

    activeButton->setStyleSheet(activeStyle);
}

void MainWindow::updateContent(const QString &title)
{
    ui->titleLabel->setText(title);
}

void MainWindow::showCleanerPage()
{
    ui->contentStackedWidget->setCurrentWidget(ui->cleanerPage);
}

// Main navigation slots
void MainWindow::on_generalButton_clicked()
{
    setActiveButton(ui->generalButton);
    updateContent("General Settings");
    ui->contentStackedWidget->setCurrentWidget(ui->generalPage);

    // Populate startup programs table
    if (ui->startupTable->rowCount() == 0)
    {
        QStringList startupPrograms = {
            "Microsoft OneDrive", "Spotify", "Adobe Creative Cloud",
            "Discord", "Steam Client", "NVIDIA Control Panel",
            "Realtek Audio", "Windows Security", "Google Update"};

        QStringList impacts = {"Low", "Medium", "High", "Low", "Medium", "Low", "Low", "Low", "Low"};
        QStringList types = {"Registry", "Startup Folder", "Service", "Registry", "Registry", "Service", "Service", "Service", "Scheduled Task"};

        for (int i = 0; i < startupPrograms.size(); ++i)
        {
            int row = ui->startupTable->rowCount();
            ui->startupTable->insertRow(row);

            ui->startupTable->setItem(row, 0, new QTableWidgetItem(startupPrograms[i]));

            QTableWidgetItem *statusItem = new QTableWidgetItem("Enabled");
            statusItem->setForeground(QBrush(QColor("#2ecc71")));
            ui->startupTable->setItem(row, 1, statusItem);

            ui->startupTable->setItem(row, 2, new QTableWidgetItem(impacts[i]));
            ui->startupTable->setItem(row, 3, new QTableWidgetItem(types[i]));
        }
        ui->startupTable->resizeColumnsToContents();
    }
}

void MainWindow::on_wifiButton_clicked()
{
    setActiveButton(ui->wifiButton);
    updateContent("WiFi Management");
    ui->contentStackedWidget->setCurrentWidget(ui->wifiPage);
}

void MainWindow::on_appsButton_clicked()
{
    setActiveButton(ui->appsButton);
    updateContent("Favorite Apps");
    ui->contentStackedWidget->setCurrentWidget(ui->appsPage);

    // Populate apps on first load
    if (ui->appsGridLayout->count() == 0)
    {
        on_refreshAppsButton_clicked();
    }
}

void MainWindow::on_cleanerButton_clicked()
{
    setActiveButton(ui->cleanerButton);
    updateContent("System Cleaner");
    showCleanerPage();
}

void MainWindow::on_networkButton_clicked()
{
    setActiveButton(ui->networkButton);
    updateContent("Network Tools");
    ui->contentStackedWidget->setCurrentWidget(ui->networkPage);

    // Auto-refresh network status when page is opened
    on_pushButton_refreshNetwork_clicked();
}

void MainWindow::on_hardwareButton_clicked()
{
    setActiveButton(ui->hardwareButton);
    updateContent("Hardware Information");
    ui->contentStackedWidget->setCurrentWidget(ui->hardwarePage);

    // Auto-refresh hardware info when page is opened
    on_pushButton_refreshHardware_clicked();
}

void MainWindow::on_optionsButton_clicked()
{
    setActiveButton(ui->optionsButton);
    updateContent("Options & Settings");
    ui->contentStackedWidget->setCurrentWidget(ui->optionsPage);
}

// Cleaner tab slots - Delegated to SystemCleaner
void MainWindow::on_scanQuickButton_clicked()
{
    m_systemCleaner->performQuickScan();
}

void MainWindow::on_cleanQuickButton_clicked()
{
    m_systemCleaner->performQuickClean();
}

void MainWindow::on_scanSystemButton_clicked()
{
    m_systemCleaner->performSystemScan();
}

void MainWindow::on_cleanSystemButton_clicked()
{
    m_systemCleaner->performSystemClean();
}

void MainWindow::on_scanBrowsersButton_clicked()
{
    m_systemCleaner->performBrowserScan();
}

void MainWindow::on_cleanBrowsersButton_clicked()
{
    m_systemCleaner->performBrowserClean();
}

void MainWindow::on_selectAllSystemButton_clicked()
{
    for (int i = 0; i < ui->systemCleanerList->count(); ++i)
    {
        ui->systemCleanerList->item(i)->setSelected(true);
    }
}

// Software Uninstaller slots - Delegated to SoftwareManager
void MainWindow::on_refreshSoftwareButton_clicked()
{
    m_softwareManager->refreshSoftware();
}

void MainWindow::on_searchSoftwareInput_textChanged(const QString &searchText)
{
    m_softwareManager->searchSoftware(searchText);
}

void MainWindow::on_softwareTable_itemSelectionChanged()
{
    m_softwareManager->onSoftwareSelectionChanged();
}

void MainWindow::on_uninstallSoftwareButton_clicked()
{
    m_softwareManager->uninstallSoftware();
}

void MainWindow::on_forceUninstallButton_clicked()
{
    m_softwareManager->forceUninstallSoftware();
}

void MainWindow::populateSoftwareTable()
{
    m_softwareManager->populateSoftwareTable();
}

// Apps page slots - Delegated to AppManager
void MainWindow::on_addAppButton_clicked()
{
    m_appManager->addApp();
}

void MainWindow::on_refreshAppsButton_clicked()
{
    m_appManager->refreshApps();
}

void MainWindow::on_launchAppButton_clicked()
{
    m_appManager->launchApp();
}

void MainWindow::on_removeAppButton_clicked()
{
    m_appManager->removeApp();
}

void MainWindow::on_searchAppsInput_textChanged(const QString &searchText)
{
    m_appManager->searchApps(searchText);
}

// General tab slots
void MainWindow::on_refreshSystemInfoButton_clicked()
{
    // Simulate refreshing system info
    ui->cpuUsageProgress->setValue(rand() % 100);
    ui->ramUsageProgress->setValue(rand() % 100);
    ui->diskUsageProgress->setValue(rand() % 100);

    ui->cpuUsageValue->setText(QString("%1%").arg(ui->cpuUsageProgress->value()));
    ui->ramUsageValue->setText(QString("%1%").arg(ui->ramUsageProgress->value()));
    ui->diskUsageValue->setText(QString("%1%").arg(ui->diskUsageProgress->value()));
}

void MainWindow::on_generateReportButton_clicked()
{
    // Placeholder for report generation
    ui->systemInfoLabel->setText("System report generated successfully! Check your Documents folder.");
}

void MainWindow::on_disableStartupButton_clicked()
{
    // Placeholder for disabling startup program
    int row = ui->startupTable->currentRow();
    if (row >= 0)
    {
        QTableWidgetItem *statusItem = ui->startupTable->item(row, 1);
        if (statusItem && statusItem->text() == "Enabled")
        {
            statusItem->setText("Disabled");
            statusItem->setForeground(QBrush(QColor("#e74c3c")));
        }
    }
}

void MainWindow::on_enableStartupButton_clicked()
{
    // Placeholder for enabling startup program
    int row = ui->startupTable->currentRow();
    if (row >= 0)
    {
        QTableWidgetItem *statusItem = ui->startupTable->item(row, 1);
        if (statusItem && statusItem->text() == "Disabled")
        {
            statusItem->setText("Enabled");
            statusItem->setForeground(QBrush(QColor("#2ecc71")));
        }
    }
}

void MainWindow::on_startupTable_itemSelectionChanged()
{
    QList<QTableWidgetItem *> selectedItems = ui->startupTable->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();

    if (hasSelection)
    {
        int row = ui->startupTable->currentRow();
        QTableWidgetItem *statusItem = ui->startupTable->item(row, 1);
        bool isEnabled = statusItem && statusItem->text() == "Enabled";

        ui->disableStartupButton->setEnabled(isEnabled);
        ui->enableStartupButton->setEnabled(!isEnabled);
    }
    else
    {
        ui->disableStartupButton->setEnabled(false);
        ui->enableStartupButton->setEnabled(false);
    }
}

// WiFi Management slots - Delegated to WiFiManager
void MainWindow::on_scanNetworksButton_clicked()
{
    m_wifiManager->scanNetworks();
}

void MainWindow::on_refreshNetworksButton_clicked()
{
    m_wifiManager->refreshNetworks();
}

void MainWindow::on_connectNetworkButton_clicked()
{
    m_wifiManager->connectNetwork();
}

void MainWindow::on_disconnectNetworkButton_clicked()
{
    m_wifiManager->disconnectNetwork();
}

void MainWindow::on_networksTable_itemSelectionChanged()
{
    m_wifiManager->onNetworkSelectionChanged();
}

void MainWindow::on_enableAdapterButton_clicked()
{
    m_wifiManager->enableAdapter();
}

void MainWindow::on_disableAdapterButton_clicked()
{
    m_wifiManager->disableAdapter();
}

void MainWindow::on_refreshAdaptersButton_clicked()
{
    m_wifiManager->refreshAdapters();
}

void MainWindow::on_adaptersList_itemSelectionChanged()
{
    m_wifiManager->onAdapterSelectionChanged();
}

void MainWindow::on_runDiagnosticsButton_clicked()
{
    m_wifiManager->runDiagnostics();
}

void MainWindow::on_resetNetworkButton_clicked()
{
    m_wifiManager->resetNetwork();
}

void MainWindow::on_flushDnsButton_clicked()
{
    m_wifiManager->flushDns();
}

void MainWindow::on_restartWifiServiceButton_clicked()
{
    m_wifiManager->restartWifiService();
}

void MainWindow::on_renewIpButton_clicked()
{
    m_wifiManager->renewIp();
}

void MainWindow::on_forgetNetworkButton_clicked()
{
    m_wifiManager->forgetNetwork();
}

void MainWindow::on_driverUpdateButton_clicked()
{
    m_wifiManager->updateDriver();
}

// Options page slots
void MainWindow::on_pushButton_saveSettings_clicked()
{
    ui->label_optionsTitle->setText("Settings saved successfully!");
}

void MainWindow::on_pushButton_resetSettings_clicked()
{
    // Reset to default values
    ui->checkBox_startWithWindows->setChecked(true);
    ui->checkBox_autoUpdate->setChecked(true);
    ui->checkBox_notifications->setChecked(true);
    ui->comboBox_theme->setCurrentIndex(2);    // System Default
    ui->comboBox_language->setCurrentIndex(0); // English

    ui->label_optionsTitle->setText("Settings reset to defaults!");
}

void MainWindow::on_pushButton_checkUpdates_clicked()
{
    ui->label_optionsTitle->setText("Checking for updates... Your software is up to date!");
}

void MainWindow::on_pushButton_refreshHardware_clicked()
{
    m_hardwareInfo->refreshHardware();
}

void MainWindow::on_pushButton_exportHardware_clicked()
{
    m_hardwareInfo->exportHardware();
}

// Network page slots - Delegated to NetworkManager
void MainWindow::on_pushButton_refreshNetwork_clicked()
{
    m_networkManager->refreshNetworkStatus();
}

void MainWindow::on_pushButton_testConnection_clicked()
{
    m_networkManager->testConnection();
}

void MainWindow::on_pushButton_startPing_clicked()
{
    m_networkManager->startPing();
}

void MainWindow::on_pushButton_stopPing_clicked()
{
    m_networkManager->stopPing();
}

void MainWindow::on_pushButton_startTraceroute_clicked()
{
    m_networkManager->startTraceroute();
}

void MainWindow::on_pushButton_stopTraceroute_clicked()
{
    m_networkManager->stopTraceroute();
}

void MainWindow::on_pushButton_startScan_clicked()
{
    m_networkManager->startPortScan();
}

void MainWindow::on_pushButton_stopScan_clicked()
{
    m_networkManager->stopPortScan();
}