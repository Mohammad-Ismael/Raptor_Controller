#include "windowsutils.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

WindowsUtils::WindowsUtils(QObject *parent)
    : QObject(parent)
{
    initializeCleanerItems();
}

void WindowsUtils::initializeCleanerItems()
{
    m_cleanerItems.clear();

    // Create CleanerItem objects properly
    m_cleanerItems.append(CleanerItem(
        "Temporary Files",
        "Windows and user temporary files",
        getUserTempPath(),
        QStringList{"*.*"},  // Scan ALL files recursively
        0,
        true,
        true
    ));

    m_cleanerItems.append(CleanerItem(
        "Windows Update Cache",
        "Windows Update temporary files",
        "C:/Windows/Temp",
        QStringList{"*.*"},
        0,
        true,
        true
    ));

    m_cleanerItems.append(CleanerItem(
        "System Log Files",
        "System event and error logs",
        "C:/Windows/Logs",
        QStringList{"*.*"},
        0,
        true,
        true
    ));

    m_cleanerItems.append(CleanerItem(
        "Memory Dump Files",
        "System memory dump files",
        "C:/Windows",
        QStringList{"*.dmp", "*.hdmp"},
        0,
        true,
        true
    ));

    m_cleanerItems.append(CleanerItem(
        "Thumbnail Cache",
        "Windows thumbnail cache",
        getThumbnailCachePath(),
        QStringList{"*.db", "thumbcache_*.db"},
        0,
        true,
        true
    ));

    m_cleanerItems.append(CleanerItem(
        "Prefetch Files",
        "Windows prefetch files",
        "C:/Windows/Prefetch",
        QStringList{"*.pf"},
        0,
        true,
        true
    ));
}

QString WindowsUtils::getUserTempPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
}

QString WindowsUtils::getThumbnailCachePath()
{
    QString localAppData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return localAppData + "/Microsoft/Windows/Explorer";
}

QVector<CleanerItem> WindowsUtils::scanJunkFiles()
{
    QVector<CleanerItem> results;
    
    for (const CleanerItem &item : m_cleanerItems) {
        CleanerItem scannedItem = item;
        
        // Calculate directory size
        QString itemPath = scannedItem.path();
        if (!itemPath.isEmpty()) {
            QDir dir(itemPath);
            if (dir.exists()) {
                scannedItem.setSize(calculateDirectorySize(itemPath));
            } else {
                scannedItem.setSize(0);
            }
        } else {
            scannedItem.setSize(0);
        }
        
        results.append(scannedItem);
    }
    
    return results;
}

qint64 WindowsUtils::cleanJunkFiles(QVector<CleanerItem> &items)
{
    qint64 totalFreed = 0;
    
    for (const CleanerItem &item : items) {
        if (item.isSelected() && item.isSafe()) {
            QString itemPath = item.path();
            if (!itemPath.isEmpty()) {
                qint64 beforeSize = calculateDirectorySize(itemPath);
                deleteFilesByPattern(itemPath, item.filePatterns());
                qint64 afterSize = calculateDirectorySize(itemPath);
                totalFreed += (beforeSize - afterSize);
            }
        }
    }
    
    return totalFreed;
}

qint64 WindowsUtils::calculateDirectorySize(const QString &path)
{
    qint64 totalSize = 0;
    QDir dir(path);
    
    if (!dir.exists()) {
        return 0;
    }
    
    // Get all files in directory
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fileInfo : files) {
        totalSize += fileInfo.size();
    }
    
    // Recursively process subdirectories
    QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subDirInfo : subDirs) {
        totalSize += calculateDirectorySize(subDirInfo.absoluteFilePath());
    }
    
    return totalSize;
}

void WindowsUtils::deleteFilesByPattern(const QString &path, const QStringList &patterns)
{
    QDir dir(path);
    
    if (!dir.exists()) {
        return;
    }
    
    // Delete files matching patterns
    for (const QString &pattern : patterns) {
        QFileInfoList files = dir.entryInfoList(QStringList() << pattern, QDir::Files);
        for (const QFileInfo &fileInfo : files) {
            QFile file(fileInfo.absoluteFilePath());
            file.remove();
        }
    }
    
    // Recursively process subdirectories
    QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subDirInfo : subDirs) {
        deleteFilesByPattern(subDirInfo.absoluteFilePath(), patterns);
    }
}