#include "windowsutils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>

WindowsUtils::WindowsUtils(QObject *parent) : QObject(parent)
{
    initializeCleanerItems();
}

void WindowsUtils::initializeCleanerItems()
{
    m_cleanerItems.clear();
    
    // SAFE locations only for Windows 10/11
    m_cleanerItems.append({
        "Temporary Files", 
        "Windows and user temporary files",
        getUserTempPath(),
        {"*.tmp", "*.temp", "*.log", "~*.*", "_*.*"},
        0,
        true,
        true
    });
    
    m_cleanerItems.append({
        "Windows Update Cache", 
        "Windows Update temporary files",
        getWindowsTempPath(),
        {"*.*"},
        0,
        true,
        true
    });
    
    m_cleanerItems.append({
        "System Log Files", 
        "System event and error logs",
        getWindowsLogsPath(),
        {"*.log", "*.etl", "*.evtx"},
        0,
        true,
        true
    });
    
    m_cleanerItems.append({
        "Memory Dump Files", 
        "System memory dump files",
        getMemoryDumpPath(),
        {"*.dmp", "*.hdmp"},
        0,
        true,
        true
    });
    
    m_cleanerItems.append({
        "Thumbnail Cache", 
        "Windows thumbnail cache",
        getThumbnailCachePath(),
        {"*.db", "thumbcache_*.db"},
        0,
        true,
        true
    });
    
    m_cleanerItems.append({
        "Prefetch Files", 
        "Windows prefetch files",
        getPrefetchPath(),
        {"*.pf"},
        0,
        true,
        true
    });
}

QVector<CleanerItem> WindowsUtils::scanJunkFiles()
{
    QVector<CleanerItem> result;
    
    for (int i = 0; i < m_cleanerItems.size(); ++i) {
        CleanerItem scannedItem = m_cleanerItems[i];
        
        // Calculate size for this location
        QDir dir(scannedItem.path);
        if (dir.exists()) {
            scannedItem.size = calculateDirectorySize(dir);
        } else {
            scannedItem.size = 0;
        }
        
        result.append(scannedItem);
    }
    
    return result;
}

qint64 WindowsUtils::cleanJunkFiles(const QVector<CleanerItem> &itemsToClean)
{
    qint64 totalFreed = 0;
    
    for (int i = 0; i < itemsToClean.size(); ++i) {
        const CleanerItem &item = itemsToClean[i];
        if (item.isSelected && item.isSafe) {
            qint64 beforeSize = calculateDirectorySize(QDir(item.path));
            deleteFilesByPattern(item.path, item.filePatterns);
            qint64 afterSize = calculateDirectorySize(QDir(item.path));
            totalFreed += (beforeSize - afterSize);
        }
    }
    
    return totalFreed;
}

qint64 WindowsUtils::calculateFolderSize(const QString &path)
{
    QDir dir(path);
    return calculateDirectorySize(dir);
}

qint64 WindowsUtils::calculateDirectorySize(const QDir &dir)
{
    qint64 size = 0;
    
    // Calculate file sizes
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo &file : files) {
        size += file.size();
    }
    
    // Calculate subdirectory sizes
    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subDir : dirs) {
        size += calculateDirectorySize(QDir(subDir.absoluteFilePath()));
    }
    
    return size;
}

void WindowsUtils::deleteFilesByPattern(const QString &path, const QStringList &patterns)
{
    QDir dir(path);
    if (!dir.exists()) return;
    
    // Delete files matching patterns
    for (int i = 0; i < patterns.size(); ++i) {
        const QString &pattern = patterns[i];
        QFileInfoList files = dir.entryInfoList(QStringList() << pattern, 
                                               QDir::Files | QDir::Hidden | QDir::System);
        for (int j = 0; j < files.size(); ++j) {
            const QFileInfo &file = files[j];
            QFile::remove(file.absoluteFilePath());
        }
    }
    
    // Clean empty subdirectories (safe operation)
    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (int i = 0; i < dirs.size(); ++i) {
        const QFileInfo &subDir = dirs[i];
        QString subDirPath = subDir.absoluteFilePath();
        QDir subDirObj(subDirPath);
        
        // Only delete if directory is empty
        if (subDirObj.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
            subDirObj.removeRecursively();
        }
    }
}

// Windows-specific path getters
QString WindowsUtils::getWindowsTempPath() const
{
    return QDir::cleanPath(QDir::tempPath() + "/.."); // Go to main Temp directory
}

QString WindowsUtils::getUserTempPath() const
{
    return QDir::tempPath();
}

QString WindowsUtils::getPrefetchPath() const
{
    return "C:/Windows/Prefetch";
}

QString WindowsUtils::getThumbnailCachePath() const
{
    QString localAppData = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir::cleanPath(localAppData + "/../Local/Microsoft/Windows/Explorer");
}

QString WindowsUtils::getWindowsLogsPath() const
{
    return "C:/Windows/Logs";
}

QString WindowsUtils::getMemoryDumpPath() const
{
    return "C:/Windows";
}