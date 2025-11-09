#include "softwaremanager.h"
#include "../mainwindow.h"
#include "../ui_mainwindow.h"
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <windows.h>
#include <tlhelp32.h>

SoftwareManager::SoftwareManager(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow), m_cancelScan(false)
{
    m_scanWatcher = new QFutureWatcher<QList<InstalledSoftware>>(this);
    setupConnections();
}

SoftwareManager::~SoftwareManager()
{
    cancelScan();
    if (m_scanWatcher) {
        m_scanWatcher->waitForFinished();
        delete m_scanWatcher;
    }
}

void SoftwareManager::setupConnections()
{
    connect(m_scanWatcher, &QFutureWatcher<QList<InstalledSoftware>>::finished,
            this, &SoftwareManager::onScanFinished);
    
    connect(this, &SoftwareManager::scanProgressUpdated,
            this, [this](int progress, const QString &status) {
                if (m_mainWindow && m_mainWindow->ui) {
                    m_mainWindow->ui->selectedSoftwareInfo->setText(status);
                    
                    // You could add a progress bar to your UI if needed
                    // m_mainWindow->ui->softwareScanProgress->setValue(progress);
                }
            });
    
    connect(this, &SoftwareManager::scanFinished,
            this, [this](bool success, const QString &message) {
                if (m_mainWindow && m_mainWindow->ui) {
                    if (success) {
                        populateSoftwareTable();
                        m_mainWindow->ui->scanSoftwareButton->setEnabled(true);
                        m_mainWindow->ui->searchSoftwareInput->setEnabled(true);
                    }
                    m_mainWindow->ui->selectedSoftwareInfo->setText(message);
                }
            });
}

void SoftwareManager::refreshSoftware()
{
    if (m_scanWatcher->isRunning()) {
        QMessageBox::information(m_mainWindow, "Scan in Progress", 
                               "Software scan is already running. Please wait for it to complete.");
        return;
    }

    // Reset cancel flag
    m_cancelScan = false;

    // Update UI for scanning state
    m_mainWindow->ui->selectedSoftwareInfo->setText("🔄 Scanning installed software...\nThis may take a few moments.");
    m_mainWindow->ui->scanSoftwareButton->setEnabled(false);
    m_mainWindow->ui->searchSoftwareInput->setEnabled(false);
    m_mainWindow->ui->uninstallSoftwareButton->setEnabled(false);
    m_mainWindow->ui->forceUninstallButton->setEnabled(false);

    // Clear previous results but keep table structure
    m_mainWindow->ui->softwareTable->setRowCount(0);
    m_mainWindow->ui->softwareTable->setSortingEnabled(false);

    // Start background scan
    QFuture<QList<InstalledSoftware>> future = QtConcurrent::run([this]() {
        return this->getInstalledSoftwareFromRegistry();
    });

    m_scanWatcher->setFuture(future);

    // Start progress updates
    QTimer *progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this, &SoftwareManager::updateScanProgress);
    progressTimer->start(100); // Update every 100ms

    // Auto-cleanup timer
    connect(m_scanWatcher, &QFutureWatcher<QList<InstalledSoftware>>::finished,
            progressTimer, &QTimer::deleteLater);
}

void SoftwareManager::cancelScan()
{
    if (m_scanWatcher->isRunning()) {
        m_cancelScan = true;
        m_scanWatcher->cancel();
        
        if (m_mainWindow && m_mainWindow->ui) {
            m_mainWindow->ui->selectedSoftwareInfo->setText("Scan cancelled by user.");
            m_mainWindow->ui->scanSoftwareButton->setEnabled(true);
            m_mainWindow->ui->searchSoftwareInput->setEnabled(true);
        }
    }
}

void SoftwareManager::updateScanProgress()
{
    if (m_scanWatcher->isRunning()) {
        // Show that we're still scanning
        static int dotCount = 0;
        dotCount = (dotCount + 1) % 4;
        QString dots = QString(".").repeated(dotCount);
        QString spaces = QString(" ").repeated(3 - dotCount);
        
        emit scanProgressUpdated(0, QString("🔄 Scanning installed software%1%2\nPlease wait...").arg(dots).arg(spaces));
    }
}

