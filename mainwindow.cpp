#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QRegularExpression>
#include <QTableWidgetItem>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    // Set window properties
    setWindowTitle("Raptor PC Controller");
    setMinimumSize(1000, 700);
    
    setupConnections();
    
    // Set initial active button (General)
    on_generalButton_clicked();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    // Note: Most connections are now auto-connected via Qt's naming convention
    // Additional manual connections can be added here if needed
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

void MainWindow::setActiveButton(QPushButton* activeButton)
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

void MainWindow::updateContent(const QString& title)
{
    ui->titleLabel->setText(title);
}

void MainWindow::showCleanerPage()
{
    ui->contentStackedWidget->setCurrentWidget(ui->cleanerPage);
}

// Main navigation slots
void MainWindow::on_wifiButton_clicked()
{
    setActiveButton(ui->wifiButton);
    updateContent("WiFi Management");
    ui->contentStackedWidget->setCurrentWidget(ui->welcomePage);
}

void MainWindow::on_appsButton_clicked()
{
    setActiveButton(ui->appsButton);
    updateContent("Applications");
    ui->contentStackedWidget->setCurrentWidget(ui->welcomePage);
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
    updateContent("Network Settings");
    ui->contentStackedWidget->setCurrentWidget(ui->welcomePage);
}

void MainWindow::on_hardwareButton_clicked()
{
    setActiveButton(ui->hardwareButton);
    updateContent("Hardware Information");
    ui->contentStackedWidget->setCurrentWidget(ui->welcomePage);
}

void MainWindow::on_optionsButton_clicked()
{
    setActiveButton(ui->optionsButton);
    updateContent("Options & Settings");
    ui->contentStackedWidget->setCurrentWidget(ui->welcomePage);
}

// Cleaner tab slots
void MainWindow::on_scanQuickButton_clicked()
{
    ui->quickCleanResults->setPlainText("Scanning for junk files...\n• Temporary files: 245 MB\n• Browser cache: 156 MB\n• System logs: 45 MB\n\nTotal: 446 MB");
    ui->spaceSavedLabel->setText("Total space to be freed: 446 MB");
    ui->cleanQuickButton->setEnabled(true);
    ui->quickCleanProgressBar->setValue(100);
}

void MainWindow::on_cleanQuickButton_clicked()
{
    ui->quickCleanResults->append("\n\nCleaning completed successfully!");
    ui->cleanQuickButton->setEnabled(false);
    ui->spaceSavedLabel->setText("Total space freed: 446 MB");
}

void MainWindow::on_scanSystemButton_clicked()
{
    // Update list items with simulated sizes using QRegularExpression
    QRegularExpression regex("\\(.*\\)");
    for(int i = 0; i < ui->systemCleanerList->count(); ++i) {
        QListWidgetItem* item = ui->systemCleanerList->item(i);
        QString text = item->text();
        text.replace(regex, "(125 MB)");
        item->setText(text);
    }
    ui->cleanSystemButton->setEnabled(true);
}

void MainWindow::on_cleanSystemButton_clicked()
{
    ui->cleanSystemButton->setEnabled(false);
}

void MainWindow::on_selectAllSystemButton_clicked()
{
    for(int i = 0; i < ui->systemCleanerList->count(); ++i) {
        ui->systemCleanerList->item(i)->setSelected(true);
    }
}

void MainWindow::on_scanBrowsersButton_clicked()
{
    ui->cleanBrowsersButton->setEnabled(true);
}

void MainWindow::on_cleanBrowsersButton_clicked()
{
    ui->cleanBrowsersButton->setEnabled(false);
}

// Software Uninstaller slots
void MainWindow::on_refreshSoftwareButton_clicked()
{
    populateSoftwareTable();
}

void MainWindow::on_searchSoftwareInput_textChanged(const QString &searchText)
{
    // Simple search filter
    if (searchText.isEmpty()) {
        // Show all items
        for (int i = 0; i < ui->softwareTable->rowCount(); ++i) {
            ui->softwareTable->setRowHidden(i, false);
        }
    } else {
        // Hide non-matching items
        for (int i = 0; i < ui->softwareTable->rowCount(); ++i) {
            QTableWidgetItem* item = ui->softwareTable->item(i, 0); // Name column
            bool match = item && item->text().contains(searchText, Qt::CaseInsensitive);
            ui->softwareTable->setRowHidden(i, !match);
        }
    }
}

void MainWindow::on_softwareTable_itemSelectionChanged()
{
    QList<QTableWidgetItem*> selectedItems = ui->softwareTable->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();
    
    ui->uninstallSoftwareButton->setEnabled(hasSelection);
    ui->forceUninstallButton->setEnabled(hasSelection);
    
    if (hasSelection) {
        int row = ui->softwareTable->currentRow();
        QString name = ui->softwareTable->item(row, 0)->text();
        QString version = ui->softwareTable->item(row, 1)->text();
        QString size = ui->softwareTable->item(row, 2)->text();
        
        ui->selectedSoftwareInfo->setText(
            QString("Selected: %1 %2 (%3)").arg(name).arg(version).arg(size)
        );
    } else {
        ui->selectedSoftwareInfo->setText("No software selected");
    }
}

void MainWindow::on_uninstallSoftwareButton_clicked()
{
    // Placeholder for uninstall functionality
    int row = ui->softwareTable->currentRow();
    if (row >= 0) {
        QString softwareName = ui->softwareTable->item(row, 0)->text();
        ui->selectedSoftwareInfo->setText(
            QString("Would uninstall: %1 (normal mode)").arg(softwareName)
        );
    }
}

