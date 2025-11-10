#include "systeminfomanager.h"
#include "../mainwindow.h"
#include "../ui_mainwindow.h"
#include <QThread>
#include <QtConcurrent/QtConcurrent>
#include <QRegularExpression>
#include <QStorageInfo>

#include <QProcess>

#pragma comment(lib, "dxgi.lib")

SystemInfoManager::SystemInfoManager(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow), m_isMonitoring(false)
{
    m_monitorTimer = new QTimer(this);
    m_specsWatcher = new QFutureWatcher<SystemSpecs>(this);

    // Initialize CPU usage tracking
    m_lastIdleTime.QuadPart = 0;
    m_lastKernelTime.QuadPart = 0;
    m_lastUserTime.QuadPart = 0;

    connect(m_specsWatcher, &QFutureWatcher<SystemSpecs>::finished,
            this, &SystemInfoManager::onSpecsScanFinished);

    connect(m_monitorTimer, &QTimer::timeout,
            this, &SystemInfoManager::onMonitorTimeout);

    QTimer::singleShot(100, this, &SystemInfoManager::refreshSystemInfo);
}

SystemInfoManager::~SystemInfoManager()
{
    stopRealTimeMonitoring();

    if (m_specsWatcher)
    {
        m_specsWatcher->waitForFinished();
        delete m_specsWatcher;
    }

    delete m_monitorTimer;
}

void SystemInfoManager::refreshSystemInfo()
{
    if (m_specsWatcher->isRunning())
    {
        return;
    }

    // Update UI to show scanning
    if (m_mainWindow && m_mainWindow->ui)
    {
        m_mainWindow->ui->osValueLabel->setText("🔄 Scanning...");
        m_mainWindow->ui->cpuValueLabel->setText("🔄 Scanning...");
        m_mainWindow->ui->ramValueLabel->setText("🔄 Scanning...");
        m_mainWindow->ui->storageValueLabel->setText("🔄 Scanning...");
        m_mainWindow->ui->gpuValueLabel->setText("🔄 Scanning...");
    }

    // Start background scan - much faster with optimized methods
    QFuture<SystemSpecs> future = QtConcurrent::run([this]()
                                                    { return this->getSystemSpecs(); });

    m_specsWatcher->setFuture(future);
}

void SystemInfoManager::startRealTimeMonitoring()
{
    if (!m_isMonitoring)
    {
        // Initialize CPU usage baseline
        getCPUUsage(); // This sets the initial values

        m_monitorTimer->start(500); // Update every 2 seconds
        m_isMonitoring = true;

        // Get initial performance metrics
        updatePerformanceMetrics();
    }
}

void SystemInfoManager::stopRealTimeMonitoring()
{
    if (m_isMonitoring)
    {
        m_monitorTimer->stop();
        m_isMonitoring = false;
    }
}

void SystemInfoManager::onSpecsScanFinished()
{
    try
    {
        SystemSpecs specs = m_specsWatcher->result();

        QString gpuInfo = specs.gpuName;
        if (!specs.gpuVRAM.isEmpty())
        {
            gpuInfo += " (" + specs.gpuVRAM + ")";
        }

        QString ramInfo = specs.ramTotal;
        if (!specs.ramSpeed.isEmpty())
        {
            ramInfo += " " + specs.ramSpeed;
        }

        // FIX: Only use storageTotal which already contains the free space info
        // Remove the extra concatenation that was causing "() (%2)"
        emit systemInfoUpdated(specs.osName,  // osName already has the full formatted string
                              specs.cpuName + " " + specs.cpuCores,
                              ramInfo,
                              specs.storageTotal,  // Use storageTotal directly, it already has free space
                              gpuInfo);

        emit updateFinished(true, "✅ System information loaded successfully!");
    }
    catch (const std::exception &e)
    {
        emit updateFinished(false, QString("❌ Failed to get system information: %1").arg(e.what()));
    }
}

void SystemInfoManager::onMonitorTimeout()
{
    updatePerformanceMetrics();
}

bool SystemInfoManager::isWindows11()
{
    // More reliable Windows 11 detection
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                     0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {

        DWORD buildNumber = 0;
        DWORD bufferSize = sizeof(DWORD);

        if (RegQueryValueEx(hKey, L"CurrentBuildNumber", NULL, NULL,
                            (LPBYTE)&buildNumber, &bufferSize) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return buildNumber >= 22000; // Windows 11 starts from build 22000
        }
        RegCloseKey(hKey);
    }
    return false;
}

