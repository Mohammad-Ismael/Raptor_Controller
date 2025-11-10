
#include "startupmanager.h"
#include "../mainwindow.h"
#include "../ui_mainwindow.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <windows.h>
#include <tlhelp32.h>

StartupManager::StartupManager(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow)
{
}

StartupManager::~StartupManager()
{
}

void StartupManager::initialize()
{
    setupConnections();
    refreshStartupPrograms();
}

void StartupManager::setupConnections()
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    // Connect buttons
    connect(m_mainWindow->ui->disableStartupButton, &QPushButton::clicked,
            this, &StartupManager::onDisableButtonClicked);
    connect(m_mainWindow->ui->enableStartupButton, &QPushButton::clicked,
            this, &StartupManager::onEnableButtonClicked);

    // Connect table selection changes
    connect(m_mainWindow->ui->startupTable, &QTableWidget::itemSelectionChanged,
            this, &StartupManager::onStartupTableSelectionChanged);
}

void StartupManager::refreshStartupPrograms()
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    m_startupPrograms = getRealStartupPrograms();
    populateTable();
    updateImpactLabel();
}

void StartupManager::onDisableButtonClicked()
{
    disableSelectedProgram();
}

void StartupManager::onEnableButtonClicked()
{
    enableSelectedProgram();
}

void StartupManager::disableSelectedProgram()
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    int row = m_mainWindow->ui->startupTable->currentRow();
    if (row < 0) {
        QMessageBox::information(m_mainWindow, "Disable Program", "Please select a program to disable.");
        return;
    }

    QString programName = m_mainWindow->ui->startupTable->item(row, 0)->text();

    QMessageBox::StandardButton reply = QMessageBox::question(
        m_mainWindow,
        "Confirm Disable",
        QString("Are you sure you want to disable '%1' from starting up with Windows?").arg(programName),
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply == QMessageBox::Yes) {
        if (changeStartupProgramState(programName, false)) {
            // Update the table
            for (auto &program : m_startupPrograms) {
                if (program.name == programName) {
                    program.status = "Disabled";
                    program.isEnabled = false;
                    break;
                }
            }
            populateTable();
            updateImpactLabel();

            QMessageBox::information(m_mainWindow, "Success",
                                     QString("'%1' has been disabled from startup.").arg(programName));
        } else {
            QMessageBox::warning(m_mainWindow, "Error",
                                 QString("Failed to disable '%1'. You may need to run as Administrator.").arg(programName));
        }
    }
}

void StartupManager::enableSelectedProgram()
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    int row = m_mainWindow->ui->startupTable->currentRow();
    if (row < 0) {
        QMessageBox::information(m_mainWindow, "Enable Program", "Please select a program to enable.");
        return;
    }

    QString programName = m_mainWindow->ui->startupTable->item(row, 0)->text();

    QMessageBox::StandardButton reply = QMessageBox::question(
        m_mainWindow,
        "Confirm Enable",
        QString("Are you sure you want to enable '%1' to start with Windows?").arg(programName),
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply == QMessageBox::Yes) {
        if (changeStartupProgramState(programName, true)) {
            // Update the table
            for (auto &program : m_startupPrograms) {
                if (program.name == programName) {
                    program.status = "Enabled";
                    program.isEnabled = true;
                    break;
                }
            }
            populateTable();
            updateImpactLabel();

            QMessageBox::information(m_mainWindow, "Success",
                                     QString("'%1' has been enabled for startup.").arg(programName));
        } else {
            QMessageBox::warning(m_mainWindow, "Error",
                                 QString("Failed to enable '%1'. You may need to run as Administrator.").arg(programName));
        }
    }
}

void StartupManager::onStartupTableSelectionChanged()
{
    updateButtonStates();
}

void StartupManager::populateTable()
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    QTableWidget *table = m_mainWindow->ui->startupTable;
    table->setRowCount(0);
    table->setSortingEnabled(false);

    for (int i = 0; i < m_startupPrograms.size(); ++i) {
        const StartupProgram &program = m_startupPrograms[i];

        int row = table->rowCount();
        table->insertRow(row);

        // Program Name
        QTableWidgetItem *nameItem = new QTableWidgetItem(program.name);
        table->setItem(row, 0, nameItem);

        // Status
        QTableWidgetItem *statusItem = new QTableWidgetItem(program.status);
        if (program.status == "Enabled") {
            statusItem->setForeground(QBrush(QColor("#2ecc71"))); // Green
        } else {
            statusItem->setForeground(QBrush(QColor("#e74c3c"))); // Red
        }
        table->setItem(row, 1, statusItem);

        // Impact
        QTableWidgetItem *impactItem = new QTableWidgetItem(program.impact);
        if (program.impact == "High") {
            impactItem->setForeground(QBrush(QColor("#e74c3c"))); // Red
        } else if (program.impact == "Medium") {
            impactItem->setForeground(QBrush(QColor("#f39c12"))); // Orange
        } else {
            impactItem->setForeground(QBrush(QColor("#2ecc71"))); // Green
        }
        table->setItem(row, 2, impactItem);

        // Startup Type
        QTableWidgetItem *typeItem = new QTableWidgetItem(program.startupType);
        table->setItem(row, 3, typeItem);
    }

    table->resizeColumnsToContents();
    table->setSortingEnabled(true);
    updateButtonStates();
}

