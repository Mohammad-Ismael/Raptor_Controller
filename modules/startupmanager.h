#ifndef STARTUPMANAGER_H
#define STARTUPMANAGER_H

#include <QObject>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QList>

class MainWindow;

struct StartupProgram {
    QString name;
    QString status; // "Enabled" or "Disabled"
    QString impact; // "High", "Medium", "Low"
    QString startupType; // "Registry", "Startup Folder", "Service", "Scheduled Task"
    QString command;
    QString location;
    bool isEnabled;
};

class StartupManager : public QObject
{
    Q_OBJECT

public:
    explicit StartupManager(MainWindow *mainWindow, QObject *parent = nullptr);
    ~StartupManager();

    void initialize();
    void refreshStartupPrograms();
    void disableSelectedProgram();
    void enableSelectedProgram();
    void onStartupTableSelectionChanged();

private slots:
    void onDisableButtonClicked();
    void onEnableButtonClicked();

private:
    MainWindow *m_mainWindow;
    QList<StartupProgram> m_startupPrograms;

    void setupConnections();
    void populateTable();
    void updateButtonStates();
    void updateImpactLabel();
    QList<StartupProgram> getRealStartupPrograms();
    bool changeStartupProgramState(const QString &programName, bool enable);
    double calculateBootImpact();

    void scanRegistryStartup(QList<StartupProgram> &programs, const QString &registryPath, 
                           const QString &startupType, const QString &location);
    void scanStartupFolders(QList<StartupProgram> &programs);
    void scanServices(QList<StartupProgram> &programs);
    void scanScheduledTasks(QList<StartupProgram> &programs);
    void scanBootManager(QList<StartupProgram> &programs);
    QString calculateProgramImpact(const QString &programName, const QString &command);
    void removeDuplicates(QList<StartupProgram> &programs);
    void sortProgramsByName(QList<StartupProgram> &programs);


};

#endif // STARTUPMANAGER_H
