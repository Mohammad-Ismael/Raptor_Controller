#ifndef FILESCHECKER_H
#define FILESCHECKER_H

#include <QObject>
#include <QFileInfo>
#include <QDir>
#include <QFutureWatcher>
#include <QVector>
#include <QAtomicInteger>

class MainWindow;

struct FileInfo {
    QString path;
    qint64 size;
    QString sizeFormatted;
    QDateTime lastModified;
    bool isSelected;
};

struct DuplicateFile {
    QString hash;
    QVector<FileInfo> files;
    qint64 totalSize;
};

class FilesChecker : public QObject
{
    Q_OBJECT

public:
    explicit FilesChecker(MainWindow *mainWindow, QObject *parent = nullptr);
    ~FilesChecker();
    
    void scanLargeFiles(const QString &path, double minSizeGB);
    void cancelLargeFilesScan();
    void scanDuplicateFiles(const QString &path);
    void cancelDuplicateFilesScan();
    void deleteSelectedFiles(const QVector<FileInfo> &files);
    void openFileLocation(const QString &filePath);
    void refreshDiskSpace();
    QStringList getCommonPaths();
     void openFileDirectory(const QString &filePath);  // ADD THIS LINE

private slots:
    void onLargeFilesScanFinished();
    void onDuplicateFilesScanFinished();

private:
    MainWindow *m_mainWindow;
    QFutureWatcher<QVector<FileInfo>> *m_largeFilesWatcher;
    QFutureWatcher<QVector<DuplicateFile>> *m_duplicateFilesWatcher;
    QAtomicInteger<bool> m_cancelLargeFilesScan;
    QAtomicInteger<bool> m_cancelDuplicateFilesScan;

    static QVector<FileInfo> performLargeFilesScan(const QString &path, qint64 minSizeBytes, QAtomicInteger<bool> &cancelFlag);
    static QVector<DuplicateFile> performDuplicateFilesScan(const QString &path, QAtomicInteger<bool> &cancelFlag);
    static QString calculateFileHash(const QString &filePath, QAtomicInteger<bool> &cancelFlag);
    static QString formatFileSize(qint64 size);
};

#endif // FILESCHECKER_H