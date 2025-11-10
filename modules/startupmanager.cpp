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

    // Scan Registry locations
    scanRegistryStartup(programs, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", "Registry", "Current User");
    scanRegistryStartup(programs, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", "Registry", "Local Machine");
    scanRegistryStartup(programs, "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Run", "Registry", "WOW6432Node");

    // Scan RunOnce keys
    scanRegistryStartup(programs, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce", "Registry", "RunOnce");
    scanRegistryStartup(programs, "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", "Registry", "RunOnce");

    // Scan Startup folders
    scanStartupFolders(programs);

    // Scan Services
    scanServices(programs);

    // Scan Scheduled Tasks
    scanScheduledTasks(programs);

    removeDuplicates(programs);
    sortProgramsByName(programs);

    return programs;
}

void StartupManager::scanRegistryStartup(QList<StartupProgram> &programs, const QString &registryPath, const QString &startupType, const QString &location)
{
    QSettings registry(registryPath, QSettings::NativeFormat);
    QStringList keys = registry.childKeys();

    for (const QString &key : keys)
    {
        QString value = registry.value(key).toString();

        if (value.isEmpty())
            continue;

        // Check for disabled entries in the registry
        bool isEnabled = true;

        // Check common registry locations for disabled entries
        QSettings disabledRegistry(registryPath + "Disabled", QSettings::NativeFormat);
        if (disabledRegistry.contains(key))
        {
            isEnabled = false;
        }

        // Also check if the value itself indicates it's disabled
        if (value.contains("--disabled") || value.contains("-disable") ||
            key.contains("(disabled)", Qt::CaseInsensitive))
        {
            isEnabled = false;
        }

        StartupProgram program;
        program.name = key;
        program.status = isEnabled ? "Enabled" : "Disabled";
        program.startupType = startupType;
        program.command = value;
        program.location = registryPath;
        program.isEnabled = isEnabled;
        program.impact = calculateProgramImpact(key, value);

        programs.append(program);
    }
}

void StartupManager::scanStartupFolders(QList<StartupProgram> &programs)
{
    // Get actual startup folder paths
    wchar_t *startupPath = nullptr;

    // Current User startup folder
    if (SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &startupPath) == S_OK)
    {
        QString folderPath = QString::fromWCharArray(startupPath);
        CoTaskMemFree(startupPath);

        QDir startupFolder(folderPath);
        if (startupFolder.exists())
        {
            scanStartupFolder(programs, startupFolder, "Startup Folder", "Current User");
        }
    }

    // All Users startup folder
    if (SHGetKnownFolderPath(FOLDERID_CommonStartup, 0, nullptr, &startupPath) == S_OK)
    {
        QString folderPath = QString::fromWCharArray(startupPath);
        CoTaskMemFree(startupPath);

        QDir startupFolder(folderPath);
        if (startupFolder.exists())
        {
            scanStartupFolder(programs, startupFolder, "Startup Folder", "All Users");
        }
    }
}

void StartupManager::scanStartupFolder(QList<StartupProgram> &programs, QDir &startupFolder, const QString &startupType, const QString &location)
{
    QFileInfoList entries = startupFolder.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::System);

    // Check for disabled folder
    QDir disabledFolder(startupFolder.absolutePath() + "/Disabled");
    QFileInfoList disabledEntries;
    if (disabledFolder.exists())
    {
        disabledEntries = disabledFolder.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::System);
    }

    // Add enabled entries
    for (const QFileInfo &entry : entries)
    {
        QString suffix = entry.suffix().toLower();
        if (suffix == "lnk" || suffix == "exe" || suffix == "bat" || suffix == "cmd")
        {
            StartupProgram program;
            program.name = entry.baseName();
            program.status = "Enabled";
            program.startupType = startupType;
            program.command = entry.absoluteFilePath();
            program.location = location;
            program.isEnabled = true;
            program.impact = calculateProgramImpact(program.name, program.command);

            programs.append(program);
        }
    }

    // Add disabled entries
    for (const QFileInfo &entry : disabledEntries)
    {
        QString suffix = entry.suffix().toLower();
        if (suffix == "lnk" || suffix == "exe" || suffix == "bat" || suffix == "cmd")
        {
            StartupProgram program;
            program.name = entry.baseName();
            program.status = "Disabled";
            program.startupType = startupType;
            program.command = entry.absoluteFilePath();
            program.location = location + " (Disabled)";
            program.isEnabled = false;
            program.impact = calculateProgramImpact(program.name, program.command);

            programs.append(program);
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

    if (startupType == "Registry")
    {
        success = changeRegistryStartupState(programName, location, enable);
    }
    else if (startupType == "Startup Folder")
    {
        success = changeStartupFolderState(programName, location, enable);
    }
    else if (startupType == "Service")
    {
        success = changeServiceState(programName, enable);
    }
    else if (startupType == "Scheduled Task")
    {
        success = changeScheduledTaskState(programName, enable);
    }

    if (success)
    {
        // Refresh the list to show updated states
        refreshStartupPrograms();
    }

    return success;
}

bool StartupManager::changeRegistryStartupState(const QString &programName, const QString &registryPath, bool enable)
{
    QSettings registry(registryPath, QSettings::NativeFormat);

    if (enable)
    {
        // Try to restore from the disabled backup
        QSettings disabledRegistry(registryPath + "Disabled", QSettings::NativeFormat);
        if (disabledRegistry.contains(programName))
        {
            QString originalValue = disabledRegistry.value(programName).toString();
            registry.setValue(programName, originalValue);
            disabledRegistry.remove(programName);
            bool success = (registry.status() == QSettings::NoError);

            if (success)
            {
                qDebug() << "Enabled registry startup:" << programName;
            }
            return success;
        }
        else
        {
            QMessageBox::warning(m_mainWindow, "Error",
                                 QString("Cannot enable '%1' - no backup found in disabled registry.").arg(programName));
            return false;
        }
    }
    else
    {
        // Disable: save to backup and remove from main
        if (registry.contains(programName))
        {
            QString originalValue = registry.value(programName).toString();

            // Save to backup location first
            QSettings disabledRegistry(registryPath + "Disabled", QSettings::NativeFormat);
            disabledRegistry.setValue(programName, originalValue);

            // Then remove from main location
            registry.remove(programName);
            bool success = (registry.status() == QSettings::NoError);

            if (success)
            {
                qDebug() << "Disabled registry startup:" << programName;
            }
            return success;
        }
        else
        {
            QMessageBox::warning(m_mainWindow, "Error",
                                 QString("Cannot disable '%1' - not found in registry.").arg(programName));
            return false;
        }
    }
}

bool StartupManager::changeStartupFolderState(const QString &programName, const QString &folderPath, bool enable)
{
    QDir startupFolder(folderPath);
    if (!startupFolder.exists())
    {
        qDebug() << "Startup folder doesn't exist:" << folderPath;
        return false;
    }

    QDir disabledFolder(startupFolder.absolutePath() + "/Disabled");
    if (!disabledFolder.exists())
    {
        if (!disabledFolder.mkpath("."))
        {
            qDebug() << "Failed to create disabled folder:" << disabledFolder.absolutePath();
            return false;
        }
    }

    if (enable)
    {
        // Enable: move from disabled folder back to startup folder
        QFileInfoList disabledEntries = disabledFolder.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : disabledEntries)
        {
            if (entry.baseName().compare(programName, Qt::CaseInsensitive) == 0)
            {
                QString newPath = startupFolder.absolutePath() + "/" + entry.fileName();
                bool success = QFile::rename(entry.absoluteFilePath(), newPath);
                if (success)
                {
                    qDebug() << "Enabled startup folder item:" << programName;
                }
                else
                {
                    qDebug() << "Failed to enable startup folder item:" << programName;
                }
                return success;
            }
        }
        qDebug() << "Program not found in disabled folder:" << programName;
        return false;
    }
    else
    {
        // Disable: move from startup folder to disabled folder
        QFileInfoList entries = startupFolder.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : entries)
        {
            if (entry.baseName().compare(programName, Qt::CaseInsensitive) == 0)
            {
                QString newPath = disabledFolder.absolutePath() + "/" + entry.fileName();
                bool success = QFile::rename(entry.absoluteFilePath(), newPath);
                if (success)
                {
                    qDebug() << "Disabled startup folder item:" << programName;
                }
                else
                {
                    qDebug() << "Failed to disable startup folder item:" << programName;
                }
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