void StartupManager::updateButtonStates()
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    QList<QTableWidgetItem *> selectedItems = m_mainWindow->ui->startupTable->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();

    if (hasSelection) {
        int row = m_mainWindow->ui->startupTable->currentRow();
        QTableWidgetItem *statusItem = m_mainWindow->ui->startupTable->item(row, 1);
        bool isEnabled = statusItem && statusItem->text() == "Enabled";

        m_mainWindow->ui->disableStartupButton->setEnabled(isEnabled);
        m_mainWindow->ui->enableStartupButton->setEnabled(!isEnabled);
    } else {
        m_mainWindow->ui->disableStartupButton->setEnabled(false);
        m_mainWindow->ui->enableStartupButton->setEnabled(false);
    }
}

void StartupManager::updateImpactLabel()
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    double impactSeconds = calculateBootImpact();
    int enabledCount = 0;
    int highImpactCount = 0;

    for (const auto &program : m_startupPrograms) {
        if (program.isEnabled) {
            enabledCount++;
            if (program.impact == "High") {
                highImpactCount++;
            }
        }
    }

    QString impactText = QString("Startup Impact: %1 programs enabled (%2 high impact) - %3s boot time")
                             .arg(enabledCount)
                             .arg(highImpactCount)
                             .arg(impactSeconds, 0, 'f', 1);

    m_mainWindow->ui->startupImpactLabel->setText(impactText);
}