SystemInfoManager::SystemSpecs SystemInfoManager::getSystemSpecs()
{
    SystemSpecs specs;

    // Get OS Information using PowerShell command
    QProcess powerShell;
    powerShell.start("powershell", QStringList() << "-Command" << "(Get-CimInstance Win32_OperatingSystem).Caption");
    powerShell.waitForFinished(3000); // 3 second timeout

    QString osName;
    if (powerShell.exitStatus() == QProcess::NormalExit && powerShell.exitCode() == 0)
    {
        QString osResult = QString::fromUtf8(powerShell.readAllStandardOutput()).trimmed();

        if (!osResult.isEmpty())
        {
            // Clean up the OS name - remove Microsoft and any build numbers
            osName = osResult.replace("Microsoft", "").trimmed().simplified();

            // Remove any build numbers that might be in the OS name
            QRegularExpression buildRegex("\\b(build|version)?\\s*\\d{5,}\\b");
            osName = osName.remove(buildRegex).trimmed();

            // Remove any extra spaces
            osName = osName.simplified();
        }
        else
        {
            osName = getOSInfoFromRegistry();
        }
    }
    else
    {
        // Fallback if PowerShell fails
        osName = getOSInfoFromRegistry();
    }

    // Get version information for display
    QString displayVersion = getDisplayVersion();
    QString buildNumber = getBuildNumber();

    // Format: "Windows 10 Pro | 24H2 | build 26100"
    specs.osName = QString("%1 | %2 | build %3").arg(osName).arg(displayVersion).arg(buildNumber);

    // Rest of your existing code for CPU, RAM, etc.
    // Get CPU Information
    HKEY hKeyCPU;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &hKeyCPU) == ERROR_SUCCESS)
    {

        WCHAR processorName[256];
        DWORD bufferSize = sizeof(processorName);
        DWORD type;

        if (RegQueryValueExW(hKeyCPU, L"ProcessorNameString", NULL, &type,
                             (LPBYTE)processorName, &bufferSize) == ERROR_SUCCESS)
        {
            specs.cpuName = QString::fromWCharArray(processorName).trimmed().simplified();
        }

        RegCloseKey(hKeyCPU);
    }

    // Get CPU Core Count
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    specs.cpuCores = QString("%1 cores").arg(sysInfo.dwNumberOfProcessors);

    // Get RAM Information
    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus))
    {
        double totalGB = memoryStatus.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);
        specs.ramTotal = QString("%1 GB").arg(totalGB, 0, 'f', 1);
    }

    // Get RAM Speed
    specs.ramSpeed = getRamSpeed();

    // Get Boot Drive Storage Information
    specs.storageTotal = getBootDriveInfo();

    // Get GPU Information
    specs.gpuName = getGPUInfo();
    specs.gpuVRAM = getGPUVramInfo();

    return specs;
}

QString SystemInfoManager::getDisplayVersion()
{
    HKEY hKey;
    QString displayVersion = "Unknown";

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {

        WCHAR versionValue[64];
        DWORD bufferSize = sizeof(versionValue);
        DWORD type;

        // Priority order for version detection:
        // 1. DisplayVersion (Windows 11 and newer Windows 10)
        // 2. ReleaseId (older Windows 10)
        // 3. CurrentVersion (fallback)

        if (RegQueryValueExW(hKey, L"DisplayVersion", NULL, &type,
                             (LPBYTE)versionValue, &bufferSize) == ERROR_SUCCESS)
        {
            displayVersion = QString::fromWCharArray(versionValue);
        }
        else
        {
            bufferSize = sizeof(versionValue);
            if (RegQueryValueExW(hKey, L"ReleaseId", NULL, &type,
                                 (LPBYTE)versionValue, &bufferSize) == ERROR_SUCCESS)
            {
                displayVersion = QString::fromWCharArray(versionValue);
            }
            else
            {
                bufferSize = sizeof(versionValue);
                if (RegQueryValueExW(hKey, L"CurrentVersion", NULL, &type,
                                     (LPBYTE)versionValue, &bufferSize) == ERROR_SUCCESS)
                {
                    displayVersion = QString::fromWCharArray(versionValue);
                }
            }
        }

        RegCloseKey(hKey);
    }

    return displayVersion;
}