void SoftwareManager::onScanFinished()
{
    if (m_scanWatcher->isCanceled()) {
        emit scanFinished(false, "Scan cancelled by user.");
        return;
    }

    try {
        m_installedSoftware = m_scanWatcher->result();
        
        // Sort by name
        std::sort(m_installedSoftware.begin(), m_installedSoftware.end(),
                  [](const InstalledSoftware &a, const InstalledSoftware &b) {
                      return a.name.toLower() < b.name.toLower();
                  });

        emit scanFinished(true, QString("✅ Scan completed! Found %1 installed applications.").arg(m_installedSoftware.size()));
        
    } catch (const std::exception &e) {
        emit scanFinished(false, QString("❌ Scan failed: %1").arg(e.what()));
    }
}

void SoftwareManager::searchSoftware(const QString &searchText)
{
    if (searchText.isEmpty()) {
        // Show all items
        for (int i = 0; i < m_mainWindow->ui->softwareTable->rowCount(); ++i) {
            m_mainWindow->ui->softwareTable->setRowHidden(i, false);
        }
    } else {
        // Hide non-matching items
        for (int i = 0; i < m_mainWindow->ui->softwareTable->rowCount(); ++i) {
            QTableWidgetItem *item = m_mainWindow->ui->softwareTable->item(i, 0); // Name column
            bool match = item && item->text().contains(searchText, Qt::CaseInsensitive);
            m_mainWindow->ui->softwareTable->setRowHidden(i, !match);
        }
    }
}

void SoftwareManager::onSoftwareSelectionChanged()
{
    QList<QTableWidgetItem *> selectedItems = m_mainWindow->ui->softwareTable->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();

    m_mainWindow->ui->uninstallSoftwareButton->setEnabled(hasSelection);
    m_mainWindow->ui->forceUninstallButton->setEnabled(hasSelection);

    if (hasSelection) {
        int row = m_mainWindow->ui->softwareTable->currentRow();
        QString name = m_mainWindow->ui->softwareTable->item(row, 0)->text();
        QString version = m_mainWindow->ui->softwareTable->item(row, 1)->text();
        QString size = m_mainWindow->ui->softwareTable->item(row, 2)->text();
        QString date = m_mainWindow->ui->softwareTable->item(row, 3)->text();

        // Find the software details
        for (const auto &software : m_installedSoftware) {
            if (software.name == name && software.version == version) {
                QString details = QString("📱 Selected: %1 %2\n💾 Size: %3\n📅 Installed: %4\n🏢 Publisher: %5")
                    .arg(software.name)
                    .arg(software.version)
                    .arg(size)
                    .arg(software.installDate)
                    .arg(software.publisher);
                
                if (software.isSystemComponent) {
                    details += "\n⚠️ System Component - Use Force Uninstall with caution";
                }
                
                m_mainWindow->ui->selectedSoftwareInfo->setText(details);
                break;
            }
        }
    } else {
        m_mainWindow->ui->selectedSoftwareInfo->setText("No software selected");
    }
}

