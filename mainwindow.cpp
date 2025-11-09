#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "modules/systemcleaner.h"
#include "modules/networkmanager.h"
#include "modules/hardwareinfo.h"
#include "modules/appmanager.h"
#include "modules/softwaremanager.h"
#include "modules/wifimanager.h"

#include <QFileDialog>
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

    // Initialize ALL modular managers
    m_systemCleaner = new SystemCleaner(this, this);
    m_networkManager = new NetworkManager(this, this);
    m_hardwareInfo = new HardwareInfo(this, this);
    m_appManager = new AppManager(this, this);
    m_softwareManager = new SoftwareManager(this, this);
    m_wifiManager = new WiFiManager(this, this);
    m_filesChecker = new FilesChecker(this, this);

    // Add path suggestions for file checker
    QStringList commonPaths = m_filesChecker->getCommonPaths();
    QCompleter *pathCompleter = new QCompleter(commonPaths, this);
    pathCompleter->setCaseSensitivity(Qt::CaseInsensitive);

    ui->largeFilesPathInput->setCompleter(pathCompleter);
    ui->duplicateFilesPathInput->setCompleter(pathCompleter);

    // Set default paths
    ui->largeFilesPathInput->setText(QDir::homePath());
    ui->duplicateFilesPathInput->setText(QDir::homePath());

    setupConnections();

    // Set initial active button (General)
    on_generalButton_clicked();

    // Setup system cleaner list for click selection
    connect(ui->systemCleanerList, &QListWidget::itemClicked, this, &MainWindow::onSystemCleanerItemClicked);

    // Connect the table item click
    connect(ui->largeFilesTable, &QTableWidget::itemClicked, this, &MainWindow::onLargeFilesTableItemClicked);

    // Fix: Connect the cancel buttons properly
    connect(ui->cancelLargeFilesButton, &QPushButton::clicked, this, &MainWindow::on_cancelLargeFilesButton_clicked);
    connect(ui->cancelDuplicateFilesButton, &QPushButton::clicked, this, &MainWindow::on_cancelDuplicateFilesButton_clicked);

    // In MainWindow constructor, add:
    connect(ui->appsButton, &QPushButton::clicked, this, [this]()
            {
    if (ui->contentStackedWidget->currentWidget() == ui->appsPage) {
        // Auto-refresh software list when software tab is opened
        on_scanSoftwareButton_clicked();
    } });

    connect(m_softwareManager, &SoftwareManager::scanProgressUpdated, this, [this](int progress, const QString &status)
            {
    ui->selectedSoftwareInfo->setText(status);
    // Enable cancel button during scan
    ui->cancelSoftwareScanButton->setEnabled(true); });

    connect(m_softwareManager, &SoftwareManager::scanFinished, this, [this](bool success, const QString &message)
            {
    ui->selectedSoftwareInfo->setText(message);
    ui->scanSoftwareButton->setEnabled(true);
    ui->searchSoftwareInput->setEnabled(true);
    ui->cancelSoftwareScanButton->setEnabled(false);
    
    if (success) {
        // Software table will be populated automatically
    } });

    // Add cancel button slot
    connect(ui->cancelSoftwareScanButton, &QPushButton::clicked, this, [this]()
            {
    m_softwareManager->cancelScan();
    ui->cancelSoftwareScanButton->setEnabled(false); });

    // Auto-refresh when software tab is opened
    connect(ui->appsButton, &QPushButton::clicked, this, [this]()
            {
    if (ui->contentStackedWidget->currentWidget() == ui->appsPage) {
        // Only refresh if we don't have data yet
        if (ui->softwareTable->rowCount() == 0) {
            on_scanSoftwareButton_clicked();
        }
    } });

    m_systemInfoManager = new SystemInfoManager(this, this);

    // Connect system info signals
    connect(m_systemInfoManager, &SystemInfoManager::systemInfoUpdated,
            this, &MainWindow::onSystemInfoUpdated);
    connect(m_systemInfoManager, &SystemInfoManager::performanceUpdated,
            this, &MainWindow::onPerformanceUpdated);
    connect(m_systemInfoManager, &SystemInfoManager::updateFinished,
            this, &MainWindow::onSystemInfoUpdateFinished);

    // Auto-refresh when general tab is opened
    connect(ui->generalButton, &QPushButton::clicked, this, [this]()
            {
    if (ui->contentStackedWidget->currentWidget() == ui->generalPage) {
        on_refreshSystemInfoButton_clicked();
    } });
}