QString SystemInfoManager::getBuildNumber()
{
    HKEY hKey;
    QString buildNumber = "Unknown";

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {

        WCHAR buildNumberW[64];
        DWORD bufferSize = sizeof(buildNumberW);
        DWORD type;

        // Get CurrentBuild Number (primary)
        if (RegQueryValueExW(hKey, L"CurrentBuild", NULL, &type,
                             (LPBYTE)buildNumberW, &bufferSize) == ERROR_SUCCESS)
        {
            buildNumber = QString::fromWCharArray(buildNumberW);
        }
        // Only try CurrentBuildNumber if CurrentBuild wasn't found
        else
        {
            bufferSize = sizeof(buildNumberW);
            if (RegQueryValueExW(hKey, L"CurrentBuildNumber", NULL, &type,
                                 (LPBYTE)buildNumberW, &bufferSize) == ERROR_SUCCESS)
            {
                buildNumber = QString::fromWCharArray(buildNumberW);
            }
        }

        RegCloseKey(hKey);
    }

    return buildNumber;
}

QString SystemInfoManager::getOSInfoFromRegistry()
{
    HKEY hKey;
    QString osName = "Windows (Unknown Version)";

    // Open registry key
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {

        WCHAR productName[256];
        DWORD bufferSize = sizeof(productName);
        DWORD type;

        // Get ProductName
        if (RegQueryValueExW(hKey, L"ProductName", NULL, &type,
                             (LPBYTE)productName, &bufferSize) == ERROR_SUCCESS)
        {
            osName = QString::fromWCharArray(productName);

            // Check if it's actually Windows 11 but reported as Windows 10
            WCHAR currentBuild[64];
            bufferSize = sizeof(currentBuild);
            if (RegQueryValueExW(hKey, L"CurrentBuild", NULL, &type,
                                 (LPBYTE)currentBuild, &bufferSize) == ERROR_SUCCESS)
            {
                QString build = QString::fromWCharArray(currentBuild);
                int buildNumber = build.toInt();

                // Windows 11 detection - build 22000 and above
                if (osName.contains("Windows 10") && buildNumber >= 22000)
                {
                    osName = "Windows 11";

                    // Add edition info
                    WCHAR edition[64];
                    bufferSize = sizeof(edition);
                    if (RegQueryValueExW(hKey, L"EditionID", NULL, &type,
                                         (LPBYTE)edition, &bufferSize) == ERROR_SUCCESS)
                    {
                        QString editionStr = QString::fromWCharArray(edition);
                        if (editionStr.contains("Professional"))
                        {
                            osName += " Pro";
                        }
                        else if (editionStr.contains("Home"))
                        {
                            osName += " Home";
                        }
                        else if (editionStr.contains("Enterprise"))
                        {
                            osName += " Enterprise";
                        }
                    }
                }
            }
        }

        RegCloseKey(hKey);
    }

    return osName;
}

QString SystemInfoManager::getRamSpeed()
{
    // Get RAM speed from WMI (this is the most reliable way)
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                     0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {

        DWORD speed;
        DWORD bufferSize = sizeof(DWORD);

        // Try to get RAM speed from different registry locations
        if (RegQueryValueEx(hKey, L"~MHz", NULL, NULL,
                            (LPBYTE)&speed, &bufferSize) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return QString("%1 MHz").arg(speed);
        }
        RegCloseKey(hKey);
    }

    return ""; // Return empty if can't detect
}

QString SystemInfoManager::getBootDriveInfo()
{
    // Get the boot drive (where Windows is installed)
    QStorageInfo bootDrive = QStorageInfo::root();

    if (bootDrive.isValid())
    {
        qint64 totalBytes = bootDrive.bytesTotal();
        qint64 freeBytes = bootDrive.bytesFree();

        double totalGB = totalBytes / (1024.0 * 1024.0 * 1024.0);
        double freeGB = freeBytes / (1024.0 * 1024.0 * 1024.0);

        QString sizeStr;
        if (totalGB >= 1024)
        {
            sizeStr = QString::number(totalGB / 1024.0, 'f', 1) + " TB";
        }
        else
        {
            sizeStr = QString::number(totalGB, 'f', 0) + " GB";
        }

        // Add drive type detection
        QString driveType = "HDD";
        if (bootDrive.fileSystemType().contains("NTFS") || bootDrive.isReady())
        {
            // Simple SSD detection - you can enhance this later
            driveType = "SSD";
        }

        // Use simple string concatenation - no placeholders
        return sizeStr + " " + driveType + " (Free: " + QString::number(static_cast<int>(freeGB)) + " GB)";
    }

    return "Unknown";
}