void SoftwareManager::uninstallSoftware()
{
    int row = m_mainWindow->ui->softwareTable->currentRow();
    if (row < 0) {
        QMessageBox::information(m_mainWindow, "Uninstall", "Please select software to uninstall.");
        return;
    }

    QString softwareName = m_mainWindow->ui->softwareTable->item(row, 0)->text();
    QString softwareVersion = m_mainWindow->ui->softwareTable->item(row, 1)->text();

    // Find the software in our list
    InstalledSoftware selectedSoftware;
    bool found = false;
    
    for (const auto &software : m_installedSoftware) {
        if (software.name == softwareName && software.version == softwareVersion) {
            selectedSoftware = software;
            found = true;
            break;
        }
    }

    if (!found) {
        QMessageBox::warning(m_mainWindow, "Uninstall", "Selected software not found.");
        return;
    }

    // Check if software is running
    if (isSoftwareRunning(selectedSoftware.name)) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            m_mainWindow, 
            "Software Running", 
            QString("%1 appears to be running. Do you want to close it and continue uninstall?").arg(selectedSoftware.name),
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply == QMessageBox::Yes) {
            terminateProcess(selectedSoftware.name);
        } else {
            return;
        }
    }

    // Confirm uninstall
    QMessageBox::StandardButton confirm = QMessageBox::question(
        m_mainWindow,
        "Confirm Uninstall",
        QString("Are you sure you want to uninstall %1 %2?").arg(selectedSoftware.name).arg(selectedSoftware.version),
        QMessageBox::Yes | QMessageBox::No
    );

    if (confirm == QMessageBox::Yes) {
        m_mainWindow->ui->selectedSoftwareInfo->setText(
            QString("🔧 Uninstalling %1...").arg(selectedSoftware.name));
        
        executeUninstall(selectedSoftware.uninstallString.isEmpty() ? 
                        selectedSoftware.quietUninstallString : selectedSoftware.uninstallString);
        
        // Refresh the list after a delay
        QTimer::singleShot(3000, this, [this]() {
            refreshSoftware();
        });
    }
}

void SoftwareManager::forceUninstallSoftware()
{
    int row = m_mainWindow->ui->softwareTable->currentRow();
    if (row < 0) {
        QMessageBox::information(m_mainWindow, "Force Uninstall", "Please select software to uninstall.");
        return;
    }

    QString softwareName = m_mainWindow->ui->softwareTable->item(row, 0)->text();
    QString softwareVersion = m_mainWindow->ui->softwareTable->item(row, 1)->text();

    // Find the software in our list
    InstalledSoftware selectedSoftware;
    bool found = false;
    
    for (const auto &software : m_installedSoftware) {
        if (software.name == softwareName && software.version == softwareVersion) {
            selectedSoftware = software;
            found = true;
            break;
        }
    }

    if (!found) {
        QMessageBox::warning(m_mainWindow, "Force Uninstall", "Selected software not found.");
        return;
    }

    // Force close the software if running
    if (isSoftwareRunning(selectedSoftware.name)) {
        terminateProcess(selectedSoftware.name);
        QThread::msleep(1000); // Wait a bit for process to terminate
    }

    // Warning for force uninstall
    QMessageBox::StandardButton confirm = QMessageBox::warning(
        m_mainWindow,
        "Force Uninstall Warning",
        QString("Force Uninstall will attempt to remove %1 without user interaction.\n\n"
               "This may:\n"
               "• Remove system components\n"
               "• Cause instability\n"
               "• Leave residual files\n\n"
               "Are you sure you want to continue?").arg(selectedSoftware.name),
        QMessageBox::Yes | QMessageBox::No
    );

    if (confirm == QMessageBox::Yes) {
        m_mainWindow->ui->selectedSoftwareInfo->setText(
            QString("⚡ Force uninstalling %1...").arg(selectedSoftware.name));
        
        executeUninstall(selectedSoftware.quietUninstallString.isEmpty() ? 
                        selectedSoftware.uninstallString : selectedSoftware.quietUninstallString, true);
        
        // Refresh the list after a delay
        QTimer::singleShot(3000, this, [this]() {
            refreshSoftware();
        });
    }
}

