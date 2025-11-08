#ifndef WINDOWSUTILS_H
#define WINDOWSUTILS_H

#include <QObject>
#include <QList>
#include <QVector>
#include "cleaneritem.h"

class WindowsUtils : public QObject
{
    Q_OBJECT

public:
    explicit WindowsUtils(QObject *parent = nullptr);

    QVector<CleanerItem> scanJunkFiles();
    qint64 cleanJunkFiles(QVector<CleanerItem> &items);

private:
    QList<CleanerItem> m_cleanerItems;

    void initializeCleanerItems();
    QString getUserTempPath();
    QString getThumbnailCachePath();
    qint64 calculateDirectorySize(const QString &path);
    void deleteFilesByPattern(const QString &path, const QStringList &patterns);
};

#endif // WINDOWSUTILS_H