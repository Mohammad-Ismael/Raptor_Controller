#ifndef WINDOWSUTILS_H
#define WINDOWSUTILS_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QStringList>
#include <QDir>
#include "cleaneritem.h" // Include the common header

class WindowsUtils : public QObject
{
    Q_OBJECT

public:
    explicit WindowsUtils(QObject *parent = nullptr);

    // System Cleaner Methods
    QVector<CleanerItem> scanJunkFiles();
    qint64 cleanJunkFiles(const QVector<CleanerItem> &itemsToClean);
    qint64 calculateFolderSize(const QString &path);
    QVector<CleanerItem> getCleanerItemsTemplate() const;
    qint64 calculateFolderSizeQuick(const QString &path);

private:
    void deleteFilesByPattern(const QString &path, const QStringList &patterns);
    qint64 calculateDirectorySize(const QDir &dir);
    void initializeCleanerItems();

    // Safe locations for Windows 10/11 cleaning
    QString getWindowsTempPath() const;
    QString getUserTempPath() const;
    QString getPrefetchPath() const;
    QString getThumbnailCachePath() const;
    QString getWindowsLogsPath() const;
    QString getMemoryDumpPath() const;

    QVector<CleanerItem> m_cleanerItems;
    void deleteAllFilesRecursive(const QDir &dir);
};

#endif // WINDOWSUTILS_H