void SoftwareManager::populateSoftwareTable()
{
    // Clear existing items
    m_mainWindow->ui->softwareTable->setRowCount(0);
    m_mainWindow->ui->softwareTable->setSortingEnabled(false);

    for (int i = 0; i < m_installedSoftware.size(); ++i) {
        const InstalledSoftware &software = m_installedSoftware[i];
        
        int row = m_mainWindow->ui->softwareTable->rowCount();
        m_mainWindow->ui->softwareTable->insertRow(row);

        // Software Name
        QTableWidgetItem *nameItem = new QTableWidgetItem(software.name);
        if (software.isSystemComponent) {
            nameItem->setBackground(QBrush(QColor(255, 243, 205))); // Light yellow for system components
            nameItem->setToolTip("System Component - Use Force Uninstall with caution");
        }
        m_mainWindow->ui->softwareTable->setItem(row, 0, nameItem);

        // Version
        QTableWidgetItem *versionItem = new QTableWidgetItem(software.version);
        m_mainWindow->ui->softwareTable->setItem(row, 1, versionItem);

        // Size
        QTableWidgetItem *sizeItem = new QTableWidgetItem(formatFileSize(software.size));
        sizeItem->setTextAlignment(Qt::AlignRight);
        m_mainWindow->ui->softwareTable->setItem(row, 2, sizeItem);

        // Install Date
        QTableWidgetItem *dateItem = new QTableWidgetItem(software.installDate);
        m_mainWindow->ui->softwareTable->setItem(row, 3, dateItem);
    }

    // Resize columns to content and enable sorting
    m_mainWindow->ui->softwareTable->resizeColumnsToContents();
    m_mainWindow->ui->softwareTable->setSortingEnabled(true);
    
    // Sort by name by default
    m_mainWindow->ui->softwareTable->sortByColumn(0, Qt::AscendingOrder);
}

QList<SoftwareManager::InstalledSoftware> SoftwareManager::getInstalledSoftwareFromRegistry()
{
    QList<InstalledSoftware> softwareList;
    
    if (m_cancelScan) {
        return softwareList;
    }

    // Registry paths for installed software
    QStringList registryPaths = {
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
    };

    int totalPaths = registryPaths.size();
    int currentPath = 0;

    for (const QString &registryPath : registryPaths) {
        if (m_cancelScan) break;

        currentPath++;
        int progress = (currentPath * 100) / totalPaths;
        emit scanProgressUpdated(progress, QString("🔍 Scanning registry: %1...").arg(registryPath));

        QSettings registry(registryPath, QSettings::NativeFormat);
        QStringList groups = registry.childGroups();
        
        int totalGroups = groups.size();
        int currentGroup = 0;

        for (const QString &group : groups) {
            if (m_cancelScan) break;

            currentGroup++;
            if (currentGroup % 10 == 0) { // Update progress every 10 items
                int groupProgress = (currentGroup * 100) / totalGroups;
                int overallProgress = ((currentPath - 1) * 100 + groupProgress) / totalPaths;
                emit scanProgressUpdated(overallProgress, 
                    QString("📋 Processing software (%1/%2)...").arg(currentGroup).arg(totalGroups));
            }

            registry.beginGroup(group);
            
            InstalledSoftware software;
            software.guid = group;
            software.name = registry.value("DisplayName").toString();
            software.version = registry.value("DisplayVersion").toString();
            software.publisher = registry.value("Publisher").toString();
            software.uninstallString = registry.value("UninstallString").toString();
            software.quietUninstallString = registry.value("QuietUninstallString").toString();
            
            // Get install date
            QString installDate = registry.value("InstallDate").toString();
            if (!installDate.isEmpty() && installDate.length() == 8) {
                // Format: YYYYMMDD
                software.installDate = QString("%1-%2-%3")
                    .arg(installDate.left(4))
                    .arg(installDate.mid(4, 2))
                    .arg(installDate.mid(6, 2));
            } else {
                software.installDate = "Unknown";
            }
            
            // Get size from install location (this is the slow part)
            QString installLocation = registry.value("InstallLocation").toString();
            if (!installLocation.isEmpty() && QDir(installLocation).exists()) {
                software.size = getSoftwareSize(installLocation).toLongLong();
            } else {
                // Try to get size from registry if available
                QVariant sizeVar = registry.value("EstimatedSize");
                if (sizeVar.isValid()) {
                    software.size = sizeVar.toLongLong() * 1024; // Convert KB to bytes
                } else {
                    software.size = 0;
                }
            }
            
            // Check if system component
            software.isSystemComponent = registry.value("SystemComponent").toBool() || 
                                        software.publisher.contains("Microsoft", Qt::CaseInsensitive) ||
                                        software.name.contains("Update", Qt::CaseInsensitive);
            
            // Only add if it has a display name and uninstall capability
            if (!software.name.isEmpty() && 
                (!software.uninstallString.isEmpty() || !software.quietUninstallString.isEmpty())) {
                softwareList.append(software);
            }
            
            registry.endGroup();
            
            // Small delay to keep UI responsive
            if (currentGroup % 5 == 0) {
                QThread::msleep(1);
            }
        }
    }
    
    return softwareList;
}