void MainWindow::on_forceUninstallButton_clicked()
{
    // Placeholder for force uninstall functionality
    int row = ui->softwareTable->currentRow();
    if (row >= 0) {
        QString softwareName = ui->softwareTable->item(row, 0)->text();
        ui->selectedSoftwareInfo->setText(
            QString("Would force uninstall: %1").arg(softwareName)
        );
    }
}

void MainWindow::populateSoftwareTable()
{
    // Clear existing items
    ui->softwareTable->setRowCount(0);
    
    // Add sample data - in real implementation, this would query the system
    QStringList sampleSoftware = {
        "Google Chrome", "Mozilla Firefox", "Microsoft Edge", "Visual Studio Code",
        "Adobe Reader", "VLC Media Player", "7-Zip", "WinRAR", "Spotify", "Discord"
    };
    
    QStringList versions = {
        "96.0.4664.110", "95.0.2", "96.0.1054.62", "1.63.0",
        "2021.011.20039", "3.0.16", "21.07", "6.02", "1.1.68.610", "1.0.9003"
    };
    
    QStringList sizes = {
        "350 MB", "280 MB", "320 MB", "450 MB",
        "650 MB", "85 MB", "2.5 MB", "3.1 MB", "180 MB", "140 MB"
    };
    
    QStringList dates = {
        "2023-11-15", "2023-11-10", "2023-11-20", "2023-11-05",
        "2023-10-28", "2023-11-12", "2023-09-15", "2023-10-20", "2023-11-18", "2023-11-22"
    };
    
    for (int i = 0; i < sampleSoftware.size(); ++i) {
        int row = ui->softwareTable->rowCount();
        ui->softwareTable->insertRow(row);
        
        ui->softwareTable->setItem(row, 0, new QTableWidgetItem(sampleSoftware[i]));
        ui->softwareTable->setItem(row, 1, new QTableWidgetItem(versions[i]));
        ui->softwareTable->setItem(row, 2, new QTableWidgetItem(sizes[i]));
        ui->softwareTable->setItem(row, 3, new QTableWidgetItem(dates[i]));
    }
    
    // Resize columns to content
    ui->softwareTable->resizeColumnsToContents();
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
    if (row >= 0) {
        QTableWidgetItem* statusItem = ui->startupTable->item(row, 1);
        if (statusItem && statusItem->text() == "Enabled") {
            statusItem->setText("Disabled");
            statusItem->setForeground(QBrush(QColor("#e74c3c")));
        }
    }
}

void MainWindow::on_enableStartupButton_clicked()
{
    // Placeholder for enabling startup program
    int row = ui->startupTable->currentRow();
    if (row >= 0) {
        QTableWidgetItem* statusItem = ui->startupTable->item(row, 1);
        if (statusItem && statusItem->text() == "Disabled") {
            statusItem->setText("Enabled");
            statusItem->setForeground(QBrush(QColor("#2ecc71")));
        }
    }
}

void MainWindow::on_startupTable_itemSelectionChanged()
{
    QList<QTableWidgetItem*> selectedItems = ui->startupTable->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();
    
    if (hasSelection) {
        int row = ui->startupTable->currentRow();
        QTableWidgetItem* statusItem = ui->startupTable->item(row, 1);
        bool isEnabled = statusItem && statusItem->text() == "Enabled";
        
        ui->disableStartupButton->setEnabled(isEnabled);
        ui->enableStartupButton->setEnabled(!isEnabled);
    } else {
        ui->disableStartupButton->setEnabled(false);
        ui->enableStartupButton->setEnabled(false);
    }
}

void MainWindow::on_saveSettingsButton_clicked()
{
    // Placeholder for saving settings
    ui->preferencesLabel->setText("Settings saved successfully!");
}

void MainWindow::on_resetSettingsButton_clicked()
{
    // Placeholder for resetting settings
    ui->themeComboBox->setCurrentIndex(0);
    ui->languageComboBox->setCurrentIndex(0);
    ui->startMinimizedCheckbox->setChecked(false);
    ui->autoUpdateCheckbox->setChecked(true);
    ui->backupSettingsCheckbox->setChecked(true);
    ui->notificationsCheckbox->setChecked(true);
    ui->preferencesLabel->setText("Settings reset to defaults!");
}

// Also update the on_generalButton_clicked() method to show the general page:
void MainWindow::on_generalButton_clicked()
{
    setActiveButton(ui->generalButton);
    updateContent("General Settings");
    ui->contentStackedWidget->setCurrentWidget(ui->generalPage);
    
    // Populate startup programs table
    if (ui->startupTable->rowCount() == 0) {
        QStringList startupPrograms = {
            "Microsoft OneDrive", "Spotify", "Adobe Creative Cloud", 
            "Discord", "Steam Client", "NVIDIA Control Panel", 
            "Realtek Audio", "Windows Security", "Google Update"
        };
        
        QStringList impacts = {"Low", "Medium", "High", "Low", "Medium", "Low", "Low", "Low", "Low"};
        QStringList types = {"Registry", "Startup Folder", "Service", "Registry", "Registry", "Service", "Service", "Service", "Scheduled Task"};
        
        for (int i = 0; i < startupPrograms.size(); ++i) {
            int row = ui->startupTable->rowCount();
            ui->startupTable->insertRow(row);
            
            ui->startupTable->setItem(row, 0, new QTableWidgetItem(startupPrograms[i]));
            
            QTableWidgetItem* statusItem = new QTableWidgetItem("Enabled");
            statusItem->setForeground(QBrush(QColor("#2ecc71")));
            ui->startupTable->setItem(row, 1, statusItem);
            
            ui->startupTable->setItem(row, 2, new QTableWidgetItem(impacts[i]));
            ui->startupTable->setItem(row, 3, new QTableWidgetItem(types[i]));
        }
        ui->startupTable->resizeColumnsToContents();
    }
}