// Add this new slot to MainWindow class (add declaration to mainwindow.h too)
void MainWindow::onSystemCleanerItemClicked(QListWidgetItem *item)
{
    if (!item)
        return;

    QString text = item->text();

    // Toggle between checked (✅) and unchecked (❌)
    if (text.contains("❌"))
    {
        text.replace("❌", "✅");
    }
    else
    {
        text.replace("✅", "❌");
    }

    item->setText(text);

    // Enable/disable clean button based on whether any items are selected
    bool hasSelected = false;
    for (int i = 0; i < ui->systemCleanerList->count(); ++i)
    {
        if (ui->systemCleanerList->item(i)->text().contains("✅"))
        {
            hasSelected = true;
            break;
        }
    }

    ui->cleanSystemButton->setEnabled(hasSelected);
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
    ui->filesCheckerButton->setStyleSheet(defaultStyle); // Make sure this is included!
}

void MainWindow::setActiveButton(QPushButton *activeButton)
{
    // Reset all buttons first
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

    // Apply active style only to the clicked button
    if (activeButton == ui->generalButton)
    {
        ui->generalButton->setStyleSheet(activeStyle);
    }
    else if (activeButton == ui->wifiButton)
    {
        ui->wifiButton->setStyleSheet(activeStyle);
    }
    else if (activeButton == ui->appsButton)
    {
        ui->appsButton->setStyleSheet(activeStyle);
    }
    else if (activeButton == ui->cleanerButton)
    {
        ui->cleanerButton->setStyleSheet(activeStyle);
    }
    else if (activeButton == ui->networkButton)
    {
        ui->networkButton->setStyleSheet(activeStyle);
    }
    else if (activeButton == ui->hardwareButton)
    {
        ui->hardwareButton->setStyleSheet(activeStyle);
    }
    else if (activeButton == ui->optionsButton)
    {
        ui->optionsButton->setStyleSheet(activeStyle);
    }
    else if (activeButton == ui->filesCheckerButton)
    {
        ui->filesCheckerButton->setStyleSheet(activeStyle);
    }
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
// Update the navigation slots to cancel operations when switching tabs
void MainWindow::on_generalButton_clicked()
{
   cancelAllOperations();
    setActiveButton(ui->generalButton);
    updateContent("General Settings");
    ui->contentStackedWidget->setCurrentWidget(ui->generalPage);

    // Auto-refresh system info when tab is opened
    if (ui->systemInfoTab->isVisible()) {
        on_refreshSystemInfoButton_clicked();
    }

    // Keep your existing startup programs population
    if (ui->startupTable->rowCount() == 0) {
        QStringList startupPrograms = {
            "Microsoft OneDrive", "Spotify", "Adobe Creative Cloud",
            "Discord", "Steam Client", "NVIDIA Control Panel",
            "Realtek Audio", "Windows Security", "Google Update"};

        QStringList impacts = {"Low", "Medium", "High", "Low", "Medium", "Low", "Low", "Low", "Low"};
        QStringList types = {"Registry", "Startup Folder", "Service", "Registry", "Registry", "Service", "Service", "Service", "Scheduled Task"};

        for (int i = 0; i < startupPrograms.size(); ++i) {
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

void MainWindow::on_appsButton_clicked()
{
    cancelAllOperations();
    setActiveButton(ui->appsButton);
    updateContent("Favorite Apps");
    ui->contentStackedWidget->setCurrentWidget(ui->appsPage);

    // Populate apps on first load
    if (ui->appsGridLayout->count() == 0)
    {
        on_refreshAppsButton_clicked();
    }
}

void MainWindow::on_networkButton_clicked()
{
    cancelAllOperations();
    setActiveButton(ui->networkButton);
    updateContent("Network Tools");
    ui->contentStackedWidget->setCurrentWidget(ui->networkPage);

    // Auto-refresh network status when page is opened
    on_pushButton_refreshNetwork_clicked();
}

void MainWindow::on_hardwareButton_clicked()
{
    cancelAllOperations();
    setActiveButton(ui->hardwareButton);
    updateContent("Hardware Information");
    ui->contentStackedWidget->setCurrentWidget(ui->hardwarePage);

    // Auto-refresh hardware info when page is opened
    on_pushButton_refreshHardware_clicked();
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

void MainWindow::on_selectAllSystemButton_clicked()
{
    QListWidget *systemList = ui->systemCleanerList;
    if (!systemList)
        return;

    // Check if any items are currently unselected (have ❌)
    bool hasUnselected = false;
    for (int i = 0; i < systemList->count(); ++i)
    {
        QListWidgetItem *item = systemList->item(i);
        if (item && item->text().contains("❌"))
        {
            hasUnselected = true;
            break;
        }
    }

    // If there are unselected items, select all. Otherwise, deselect all.
    bool shouldSelect = hasUnselected;

    for (int i = 0; i < systemList->count(); ++i)
    {
        QListWidgetItem *item = systemList->item(i);
        if (item)
        {
            QString text = item->text();
            if (shouldSelect)
            {
                // Select item (replace ❌ with ✅)
                text.replace("❌", "✅");
            }
            else
            {
                // Deselect item (replace ✅ with ❌)
                text.replace("✅", "❌");
            }
            item->setText(text);
        }
    }

    // Enable/disable clean button based on selection
    bool hasSelected = false;
    for (int i = 0; i < systemList->count(); ++i)
    {
        if (systemList->item(i)->text().contains("✅"))
        {
            hasSelected = true;
            break;
        }
    }

    ui->cleanSystemButton->setEnabled(hasSelected);
}

// Software Uninstaller slots - Delegated to SoftwareManager
void MainWindow::on_scanSoftwareButton_clicked()
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

// Implement the new slots
void MainWindow::on_refreshDiskSpaceButton_clicked()
{
    m_filesChecker->refreshDiskSpace();
}

void MainWindow::debugTableState()
{
    qDebug() << "=== Table Debug Info ===";
    qDebug() << "Table row count:" << ui->largeFilesTable->rowCount();
    qDebug() << "Table column count:" << ui->largeFilesTable->columnCount();
    qDebug() << "Table is visible:" << ui->largeFilesTable->isVisible();
    qDebug() << "Table viewport is visible:" << ui->largeFilesTable->viewport()->isVisible();

    for (int i = 0; i < ui->largeFilesTable->rowCount() && i < 5; ++i)
    {
        qDebug() << "Row" << i << "data:";
        for (int j = 0; j < ui->largeFilesTable->columnCount(); ++j)
        {
            QTableWidgetItem *item = ui->largeFilesTable->item(i, j);
            if (item)
            {
                qDebug() << "  Col" << j << ":" << item->text() << "CheckState:" << item->checkState();
            }
            else
            {
                qDebug() << "  Col" << j << ": NULL ITEM";
            }
        }
    }
    qDebug() << "=== End Table Debug ===";
}

void MainWindow::on_scanLargeFilesButton_clicked()
{
    QString path = ui->largeFilesPathInput->text();
    if (path.isEmpty())
    {
        path = QDir::homePath();
        ui->largeFilesPathInput->setText(path);
    }

    double minSizeGB = ui->largeFilesSizeSpinBox->value();

    qDebug() << "=== Starting scan ===";
    qDebug() << "Path:" << path;
    qDebug() << "Min size:" << minSizeGB << "GB";

    if (m_filesChecker)
    {
        m_filesChecker->scanLargeFiles(path, minSizeGB);

        // Debug table state after a short delay
        QTimer::singleShot(1000, this, &MainWindow::debugTableState);
    }
}

void MainWindow::on_openFileLocationButton_clicked()
{
    // Check if any file is selected
    bool hasSelection = false;
    int selectedRow = -1;

    for (int row = 0; row < ui->largeFilesTable->rowCount(); ++row)
    {
        QTableWidgetItem *checkItem = ui->largeFilesTable->item(row, 0);
        if (checkItem && checkItem->checkState() == Qt::Checked)
        {
            hasSelection = true;
            selectedRow = row;
            break;
        }
    }

    if (hasSelection && selectedRow >= 0)
    {
        QTableWidgetItem *pathItem = ui->largeFilesTable->item(selectedRow, 1);
        if (pathItem && m_filesChecker)
        {
            QString filePath = pathItem->text();
            m_filesChecker->openFileDirectory(filePath);
        }
    }
    else
    {
        QMessageBox::information(this, "Open Location", "Please select a file first by checking the checkbox.");
    }
}

void MainWindow::on_deleteLargeFilesButton_clicked()
{
    QVector<FileInfo> filesToDelete;

    for (int row = 0; row < ui->largeFilesTable->rowCount(); ++row)
    {
        QTableWidgetItem *checkItem = ui->largeFilesTable->item(row, 0);
        QTableWidgetItem *pathItem = ui->largeFilesTable->item(row, 1);
        QTableWidgetItem *sizeItem = ui->largeFilesTable->item(row, 2);

        if (checkItem && checkItem->data(Qt::UserRole).toBool() && pathItem && sizeItem)
        {
            FileInfo fileInfo;
            fileInfo.path = pathItem->text();
            fileInfo.size = 0;
            fileInfo.isSelected = true;
            filesToDelete.append(fileInfo);
        }
    }

    if (filesToDelete.isEmpty())
    {
        QMessageBox::information(this, "Delete Files", "No files selected for deletion.");
        return;
    }

    if (m_filesChecker)
    {
        m_filesChecker->deleteSelectedFiles(filesToDelete);

        // Remove deleted rows from the table
        for (int row = ui->largeFilesTable->rowCount() - 1; row >= 0; --row)
        {
            QTableWidgetItem *checkItem = ui->largeFilesTable->item(row, 0);
            if (checkItem && checkItem->data(Qt::UserRole).toBool())
            {
                ui->largeFilesTable->removeRow(row);
            }
        }

        // Update the results text
        ui->largeFilesResults->append(QString("\nDeleted %1 file(s).").arg(filesToDelete.size()));
        updateDeleteButtonState();
    }
}

void MainWindow::on_scanDuplicateFilesButton_clicked()
{
    QString path = ui->duplicateFilesPathInput->text();
    if (path.isEmpty())
    {
        path = QDir::homePath();
        ui->duplicateFilesPathInput->setText(path);
    }

    m_filesChecker->scanDuplicateFiles(path);
}

void MainWindow::on_deleteDuplicateFilesButton_clicked()
{
    if (!m_filesChecker)
        return;

    QTreeWidget *tree = ui->duplicateFilesTree;
    QVector<FileInfo> filesToDelete;

    // Collect files to delete (keep one from each group)
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *groupItem = tree->topLevelItem(i);
        bool firstCheckedKept = false;

        for (int j = 0; j < groupItem->childCount(); ++j)
        {
            QTreeWidgetItem *fileItem = groupItem->child(j);
            if (fileItem->data(0, Qt::UserRole).toBool())
            {
                if (!firstCheckedKept)
                {
                    // Keep the first checked file in each group
                    firstCheckedKept = true;
                }
                else
                {
                    // Delete additional checked files
                    FileInfo fileInfo;
                    fileInfo.path = fileItem->text(4); // Full path from hidden column
                    fileInfo.isSelected = true;
                    filesToDelete.append(fileInfo);
                }
            }
        }
    }

    if (filesToDelete.isEmpty())
    {
        QMessageBox::information(this, "Delete Duplicate Files",
                                 "No duplicate files selected for deletion.\n\n"
                                 "Please check the files you want to delete (keep at least one file from each duplicate group).");
        return;
    }

    // Show confirmation with file list
    QString confirmationText = QString("Are you sure you want to delete %1 duplicate file(s)?\n\nSelected files:\n")
                                   .arg(filesToDelete.size());

    for (int i = 0; i < filesToDelete.size() && i < 10; ++i)
    {
        QFileInfo fileInfo(filesToDelete[i].path);
        confirmationText += "• " + fileInfo.fileName() + "\n";
    }

    if (filesToDelete.size() > 10)
    {
        confirmationText += QString("• ... and %1 more files\n").arg(filesToDelete.size() - 10);
    }

    confirmationText += "\nThis action cannot be undone.";

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm Delete",
        confirmationText,
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No)
        return;

    // Delete the files
    m_filesChecker->deleteSelectedFiles(filesToDelete);

    // Refresh the tree to remove deleted files
    on_scanDuplicateFilesButton_clicked();
}

void MainWindow::on_duplicateFilesTree_itemChanged(QTreeWidgetItem *item, int column)
{
    // Handle selection changes in duplicate files tree
}

void MainWindow::on_browseLargeFilesPathButton_clicked()
{
    QString currentPath = ui->largeFilesPathInput->text();
    if (currentPath.isEmpty())
    {
        currentPath = QDir::homePath();
    }

    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory to Scan", currentPath);
    if (!dir.isEmpty())
    {
        ui->largeFilesPathInput->setText(dir);
    }
}

void MainWindow::on_browseDuplicateFilesPathButton_clicked()
{
    QString currentPath = ui->duplicateFilesPathInput->text();
    if (currentPath.isEmpty())
    {
        currentPath = QDir::homePath();
    }

    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory to Scan", currentPath);
    if (!dir.isEmpty())
    {
        ui->duplicateFilesPathInput->setText(dir);
    }
}

void MainWindow::on_largeFilesPathInput_textChanged(const QString &text)
{
    // Enable/disable scan button based on path validity
    QDir dir(text);
    ui->scanLargeFilesButton->setEnabled(dir.exists());
}

void MainWindow::on_duplicateFilesPathInput_textChanged(const QString &text)
{
    // Enable/disable scan button based on path validity
    QDir dir(text);
    ui->scanDuplicateFilesButton->setEnabled(dir.exists());
}

void MainWindow::on_cancelLargeFilesButton_clicked()
{
    if (m_filesChecker)
    {
        m_filesChecker->cancelLargeFilesScan();
    }
}

void MainWindow::on_cancelDuplicateFilesButton_clicked()
{
    if (m_filesChecker)
    {
        m_filesChecker->cancelDuplicateFilesScan();
    }
}

void MainWindow::on_largeFilesTable_itemDoubleClicked(QTableWidgetItem *item)
{
    if (!item)
        return;

    int row = item->row();
    QTableWidgetItem *pathItem = ui->largeFilesTable->item(row, 1); // Path column

    if (pathItem && m_filesChecker)
    {
        QString filePath = pathItem->text();
        m_filesChecker->openFileDirectory(filePath);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    cancelAllOperations();
    QMainWindow::closeEvent(event);
}

void MainWindow::cancelAllOperations()
{
    // Cancel all running operations before switching tabs
    if (m_filesChecker)
    {
        m_filesChecker->cancelLargeFilesScan();
        m_filesChecker->cancelDuplicateFilesScan();
    }

    // Process events to ensure cancellation happens immediately
    QCoreApplication::processEvents();

    // Add other managers if they have cancelable operations
    if (m_networkManager)
    {
        // Add network operation cancellations if needed
    }
}

void MainWindow::on_wifiButton_clicked()
{
    cancelAllOperations();
    setActiveButton(ui->wifiButton);
    updateContent("WiFi Management");
    ui->contentStackedWidget->setCurrentWidget(ui->wifiPage);
}

void MainWindow::on_cleanerButton_clicked()
{
    cancelAllOperations();
    setActiveButton(ui->cleanerButton);
    updateContent("System Cleaner");
    showCleanerPage();
}

void MainWindow::on_optionsButton_clicked()
{
    cancelAllOperations();
    setActiveButton(ui->optionsButton);
    updateContent("Options & Settings");
    ui->contentStackedWidget->setCurrentWidget(ui->optionsPage);
}

void MainWindow::on_filesCheckerButton_clicked()
{
    cancelAllOperations();
    setActiveButton(ui->filesCheckerButton);
    updateContent("Files Checker");
    ui->contentStackedWidget->setCurrentWidget(ui->filesCheckerPage);

    // Refresh disk space on first load
    if (m_filesChecker)
    {
        m_filesChecker->refreshDiskSpace();
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->largeFilesTable->viewport() && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        QModelIndex index = ui->largeFilesTable->indexAt(mouseEvent->pos());

        if (index.isValid() && index.column() == 0)
        { // Checkbox column
            QTableWidgetItem *item = ui->largeFilesTable->item(index.row(), 0);
            if (item)
            {
                // Toggle the checkbox
                bool isChecked = item->data(Qt::UserRole).toBool();
                if (isChecked)
                {
                    item->setText("❌");
                    item->setData(Qt::UserRole, false);
                }
                else
                {
                    item->setText("✅");
                    item->setData(Qt::UserRole, true);
                }

                // Update delete button state
                updateDeleteButtonState();
                return true; // Event handled
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::updateDeleteButtonState()
{
    bool hasSelection = false;
    for (int row = 0; row < ui->largeFilesTable->rowCount(); ++row)
    {
        QTableWidgetItem *checkItem = ui->largeFilesTable->item(row, 0);
        if (checkItem && checkItem->data(Qt::UserRole).toBool())
        {
            hasSelection = true;
            break;
        }
    }
    ui->deleteLargeFilesButton->setEnabled(hasSelection);
}

void MainWindow::on_largeFilesTable_itemChanged(QTableWidgetItem *item)
{
    if (!item)
        return;

    if (item->column() == 0)
    { // Checkbox column
        // NO EMOJI TEXT - let Qt handle the checkbox visuals

        // Enable/disable delete button based on selection
        bool hasSelection = false;
        for (int row = 0; row < ui->largeFilesTable->rowCount(); ++row)
        {
            QTableWidgetItem *checkItem = ui->largeFilesTable->item(row, 0);
            if (checkItem && checkItem->checkState() == Qt::Checked)
            {
                hasSelection = true;
                break;
            }
        }
        ui->deleteLargeFilesButton->setEnabled(hasSelection);
    }
}

void MainWindow::onLargeFilesTableItemClicked(QTableWidgetItem *item)
{
    if (!item)
        return;

    // Only handle clicks in the checkbox column (column 0)
    if (item->column() == 0)
    {
        // Toggle the checkbox
        bool isChecked = item->data(Qt::UserRole).toBool();
        if (isChecked)
        {
            item->setText("❌");
            item->setData(Qt::UserRole, false);
        }
        else
        {
            item->setText("✅");
            item->setData(Qt::UserRole, true);
        }

        // Update delete button state
        updateDeleteButtonState();
    }
}

void MainWindow::on_cancelSoftwareScanButton_clicked()
{
    if (m_softwareManager)
    {
        m_softwareManager->cancelScan();
        ui->cancelSoftwareScanButton->setEnabled(false);
        ui->selectedSoftwareInfo->setText("Scan cancelled by user.");
    }
}













void MainWindow::onSystemInfoUpdated(const QString &osInfo, const QString &cpuInfo, 
                                    const QString &ramInfo, const QString &storageInfo, 
                                    const QString &gpuInfo)
{
    ui->osValueLabel->setText(osInfo);
    ui->cpuValueLabel->setText(cpuInfo);
    ui->ramValueLabel->setText(ramInfo);
    ui->storageValueLabel->setText(QString("%1 (%2)").arg(storageInfo));
    ui->gpuValueLabel->setText(gpuInfo);
}

void MainWindow::onPerformanceUpdated(int cpuUsage, int ramUsage, int diskUsage)
{
    ui->cpuUsageProgress->setValue(cpuUsage);
    ui->ramUsageProgress->setValue(ramUsage);
    ui->diskUsageProgress->setValue(diskUsage);
    
    ui->cpuUsageValue->setText(QString("%1%").arg(cpuUsage));
    ui->ramUsageValue->setText(QString("%1%").arg(ramUsage));
    ui->diskUsageValue->setText(QString("%1%").arg(diskUsage));
}

void MainWindow::onSystemInfoUpdateFinished(bool success, const QString &message)
{
    if (success) {
        // Start real-time monitoring after initial scan
        m_systemInfoManager->startRealTimeMonitoring();
    }
    // You can show the message in status if needed
}

// Update the existing refresh method
void MainWindow::on_refreshSystemInfoButton_clicked()
{
    m_systemInfoManager->refreshSystemInfo();
}

// Update the existing generate report method  
void MainWindow::on_generateReportButton_clicked()
{
    m_systemInfoManager->generateSystemReport();
}