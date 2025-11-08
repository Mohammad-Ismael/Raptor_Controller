#include "windowsutils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>
#include <QApplication>

WindowsUtils::WindowsUtils(QObject *parent) : QObject(parent)
{
    initializeCleanerItems();
}

void WindowsUtils::initializeCleanerItems()
{
    m_cleanerItems.clear();
    
    // SAFE locations only for Windows 10/11 - CORRECTED PATHS
    m_cleanerItems.append({
        "Temporary Files", 
        "Windows and user temporary files",
        getUserTempPath(),
        {"*.*"},  // Scan ALL files recursively
        0,
        true,
        true
    });
    
    m_cleanerItems.append({
        "Windows Update Cache", 
        "Windows Update temporary files", 
        "C:/Windows/Temp",
        {"*.*"},
        0,
        true,
        true
    });
    
    m_cleanerItems.append({
        "System Log Files", 
        "System event and error logs",
        "C:/Windows/Logs",
        {"*.*"},
        0,
        true,
        true
    });
    
    m_cleanerItems.append({
        "Memory Dump Files", 
        "System memory dump files",
        "C:/Windows",
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
        "C:/Windows/Prefetch", 
        {"*.pf"},
        0,
        true,
        true
    });
}

QVector<CleanerItem> WindowsUtils::scanJunkFiles()
{
    QVector<CleanerItem> result;

    for (int i = 0; i < m_cleanerItems.size(); ++i)
    {
        CleanerItem scannedItem = m_cleanerItems[i];

        // Calculate size for this location
        QDir dir(scannedItem.path);
        if (dir.exists())
        {
            scannedItem.size = calculateDirectorySize(dir);
        }
        else
        {
            scannedItem.size = 0;
        }

        result.append(scannedItem);
    }

    return result;
}

qint64 WindowsUtils::cleanJunkFiles(const QVector<CleanerItem> &itemsToClean)
{
    qint64 totalFreed = 0;

    for (int i = 0; i < itemsToClean.size(); ++i)
    {
        const CleanerItem &item = itemsToClean[i];
        if (item.isSelected && item.isSafe)
        {
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
    if (!dir.exists()) return 0;
    
    qint64 size = 0;
    int processedCount = 0;
    
    // Count files in current directory
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo &file : files) {
        size += file.size();
        processedCount++;
        
        // Keep GUI responsive
        if (processedCount % 100 == 0) {
            QApplication::processEvents();
        }
    }
    
    // Recursively count subdirectories - THIS IS WHAT WAS MISSING!
    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subDir : dirs) {
        size += calculateFolderSize(subDir.absoluteFilePath());
        processedCount++;
        
        // Keep GUI responsive
        if (processedCount % 10 == 0) {
            QApplication::processEvents();
        }
    }
    
    return size;
}

qint64 WindowsUtils::calculateDirectorySize(const QDir &dir)
{
    qint64 size = 0;

    // Calculate file sizes
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo &file : files)
    {
        size += file.size();
    }

    // Calculate subdirectory sizes
    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subDir : dirs)
    {
        size += calculateDirectorySize(QDir(subDir.absoluteFilePath()));
    }

    return size;
}

void WindowsUtils::deleteAllFilesRecursive(const QDir &dir)
{
    // Delete all files in current directory
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo &file : files) {
        QFile::remove(file.absoluteFilePath());
    }

    // Recursively delete files in subdirectories
    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subDir : dirs) {
        deleteAllFilesRecursive(QDir(subDir.absoluteFilePath()));
    }
}

void WindowsUtils::deleteFilesByPattern(const QString &path, const QStringList &patterns)
{
    QDir dir(path);
    if (!dir.exists()) return;
    
    // If patterns is "*.*", scan all files recursively
    if (patterns.contains("*.*")) {
        // Recursively delete all files in this directory and subdirectories
        deleteAllFilesRecursive(dir);
        return;
    }
    
    // For specific patterns, use the existing logic
    for (int i = 0; i < patterns.size(); ++i) {
        const QString &pattern = patterns[i];
        QFileInfoList files = dir.entryInfoList(QStringList() << pattern, 
                                               QDir::Files | QDir::Hidden | QDir::System);
        for (int j = 0; j < files.size(); ++j) {
            const QFileInfo &file = files[j];
            QFile::remove(file.absoluteFilePath());
        }
    }
}



QString WindowsUtils::getWindowsTempPath() const
{
    return "C:/Windows/Temp";  // Fixed - was going to wrong location
}

QString WindowsUtils::getUserTempPath() const
{
    return QDir::tempPath();  // This should be correct
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

QVector<CleanerItem> WindowsUtils::getCleanerItemsTemplate() const
{
    return m_cleanerItems; // Return the template without sizes
}

qint64 WindowsUtils::calculateFolderSizeQuick(const QString &path)
{
    QDir dir(path);
    if (!dir.exists())
        return 0;

    qint64 size = 0;

    // Only count immediate files (don't recurse into subdirectories)
    // This makes it much faster and non-blocking
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);

    for (const QFileInfo &file : files)
    {
        size += file.size();

        // Process events every 50 files to keep GUI responsive
        static int fileCount = 0;
        fileCount++;
        if (fileCount % 50 == 0)
        {
            QApplication::processEvents();
        }
    }

    return size;
}