QString SoftwareManager::getSoftwareSize(const QString &installPath)
{
    if (installPath.isEmpty() || !QDir(installPath).exists() || m_cancelScan) {
        return "0";
    }
    
    qint64 totalSize = 0;
    QDir dir(installPath);
    
    // Get file sizes recursively with cancellation check
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (m_cancelScan) break;
        
        if (entry.isDir()) {
            totalSize += getSoftwareSize(entry.absoluteFilePath()).toLongLong();
        } else {
            totalSize += entry.size();
        }
    }
    
    return QString::number(totalSize);
}

QString SoftwareManager::formatFileSize(qint64 bytes)
{
    if (bytes == 0) return "0 B";
    
    const QString sizes[] = {"B", "KB", "MB", "GB"};
    int i = 0;
    double size = bytes;
    
    while (size >= 1024 && i < 3) {
        size /= 1024;
        i++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', 1).arg(sizes[i]);
}

void SoftwareManager::executeUninstall(const QString &uninstallString, bool force)
{
    if (uninstallString.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "Uninstall Error", "No uninstall command available for this software.");
        return;
    }
    
    QProcess process;
    QString command = uninstallString;
    
    // Handle MsiExec uninstall
    if (command.contains("MsiExec.exe", Qt::CaseInsensitive)) {
        if (force) {
            command += " /quiet /norestart";
        } else {
            command += " /passive";
        }
    }
    // Handle typical uninstallers
    else if (force) {
        if (command.contains("/S") || command.contains("/silent") || command.contains("/quiet")) {
            // Already has silent parameter
        } else if (command.endsWith(".exe", Qt::CaseInsensitive)) {
            command += " /S"; // Common silent uninstall parameter
        }
    }
    
    // Execute the uninstall command
    bool started = process.startDetached(command);
    
    if (started) {
        m_mainWindow->ui->selectedSoftwareInfo->setText(
            QString("✅ Uninstall command executed successfully.\nThe software uninstaller should open shortly."));
    } else {
        QMessageBox::warning(m_mainWindow, "Uninstall Error", 
                           "Failed to start uninstall process. You may need to run this application as Administrator.");
    }
}

bool SoftwareManager::isSoftwareRunning(const QString &softwareName)
{
    // Simple check for common processes
    QString processName = softwareName.split(" ").first() + ".exe";
    processName = processName.replace("/", "").replace("\\", "");
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(hSnapshot, &pe)) {
        do {
            QString currentProcess = QString::fromWCharArray(pe.szExeFile);
            if (currentProcess.compare(processName, Qt::CaseInsensitive) == 0) {
                CloseHandle(hSnapshot);
                return true;
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    
    CloseHandle(hSnapshot);
    return false;
}

void SoftwareManager::terminateProcess(const QString &processName)
{
    QString cleanProcessName = processName.split(" ").first() + ".exe";
    cleanProcessName = cleanProcessName.replace("/", "").replace("\\", "");
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(hSnapshot, &pe)) {
        do {
            QString currentProcess = QString::fromWCharArray(pe.szExeFile);
            if (currentProcess.compare(cleanProcessName, Qt::CaseInsensitive) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
                break;
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    
    CloseHandle(hSnapshot);
}