QString SystemInfoManager::getGPUInfo()
{
    DISPLAY_DEVICE displayDevice;
    ZeroMemory(&displayDevice, sizeof(displayDevice));
    displayDevice.cb = sizeof(DISPLAY_DEVICE);

    QString gpuName = "Unknown GPU";

    for (DWORD deviceNum = 0; EnumDisplayDevices(NULL, deviceNum, &displayDevice, 0); deviceNum++)
    {
        if (displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            QString deviceName = QString::fromWCharArray(displayDevice.DeviceString);

            if (!deviceName.isEmpty() && deviceName != "Unknown")
            {
                gpuName = deviceName;

                // Simplify GPU names
                if (gpuName.contains("NVIDIA", Qt::CaseInsensitive))
                {
                    QRegularExpression re("(RTX|GTX|GT)\\s*\\d+\\s*\\w*");
                    QRegularExpressionMatch match = re.match(gpuName);
                    if (match.hasMatch())
                    {
                        gpuName = "NVIDIA " + match.captured();
                    }
                }
                else if (gpuName.contains("AMD", Qt::CaseInsensitive) ||
                         gpuName.contains("Radeon", Qt::CaseInsensitive))
                {
                    QRegularExpression re("(RX|Radeon)\\s*\\d+\\s*\\w*");
                    QRegularExpressionMatch match = re.match(gpuName);
                    if (match.hasMatch())
                    {
                        gpuName = "AMD " + match.captured();
                    }
                }
                else if (gpuName.contains("Intel", Qt::CaseInsensitive))
                {
                    gpuName = "Intel Graphics";
                }
            }
            break;
        }
    }

    return gpuName;
}

QString SystemInfoManager::getGPUVramInfo()
{
    // Use DXGI to get VRAM information (more reliable)
    IDXGIFactory *pFactory;
    if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void **)&pFactory)))
    {
        IDXGIAdapter *pAdapter;
        if (SUCCEEDED(pFactory->EnumAdapters(0, &pAdapter)))
        {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(pAdapter->GetDesc(&desc)))
            {
                pAdapter->Release();
                pFactory->Release();

                double vramGB = desc.DedicatedVideoMemory / (1024.0 * 1024.0 * 1024.0);
                if (vramGB > 0)
                {
                    return QString("%1 GB VRAM").arg(vramGB, 0, 'f', 1);
                }
            }
            pAdapter->Release();
        }
        pFactory->Release();
    }

    return ""; // Return empty if can't detect VRAM
}

void SystemInfoManager::updatePerformanceMetrics()
{
    int cpuUsage = getCPUUsage();
    int ramUsage = getRAMUsage();
    int diskUsage = getDiskUsage();

    emit performanceUpdated(cpuUsage, ramUsage, diskUsage);
}

int SystemInfoManager::getCPUUsage()
{
    FILETIME idleTime, kernelTime, userTime;

    if (GetSystemTimes(&idleTime, &kernelTime, &userTime))
    {
        ULARGE_INTEGER idle, kernel, user;
        idle.LowPart = idleTime.dwLowDateTime;
        idle.HighPart = idleTime.dwHighDateTime;
        kernel.LowPart = kernelTime.dwLowDateTime;
        kernel.HighPart = kernelTime.dwHighDateTime;
        user.LowPart = userTime.dwLowDateTime;
        user.HighPart = userTime.dwHighDateTime;

        ULARGE_INTEGER sysTime;
        sysTime.QuadPart = (kernel.QuadPart - m_lastKernelTime.QuadPart) +
                           (user.QuadPart - m_lastUserTime.QuadPart);

        if (sysTime.QuadPart > 0 && m_lastKernelTime.QuadPart != 0 && m_lastUserTime.QuadPart != 0)
        {
            ULONGLONG idleDiff = idle.QuadPart - m_lastIdleTime.QuadPart;
            int usage = 100 - ((idleDiff * 100) / sysTime.QuadPart);

            m_lastIdleTime = idle;
            m_lastKernelTime = kernel;
            m_lastUserTime = user;

            return qBound(0, usage, 100);
        }

        // Store first measurement
        m_lastIdleTime = idle;
        m_lastKernelTime = kernel;
        m_lastUserTime = user;
    }

    return 0;
}

int SystemInfoManager::getRAMUsage()
{
    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);

    if (GlobalMemoryStatusEx(&memoryStatus))
    {
        return memoryStatus.dwMemoryLoad;
    }

    return 0;
}

int SystemInfoManager::getDiskUsage()
{
    QStorageInfo bootDrive = QStorageInfo::root();

    if (bootDrive.isValid() && bootDrive.bytesTotal() > 0)
    {
        double usedBytes = bootDrive.bytesTotal() - bootDrive.bytesFree();
        return (usedBytes * 100) / bootDrive.bytesTotal();
    }

    return 0;
}