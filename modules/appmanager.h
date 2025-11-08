#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <QObject>

class MainWindow;

class AppManager : public QObject
{
    Q_OBJECT

public:
    explicit AppManager(MainWindow *mainWindow, QObject *parent = nullptr);
    
    void addApp();
    void refreshApps();
    void launchApp();
    void removeApp();
    void searchApps(const QString &searchText);

private:
    MainWindow *m_mainWindow;
};

#endif // APPMANAGER_H