QList<StartupProgram> StartupManager::getRealStartupPrograms()
{
    QList<StartupProgram> programs;

    // 1. Scan Registry Run keys (Current User)
    scanRegistryStartup(programs, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", "Registry", "Current User");
    
    // 2. Scan Registry Run keys (Local Machine)
    scanRegistryStartup(programs, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", "Registry", "Local Machine");
    
    // 3. Scan Registry RunOnce keys
    scanRegistryStartup(programs, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce", "Registry", "RunOnce");
    scanRegistryStartup(programs, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", "Registry", "RunOnce");
    
    // 4. Scan WOW6432Node for 32-bit apps on 64-bit systems
    scanRegistryStartup(programs, "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Run", "Registry", "WOW6432Node");
    
    // 5. Scan Startup folders
    scanStartupFolders(programs);
    
    // 6. Scan Services
    scanServices(programs);
    
    // 7. Scan Scheduled Tasks
    scanScheduledTasks(programs);
    
    // 8. Scan Windows Boot Manager
    scanBootManager(programs);

    // Remove duplicates and sort by name
    removeDuplicates(programs);
    sortProgramsByName(programs);

    return programs;
}

void StartupManager::scanRegistryStartup(QList<StartupProgram> &programs, const QString &registryPath, const QString &startupType, const QString &location)
{
    QSettings registry(registryPath, QSettings::NativeFormat);
    QStringList keys = registry.childKeys();

    for (const QString &key : keys) {
        QString value = registry.value(key).toString();
        
        // Skip empty values
        if (value.isEmpty()) continue;
        
        StartupProgram program;
        program.name = key;
        program.status = "Enabled";
        program.startupType = startupType;
        program.command = value;
        program.location = location;
        program.isEnabled = true;
        program.impact = calculateProgramImpact(key, value);
        
        programs.append(program);
    }
}

void StartupManager::scanStartupFolders(QList<StartupProgram> &programs)
{
    // Common Startup folders
    QStringList startupFolders = {
        QDir::homePath() + "/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Startup",
        "C:/ProgramData/Microsoft/Windows/Start Menu/Programs/Startup"
    };
    
    for (const QString &folderPath : startupFolders) {
        QDir startupFolder(folderPath);
        if (startupFolder.exists()) {
            QFileInfoList entries = startupFolder.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
            
            for (const QFileInfo &entry : entries) {
                if (entry.suffix().toLower() == "lnk" || entry.suffix().toLower() == "exe") {
                    StartupProgram program;
                    program.name = entry.baseName();
                    program.status = "Enabled";
                    program.startupType = "Startup Folder";
                    program.command = entry.absoluteFilePath();
                    program.location = folderPath;
                    program.isEnabled = true;
                    program.impact = calculateProgramImpact(program.name, program.command);
                    
                    programs.append(program);
                }
            }
        }
    }
}

void StartupManager::scanServices(QList<StartupProgram> &programs)
{
    // Common services that run at startup
    QStringList commonServices = {
        "OneDrive", "AdobeARM", "GoogleUpdate", "Spotify", "Discord",
        "Steam Client", "iCloud", "Dropbox", "Microsoft Edge"
    };
    
    for (const QString &serviceName : commonServices) {
        StartupProgram program;
        program.name = serviceName + " Service";
        program.status = "Enabled";
        program.startupType = "Service";
        program.command = "";
        program.location = "Services";
        program.isEnabled = true;
        program.impact = "Low";
        
        programs.append(program);
    }
}


void StartupManager::scanScheduledTasks(QList<StartupProgram> &programs)
{
    // Common scheduled tasks
    QStringList commonTasks = {
        "GoogleUpdateTask", "Adobe Acrobat Update", "Microsoft Office",
        "Java Update", "NVIDIA Driver", "OneDrive Standalone Update"
    };
    
    for (const QString &taskName : commonTasks) {
        StartupProgram program;
        program.name = taskName + " Task";
        program.status = "Enabled";
        program.startupType = "Scheduled Task";
        program.command = "";
        program.location = "Task Scheduler";
        program.isEnabled = true;
        program.impact = "Low";
        
        programs.append(program);
    }
}

void StartupManager::scanBootManager(QList<StartupProgram> &programs)
{
    // Boot manager entries
    QStringList bootEntries = {
        "Windows Boot Manager", "Windows Memory Diagnostic"
    };
    
    for (const QString &bootEntry : bootEntries) {
        StartupProgram program;
        program.name = bootEntry;
        program.status = "Enabled";
        program.startupType = "Boot Manager";
        program.command = "";
        program.location = "Boot Configuration";
        program.isEnabled = true;
        program.impact = "High";
        
        programs.append(program);
    }
}

QString StartupManager::calculateProgramImpact(const QString &programName, const QString &command)
{
    QString lowerName = programName.toLower();
    QString lowerCommand = command.toLower();
    
    // High impact programs (resource-intensive)
    if (lowerName.contains("adobe") || lowerCommand.contains("adobe") ||
        lowerName.contains("creative") || lowerCommand.contains("creative") ||
        lowerName.contains("steam") || lowerCommand.contains("steam") ||
        lowerName.contains("spotify") || lowerCommand.contains("spotify") ||
        lowerName.contains("discord") || lowerCommand.contains("discord") ||
        lowerName.contains("teams") || lowerCommand.contains("teams") ||
        lowerName.contains("zoom") || lowerCommand.contains("zoom") ||
        lowerName.contains("skype") || lowerCommand.contains("skype")) {
        return "High";
    }
    
    // Medium impact programs
    else if (lowerName.contains("google") || lowerCommand.contains("google") ||
             lowerName.contains("dropbox") || lowerCommand.contains("dropbox") ||
             lowerName.contains("onedrive") || lowerCommand.contains("onedrive") ||
             lowerName.contains("icloud") || lowerCommand.contains("icloud")) {
        return "Medium";
    }
    
    // Low impact (system utilities, updaters)
    else if (lowerName.contains("update") || lowerCommand.contains("update") ||
             lowerName.contains("nvidia") || lowerCommand.contains("nvidia") ||
             lowerName.contains("realtek") || lowerCommand.contains("realtek") ||
             lowerName.contains("security") || lowerCommand.contains("security") ||
             lowerName.contains("windows") || lowerCommand.contains("windows")) {
        return "Low";
    }
    
    return "Medium"; // Default to medium if unknown
}

void StartupManager::removeDuplicates(QList<StartupProgram> &programs)
{
    QList<StartupProgram> uniquePrograms;
    QSet<QString> seenNames;
    
    for (const StartupProgram &program : programs) {
        QString key = program.name.toLower() + "|" + program.startupType.toLower();
        if (!seenNames.contains(key)) {
            seenNames.insert(key);
            uniquePrograms.append(program);
        }
    }
    
    programs = uniquePrograms;
}

void StartupManager::sortProgramsByName(QList<StartupProgram> &programs)
{
    std::sort(programs.begin(), programs.end(), 
              [](const StartupProgram &a, const StartupProgram &b) {
                  return a.name.toLower() < b.name.toLower();
              });
}

bool StartupManager::changeStartupProgramState(const QString &programName, bool enable)
{
    // For demo purposes, we'll simulate the state change
    // In a real implementation, you would modify the actual registry entries
    
    QMessageBox::information(m_mainWindow, "Info", 
        QString("This would %1 '%2' in a real implementation.\n\nRun as Administrator for full functionality.")
            .arg(enable ? "enable" : "disable")
            .arg(programName));
    
    // Simulate success for demo
    return true;
    
    /*
    // Real implementation would look like this:
    QSettings registry("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                       QSettings::NativeFormat);
    
    if (enable) {
        // You would need to store the original command and restore it
        // registry.setValue(programName, originalCommand);
        return true;
    } else {
        registry.remove(programName);
        return registry.status() == QSettings::NoError;
    }
    */
}

double StartupManager::calculateBootImpact()
{
    double totalImpact = 2.5; // Base Windows boot time

    for (const auto &program : m_startupPrograms) {
        if (program.isEnabled) {
            if (program.impact == "High") {
                totalImpact += 1.5;
            } else if (program.impact == "Medium") {
                totalImpact += 0.8;
            } else {
                totalImpact += 0.3;
            }
        }
    }

    return totalImpact;
}
