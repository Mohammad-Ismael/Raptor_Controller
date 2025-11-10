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
#include <QRegularExpression>
#include <windows.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <psapi.h>
#include <shlobj.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

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
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    connect(m_mainWindow->ui->disableStartupButton, &QPushButton::clicked,
            this, &StartupManager::onDisableButtonClicked);
    connect(m_mainWindow->ui->enableStartupButton, &QPushButton::clicked,
            this, &StartupManager::onEnableButtonClicked);
    connect(m_mainWindow->ui->startupTable, &QTableWidget::itemSelectionChanged,
            this, &StartupManager::onStartupTableSelectionChanged);
}

void StartupManager::refreshStartupPrograms()
{
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

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
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    int row = m_mainWindow->ui->startupTable->currentRow();
    if (row < 0)
    {
        QMessageBox::information(m_mainWindow, "Disable Program", "Please select a program to disable.");
        return;
    }

    QString programName = m_mainWindow->ui->startupTable->item(row, 0)->text();
    QString location = m_startupPrograms[row].location;
    QString startupType = m_startupPrograms[row].startupType;

    QMessageBox::StandardButton reply = QMessageBox::question(
        m_mainWindow,
        "Confirm Disable",
        QString("Are you sure you want to disable '%1' from starting up with Windows?").arg(programName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        if (changeStartupProgramState(programName, location, startupType, false))
        {
            // Don't show immediate success - let the refresh show the actual state
            refreshStartupPrograms();

            // Check if it actually got disabled
            bool actuallyDisabled = false;
            for (const auto &program : m_startupPrograms)
            {
                if (program.name == programName && !program.isEnabled)
                {
                    actuallyDisabled = true;
                    break;
                }
            }

            if (actuallyDisabled)
            {
                QMessageBox::information(m_mainWindow, "Success",
                                         QString("'%1' has been disabled from startup.").arg(programName));
            }
            else
            {
                QMessageBox::warning(m_mainWindow, "Warning",
                                     QString("'%1' may still appear as enabled. The change might require a restart to take effect.").arg(programName));
            }
        }
        else
        {
            QMessageBox::warning(m_mainWindow, "Error",
                                 QString("Failed to disable '%1'. You may need to run as Administrator.").arg(programName));
        }
    }
}

void StartupManager::enableSelectedProgram()
{
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    int row = m_mainWindow->ui->startupTable->currentRow();
    if (row < 0)
    {
        QMessageBox::information(m_mainWindow, "Enable Program", "Please select a program to enable.");
        return;
    }

    QString programName = m_mainWindow->ui->startupTable->item(row, 0)->text();
    QString location = m_startupPrograms[row].location;
    QString startupType = m_startupPrograms[row].startupType;

    QMessageBox::StandardButton reply = QMessageBox::question(
        m_mainWindow,
        "Confirm Enable",
        QString("Are you sure you want to enable '%1' to start with Windows?").arg(programName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        if (changeStartupProgramState(programName, location, startupType, true))
        {
            refreshStartupPrograms();
            QMessageBox::information(m_mainWindow, "Success",
                                     QString("'%1' has been enabled for startup.").arg(programName));
        }
        else
        {
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
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    QTableWidget *table = m_mainWindow->ui->startupTable;
    table->setRowCount(0);
    table->setSortingEnabled(false);

    for (int i = 0; i < m_startupPrograms.size(); ++i)
    {
        const StartupProgram &program = m_startupPrograms[i];

        int row = table->rowCount();
        table->insertRow(row);

        // Program Name
        QTableWidgetItem *nameItem = new QTableWidgetItem(program.name);
        nameItem->setToolTip(program.command);
        table->setItem(row, 0, nameItem);

        // Status
        QTableWidgetItem *statusItem = new QTableWidgetItem(program.status);
        if (program.status == "Enabled")
        {
            statusItem->setForeground(QBrush(QColor("#2ecc71")));
        }
        else
        {
            statusItem->setForeground(QBrush(QColor("#e74c3c")));
        }
        table->setItem(row, 1, statusItem);

        // Impact
        QTableWidgetItem *impactItem = new QTableWidgetItem(program.impact);
        if (program.impact == "High")
        {
            impactItem->setForeground(QBrush(QColor("#e74c3c")));
        }
        else if (program.impact == "Medium")
        {
            impactItem->setForeground(QBrush(QColor("#f39c12")));
        }
        else
        {
            impactItem->setForeground(QBrush(QColor("#2ecc71")));
        }
        table->setItem(row, 2, impactItem);

        // Startup Type
        QTableWidgetItem *typeItem = new QTableWidgetItem(program.startupType);
        typeItem->setToolTip(program.location);
        table->setItem(row, 3, typeItem);
    }

    table->resizeColumnsToContents();
    table->setSortingEnabled(true);
    updateButtonStates();
}

void StartupManager::updateButtonStates()
{
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    QList<QTableWidgetItem *> selectedItems = m_mainWindow->ui->startupTable->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();

    if (hasSelection)
    {
        int row = m_mainWindow->ui->startupTable->currentRow();
        QTableWidgetItem *statusItem = m_mainWindow->ui->startupTable->item(row, 1);
        bool isEnabled = statusItem && statusItem->text() == "Enabled";

        m_mainWindow->ui->disableStartupButton->setEnabled(isEnabled);
        m_mainWindow->ui->enableStartupButton->setEnabled(!isEnabled);
    }
    else
    {
        m_mainWindow->ui->disableStartupButton->setEnabled(false);
        m_mainWindow->ui->enableStartupButton->setEnabled(false);
    }
}

void StartupManager::updateImpactLabel()
{
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    double impactSeconds = calculateBootImpact();
    int enabledCount = 0;
    int highImpactCount = 0;

    for (const auto &program : m_startupPrograms)
    {
        if (program.isEnabled)
        {
            enabledCount++;
            if (program.impact == "High")
            {
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

    qDebug() << "Scanning startup locations that Task Manager monitors...";
    
    // Debug first to see actual state
    debugActualRegistryState();
    
    // Then scan normally
    scanRegistryStartup(programs, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", "Registry", "Current User");
    scanRegistryStartup(programs, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", "Registry", "Local Machine");
    scanStartupFolders(programs);
    
    qDebug() << "Total programs found:" << programs.size();
    
    removeDuplicates(programs);
    sortProgramsByName(programs);

    return programs;
}

bool StartupManager::shouldSkipProgram(const QString &name, const QString &command)
{
    QString lowerName = name.toLower();
    QString lowerCommand = command.toLower();
    
    // Only skip truly system-critical entries
    if (lowerName.contains("securityhealth") ||
        lowerName.contains("windowsdefender") ||
        lowerCommand.contains("system32\\securityhealth")) {
        return true;
    }
    
    // Don't skip Microsoft Store apps, Riot, Steam, etc.
    // Let Task Manager decide what to show
    
    return false;
}

void StartupManager::scanRegistryStartup(QList<StartupProgram> &programs, const QString &registryPath, const QString &startupType, const QString &location)
{
    QSettings registry(registryPath, QSettings::NativeFormat);
    QStringList keys = registry.childKeys();

    for (const QString &key : keys) {
        QString value = registry.value(key).toString();
        if (value.isEmpty()) continue;
        
        // For now, let's assume ALL programs in the main registry are ENABLED
        // and ALL programs in Disabled key are DISABLED
        // This is the simplest approach that should match Task Manager
        
        StartupProgram program;
        program.name = key;
        program.status = "Enabled";
        program.startupType = startupType;
        program.command = value;
        program.location = registryPath;
        program.isEnabled = true;
        program.impact = calculateProgramImpact(key, value);
        programs.append(program);
        
        qDebug() << "Found in MAIN registry (ENABLED):" << key;
    }
    
    // Check Disabled key
    QSettings disabledReg(registryPath + "Disabled", QSettings::NativeFormat);
    QStringList disabledKeys = disabledReg.childKeys();
    
    for (const QString &key : disabledKeys) {
        QString originalValue = disabledReg.value(key).toString();
        
        StartupProgram program;
        program.name = key;
        program.status = "Disabled";
        program.startupType = startupType;
        program.command = originalValue;
        program.location = registryPath + " (Disabled)";
        program.isEnabled = false;
        program.impact = calculateProgramImpact(key, originalValue);
        programs.append(program);
        qDebug() << "Found in DISABLED registry:" << key;
    }
}

bool StartupManager::isProgramEnabledInTaskManager(const QString &programName, const QString &registryPath, const QString &value)
{
    // Method 1: Check if value indicates it's disabled
    if (value.startsWith("DISABLED_") || 
        value.contains("--disabled") || 
        value.contains("/disabled")) {
        return false;
    }
    
    // Method 2: Check if it's in the official Windows Disabled key
    QSettings disabledReg(registryPath + "Disabled", QSettings::NativeFormat);
    if (disabledReg.contains(programName)) {
        return false;
    }
    
    // Method 3: Check common patterns that indicate disabled state
    if (programName.contains("(disabled)", Qt::CaseInsensitive)) {
        return false;
    }
    
    // If none of the above, assume it's enabled
    return true;
}


void StartupManager::scanStartupFolders(QList<StartupProgram> &programs)
{
    // Get actual startup folder paths
    wchar_t* startupPath = nullptr;
    
    // Current User startup folder
    if (SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &startupPath) == S_OK) {
        QString folderPath = QString::fromWCharArray(startupPath);
        CoTaskMemFree(startupPath);
        
        QDir startupFolder(folderPath);
        if (startupFolder.exists()) {
            scanStartupFolder(programs, startupFolder, "Startup Folder", "Current User");
        } else {
            qDebug() << "Startup folder doesn't exist:" << folderPath;
        }
    }
    
    // All Users startup folder
    if (SHGetKnownFolderPath(FOLDERID_CommonStartup, 0, nullptr, &startupPath) == S_OK) {
        QString folderPath = QString::fromWCharArray(startupPath);
        CoTaskMemFree(startupPath);
        
        QDir startupFolder(folderPath);
        if (startupFolder.exists()) {
            scanStartupFolder(programs, startupFolder, "Startup Folder", "All Users");
        } else {
            qDebug() << "Common startup folder doesn't exist:" << folderPath;
        }
    }
}

void StartupManager::scanStartupFolder(QList<StartupProgram> &programs, QDir &startupFolder, const QString &startupType, const QString &location)
{
    QString actualPath = startupFolder.absolutePath();
    qDebug() << "Scanning startup folder:" << actualPath;
    
    // Check for disabled folder
    QDir disabledFolder(startupFolder.absolutePath() + "/Disabled");
    
    // Scan ENABLED items (in main folder)
    QFileInfoList entries = startupFolder.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::System);
    for (const QFileInfo &entry : entries) {
        QString suffix = entry.suffix().toLower();
        if (suffix == "lnk" || suffix == "exe" || suffix == "bat" || suffix == "cmd") {
            StartupProgram program;
            program.name = entry.baseName();
            program.status = "Enabled";
            program.startupType = startupType;
            program.command = entry.absoluteFilePath();
            program.location = location;
            program.isEnabled = true;
            program.impact = calculateProgramImpact(program.name, program.command);
            
            programs.append(program);
            qDebug() << "Found ENABLED startup folder:" << program.name;
        }
    }
    
    // Scan DISABLED items (in disabled subfolder)
    if (disabledFolder.exists()) {
        QFileInfoList disabledEntries = disabledFolder.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::System);
        for (const QFileInfo &entry : disabledEntries) {
            QString suffix = entry.suffix().toLower();
            if (suffix == "lnk" || suffix == "exe" || suffix == "bat" || suffix == "cmd") {
                StartupProgram program;
                program.name = entry.baseName();
                program.status = "Disabled";
                program.startupType = startupType;
                program.command = entry.absoluteFilePath();
                program.location = location + " (Disabled)";
                program.isEnabled = false;
                program.impact = calculateProgramImpact(program.name, program.command);
                
                programs.append(program);
                qDebug() << "Found DISABLED startup folder:" << program.name;
            }
        }
    }
}

void StartupManager::scanServices(QList<StartupProgram> &programs)
{
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scManager)
        return;

    DWORD bytesNeeded = 0;
    DWORD serviceCount = 0;
    DWORD resumeHandle = 0;

    // First call to get required buffer size
    EnumServicesStatusEx(
        scManager,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        nullptr,
        0,
        &bytesNeeded,
        &serviceCount,
        &resumeHandle,
        nullptr);

    if (GetLastError() != ERROR_MORE_DATA)
    {
        CloseServiceHandle(scManager);
        return;
    }

    BYTE *buffer = new BYTE[bytesNeeded];
    ENUM_SERVICE_STATUS_PROCESS *services = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESS *>(buffer);

    if (EnumServicesStatusEx(
            scManager,
            SC_ENUM_PROCESS_INFO,
            SERVICE_WIN32,
            SERVICE_STATE_ALL,
            buffer,
            bytesNeeded,
            &bytesNeeded,
            &serviceCount,
            &resumeHandle,
            nullptr))
    {
        for (DWORD i = 0; i < serviceCount; i++)
        {
            ENUM_SERVICE_STATUS_PROCESS &service = services[i];

            QString serviceName = QString::fromWCharArray(service.lpDisplayName);
            QString lowerName = serviceName.toLower();

            // Filter out critical system services - SIMPLIFIED VERSION
            if (lowerName.contains("windows") ||
                lowerName.contains("microsoft") ||
                lowerName.contains("system32") ||
                lowerName.contains("svchost") ||
                lowerName.contains("runtime") ||
                lowerName.contains("kernel") ||
                lowerName.contains("driver") ||
                lowerName.contains("network") ||
                lowerName.contains("security") ||
                lowerName.contains("update") ||
                lowerName.contains("defender") ||
                lowerName.contains("firewall"))
            {
                continue; // Skip system services
            }

            // Open service to query configuration
            SC_HANDLE hService = OpenService(scManager, service.lpServiceName, SERVICE_QUERY_CONFIG);
            if (hService)
            {
                DWORD bytesNeededConfig = 0;
                QueryServiceConfig(hService, nullptr, 0, &bytesNeededConfig);

                if (bytesNeededConfig > 0)
                {
                    BYTE *configBuffer = new BYTE[bytesNeededConfig];
                    QUERY_SERVICE_CONFIG *serviceConfig = reinterpret_cast<QUERY_SERVICE_CONFIG *>(configBuffer);

                    if (QueryServiceConfig(hService, serviceConfig, bytesNeededConfig, &bytesNeededConfig))
                    {
                        // Check if service starts automatically and is not disabled
                        bool isEnabled = (serviceConfig->dwStartType == SERVICE_AUTO_START);

                        StartupProgram program;
                        program.name = serviceName;
                        program.status = isEnabled ? "Enabled" : "Disabled";
                        program.startupType = "Service";
                        program.command = QString::fromWCharArray(service.lpServiceName);
                        program.location = "Services";
                        program.isEnabled = isEnabled;
                        program.impact = calculateProgramImpact(program.name, program.command);

                        programs.append(program);
                    }

                    delete[] configBuffer;
                }
                CloseServiceHandle(hService);
            }
        }
    }

    delete[] buffer;
    CloseServiceHandle(scManager);
}

void StartupManager::scanScheduledTasks(QList<StartupProgram> &programs)
{
    // Use schtasks command to get scheduled tasks
    QProcess process;
    process.start("schtasks", QStringList() << "/query" << "/fo" << "csv" << "/nh");
    process.waitForFinished(3000); // 3 second timeout

    if (process.exitCode() != 0)
        return;

    QString output = process.readAllStandardOutput();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines)
    {
        QStringList fields = line.split(',');
        if (fields.size() >= 3)
        {
            QString taskName = fields[0].trimmed();
            if (taskName.startsWith('"') && taskName.endsWith('"'))
            {
                taskName = taskName.mid(1, taskName.length() - 2);
            }

            QString status = fields.size() > 2 ? fields[2].trimmed() : "";
            if (status.startsWith('"') && status.endsWith('"'))
            {
                status = status.mid(1, status.length() - 2);
            }

            // Look for tasks that run at startup, logon, or are user applications
            if ((taskName.contains("Startup", Qt::CaseInsensitive) ||
                 taskName.contains("Logon", Qt::CaseInsensitive) ||
                 taskName.contains("AtStartup", Qt::CaseInsensitive) ||
                 taskName.contains("AtLogon", Qt::CaseInsensitive)) &&
                !taskName.contains("Microsoft", Qt::CaseInsensitive))
            {

                bool isEnabled = (status != "Disabled");

                StartupProgram program;
                program.name = taskName;
                program.status = isEnabled ? "Enabled" : "Disabled";
                program.startupType = "Scheduled Task";
                program.command = "";
                program.location = "Task Scheduler";
                program.isEnabled = isEnabled;
                program.impact = "Low";

                programs.append(program);
            }
        }
    }
}

bool StartupManager::isUserService(const QString &serviceName)
{
    QString lowerName = serviceName.toLower();

    // Filter out critical system services
    if (lowerName.contains("windows") ||
        lowerName.contains("microsoft") ||
        lowerName.contains("system32") ||
        lowerName.contains("svchost") ||
        lowerName.contains("runtime") ||
        lowerName.contains("kernel") ||
        lowerName.contains("driver") ||
        lowerName.contains("network") ||
        lowerName.contains("security") ||
        lowerName.contains("update") ||
        lowerName.contains("defender") ||
        lowerName.contains("firewall"))
    {
        return false;
    }

    return true;
}

QString StartupManager::calculateProgramImpact(const QString &programName, const QString &command)
{
    QString lowerName = programName.toLower();
    QString lowerCommand = command.toLower();

    // High impact programs
    if (lowerName.contains("adobe") || lowerCommand.contains("adobe") ||
        lowerName.contains("creative") || lowerCommand.contains("creative") ||
        lowerName.contains("steam") || lowerCommand.contains("steam") ||
        lowerName.contains("spotify") || lowerCommand.contains("spotify") ||
        lowerName.contains("discord") || lowerCommand.contains("discord") ||
        lowerName.contains("teams") || lowerCommand.contains("teams") ||
        lowerName.contains("zoom") || lowerCommand.contains("zoom") ||
        lowerName.contains("skype") || lowerCommand.contains("skype"))
    {
        return "High";
    }

    // Medium impact programs
    else if (lowerName.contains("google") || lowerCommand.contains("google") ||
             lowerName.contains("dropbox") || lowerCommand.contains("dropbox") ||
             lowerName.contains("onedrive") || lowerCommand.contains("onedrive") ||
             lowerName.contains("icloud") || lowerCommand.contains("icloud"))
    {
        return "Medium";
    }

    // Low impact programs
    else if (lowerName.contains("update") || lowerCommand.contains("update") ||
             lowerName.contains("nvidia") || lowerCommand.contains("nvidia") ||
             lowerName.contains("realtek") || lowerCommand.contains("realtek") ||
             lowerName.contains("security") || lowerCommand.contains("security") ||
             lowerName.contains("windows") || lowerCommand.contains("windows"))
    {
        return "Low";
    }

    return "Medium";
}

void StartupManager::removeDuplicates(QList<StartupProgram> &programs)
{
    QList<StartupProgram> uniquePrograms;
    QSet<QString> seenNames;

    for (const StartupProgram &program : programs)
    {
        QString key = program.name.toLower() + "|" + program.startupType.toLower() + "|" + program.location;
        if (!seenNames.contains(key))
        {
            seenNames.insert(key);
            uniquePrograms.append(program);
        }
    }

    programs = uniquePrograms;
}

void StartupManager::sortProgramsByName(QList<StartupProgram> &programs)
{
    std::sort(programs.begin(), programs.end(),
              [](const StartupProgram &a, const StartupProgram &b)
              {
                  return a.name.toLower() < b.name.toLower();
              });
}

bool StartupManager::changeStartupProgramState(const QString &programName, const QString &location, const QString &startupType, bool enable)
{
    qDebug() << "Attempting to" << (enable ? "enable" : "disable") << programName 
             << "Type:" << startupType << "Location:" << location;
    
    bool success = false;
    
    if (startupType == "Registry") {
        success = changeRegistryStartupState(programName, location, enable);
    } else if (startupType == "Startup Folder") {
        success = changeStartupFolderState(programName, location, enable);
    } else {
        QMessageBox::warning(m_mainWindow, "Error", 
            QString("Cannot %1 '%2' - unsupported startup type: %3")
                .arg(enable ? "enable" : "disable")
                .arg(programName)
                .arg(startupType));
        return false;
    }
    
    if (success) {
        // Refresh to show updated state
        refreshStartupPrograms();
        QMessageBox::information(m_mainWindow, "Success", 
            QString("'%1' has been %2. Changes should appear in Task Manager after restarting it.")
                .arg(programName)
                .arg(enable ? "enabled" : "disabled"));
    } else {
        QMessageBox::warning(m_mainWindow, "Error", 
            QString("Failed to %1 '%2'. Please ensure you're running as Administrator.")
                .arg(enable ? "enable" : "disable")
                .arg(programName));
    }
    
    return success;
}

bool StartupManager::changeRegistryStartupState(const QString &programName, const QString &registryPath, bool enable)
{
    QSettings registry(registryPath, QSettings::NativeFormat);
    
    if (enable) {
        // ENABLE: Task Manager moves entries from "Disabled" back to main
        QSettings disabledReg(registryPath + "Disabled", QSettings::NativeFormat);
        if (disabledReg.contains(programName)) {
            QString originalValue = disabledReg.value(programName).toString();
            registry.setValue(programName, originalValue);
            disabledReg.remove(programName);
            qDebug() << "Enabled by moving from Disabled key:" << programName;
            return true;
        } else {
            QMessageBox::warning(m_mainWindow, "Error", 
                QString("Cannot enable '%1' - not found in disabled registry.").arg(programName));
            return false;
        }
    } else {
        // DISABLE: Task Manager moves entries to a "Disabled" subkey
        if (registry.contains(programName)) {
            QString originalValue = registry.value(programName).toString();
            
            // Move to Disabled key (this is what Task Manager does)
            QSettings disabledReg(registryPath + "Disabled", QSettings::NativeFormat);
            disabledReg.setValue(programName, originalValue);
            
            // Remove from main key
            registry.remove(programName);
            
            qDebug() << "Disabled by moving to Disabled key:" << programName;
            return true;
        } else {
            QMessageBox::warning(m_mainWindow, "Error", 
                QString("Cannot disable '%1' - not found in registry.").arg(programName));
            return false;
        }
    }
}

bool StartupManager::setWindowsStartupState(const QString &programName, const QString &registryPath, bool enable)
{
    // Windows Task Manager looks at specific registry values to determine disabled state
    // For HKEY_CURRENT_USER, it uses a different approach than HKEY_LOCAL_MACHINE
    
    if (registryPath.contains("HKEY_CURRENT_USER")) {
        return setUserStartupState(programName, enable);
    } else if (registryPath.contains("HKEY_LOCAL_MACHINE")) {
        return setSystemStartupState(programName, enable);
    }
    
    return false;
}

bool StartupManager::setUserStartupState(const QString &programName, bool enable)
{
    // For current user startup, Task Manager uses the registry value itself
    // If the value exists and is not empty, it's enabled
    // If we want to disable but keep it visible in Task Manager, we need a different approach
    
    if (enable) {
        // Restore from backup
        QSettings backup("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\RunDisabled", QSettings::NativeFormat);
        if (backup.contains(programName)) {
            QString value = backup.value(programName).toString();
            QSettings registry("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
            registry.setValue(programName, value);
            backup.remove(programName);
            return true;
        }
    } else {
        // Disable by moving to backup but keep a placeholder
        QSettings registry("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        if (registry.contains(programName)) {
            QString value = registry.value(programName).toString();
            
            // Save to backup
            QSettings backup("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\RunDisabled", QSettings::NativeFormat);
            backup.setValue(programName, value);
            
            // Set a special value that indicates disabled but keeps it visible
            registry.setValue(programName, "");
            return true;
        }
    }
    
    return false;
}

bool StartupManager::setSystemStartupState(const QString &programName, bool enable)
{
    // For system-wide startup, we need to use the proper Windows method
    // Task Manager looks at both the registry and policy settings
    
    if (enable) {
        // Enable: Restore original value
        QSettings backup("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\RunDisabled", QSettings::NativeFormat);
        if (backup.contains(programName)) {
            QString value = backup.value(programName).toString();
            QSettings registry("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
            registry.setValue(programName, value);
            backup.remove(programName);
            return true;
        }
    } else {
        // Disable: Use Windows standard method - remove from registry
        QSettings registry("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        if (registry.contains(programName)) {
            QString value = registry.value(programName).toString();
            
            // Save to backup
            QSettings backup("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\RunDisabled", QSettings::NativeFormat);
            backup.setValue(programName, value);
            
            // Remove from registry (this is what Task Manager expects)
            registry.remove(programName);
            return true;
        }
    }
    
    return false;
}


bool StartupManager::changeStartupFolderState(const QString &programName, const QString &folderType, bool enable)
{
    // Get the actual folder path based on folderType
    QString folderPath;
    wchar_t* startupPath = nullptr;
    
    if (folderType == "Current User") {
        if (SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &startupPath) == S_OK) {
            folderPath = QString::fromWCharArray(startupPath);
            CoTaskMemFree(startupPath);
        }
    } else if (folderType == "All Users") {
        if (SHGetKnownFolderPath(FOLDERID_CommonStartup, 0, nullptr, &startupPath) == S_OK) {
            folderPath = QString::fromWCharArray(startupPath);
            CoTaskMemFree(startupPath);
        }
    }
    
    if (folderPath.isEmpty()) {
        qDebug() << "Could not find startup folder path for:" << folderType;
        return false;
    }
    
    QDir startupFolder(folderPath);
    if (!startupFolder.exists()) {
        qDebug() << "Startup folder doesn't exist:" << folderPath;
        return false;
    }

    QDir disabledFolder(startupFolder.absolutePath() + "/Disabled");
    
    if (enable) {
        // Enable: Move from disabled folder back to startup folder
        if (!disabledFolder.exists()) {
            qDebug() << "No disabled folder found for enabling:" << programName;
            return false;
        }
        
        QFileInfoList disabledEntries = disabledFolder.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : disabledEntries) {
            if (entry.baseName().compare(programName, Qt::CaseInsensitive) == 0) {
                QString newPath = startupFolder.absolutePath() + "/" + entry.fileName();
                bool success = QFile::rename(entry.absoluteFilePath(), newPath);
                qDebug() << "Enabled startup folder item:" << programName << "Success:" << success;
                return success;
            }
        }
        qDebug() << "Program not found in disabled folder:" << programName;
        return false;
        
    } else {
        // Disable: Move from startup folder to disabled folder
        if (!disabledFolder.exists()) {
            if (!disabledFolder.mkpath(".")) {
                qDebug() << "Failed to create disabled folder";
                return false;
            }
        }
        
        QFileInfoList entries = startupFolder.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : entries) {
            if (entry.baseName().compare(programName, Qt::CaseInsensitive) == 0) {
                QString newPath = disabledFolder.absolutePath() + "/" + entry.fileName();
                bool success = QFile::rename(entry.absoluteFilePath(), newPath);
                qDebug() << "Disabled startup folder item:" << programName << "Success:" << success;
                return success;
            }
        }
        qDebug() << "Program not found in startup folder:" << programName;
        return false;
    }
}

bool StartupManager::changeServiceState(const QString &programName, bool enable)
{
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scManager)
    {
        qDebug() << "Failed to open Service Control Manager";
        return false;
    }

    // Try to find the service
    SC_HANDLE service = nullptr;
    QString serviceName = programName;

    // Remove common suffixes to get the actual service name
    if (serviceName.endsWith(" Service"))
    {
        serviceName.chop(9); // Remove " Service"
    }

    // Try opening with the modified name first
    service = OpenService(scManager, serviceName.toStdWString().c_str(), SERVICE_CHANGE_CONFIG);
    if (!service)
    {
        // Try with the original name
        service = OpenService(scManager, programName.toStdWString().c_str(), SERVICE_CHANGE_CONFIG);
    }

    if (!service)
    {
        qDebug() << "Failed to open service:" << programName;
        CloseServiceHandle(scManager);
        return false;
    }

    DWORD startType = enable ? SERVICE_AUTO_START : SERVICE_DISABLED;
    BOOL success = ChangeServiceConfig(
        service,
        SERVICE_NO_CHANGE, // service type
        startType,         // start type
        SERVICE_NO_CHANGE, // error control
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    if (success)
    {
        qDebug() << (enable ? "Enabled" : "Disabled") << "service:" << programName;
    }
    else
    {
        qDebug() << "Failed to change service state:" << programName << "Error:" << GetLastError();
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return success;
}

bool StartupManager::changeScheduledTaskState(const QString &programName, bool enable)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);

    QStringList args;
    if (enable)
    {
        args << "/change" << "/tn" << programName << "/enable";
    }
    else
    {
        args << "/change" << "/tn" << programName << "/disable";
    }

    process.start("schtasks", args);

    if (!process.waitForStarted(3000))
    {
        qDebug() << "Failed to start schtasks process";
        return false;
    }

    if (!process.waitForFinished(10000))
    { // 10 second timeout
        qDebug() << "schtasks process timed out";
        process.kill();
        return false;
    }

    bool success = (process.exitCode() == 0);

    if (success)
    {
        qDebug() << (enable ? "Enabled" : "Disabled") << "scheduled task:" << programName;
    }
    else
    {
        QString errorOutput = process.readAllStandardError();
        if (errorOutput.isEmpty())
        {
            errorOutput = process.readAllStandardOutput();
        }
        qDebug() << "Failed to change task state:" << programName << "Error:" << errorOutput;
    }

    return success;
}

double StartupManager::calculateBootImpact()
{
    double totalImpact = 2.5; // Base Windows boot time

    for (const auto &program : m_startupPrograms)
    {
        if (program.isEnabled)
        {
            if (program.impact == "High")
            {
                totalImpact += 1.5;
            }
            else if (program.impact == "Medium")
            {
                totalImpact += 0.8;
            }
            else
            {
                totalImpact += 0.3;
            }
        }
    }

    return totalImpact;
}


void StartupManager::debugActualRegistryState()
{
    qDebug() << "=== ACTUAL REGISTRY STATE ===";
    
    // Check HKEY_CURRENT_USER
    QSettings hkcuRun("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    QStringList hkcuKeys = hkcuRun.childKeys();
    qDebug() << "HKCU Run (" << hkcuKeys.size() << "items):";
    for (const QString &key : hkcuKeys) {
        QString value = hkcuRun.value(key).toString();
        qDebug() << "  " << key << "=" << value;
    }
    
    // Check HKEY_LOCAL_MACHINE  
    QSettings hklmRun("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    QStringList hklmKeys = hklmRun.childKeys();
    qDebug() << "HKLM Run (" << hklmKeys.size() << "items):";
    for (const QString &key : hklmKeys) {
        QString value = hklmRun.value(key).toString();
        qDebug() << "  " << key << "=" << value;
    }
    
    // Check Disabled keys
    QSettings hkcuDisabled("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\RunDisabled", QSettings::NativeFormat);
    QSettings hklmDisabled("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunDisabled", QSettings::NativeFormat);
    
    qDebug() << "HKCU Disabled:" << hkcuDisabled.childKeys();
    qDebug() << "HKLM Disabled:" << hklmDisabled.childKeys();
    qDebug() << "=== END REGISTRY STATE ===";
}