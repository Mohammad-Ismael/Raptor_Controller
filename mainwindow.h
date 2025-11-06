#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Main navigation slots
    void on_generalButton_clicked();
    void on_wifiButton_clicked();
    void on_appsButton_clicked();
    void on_cleanerButton_clicked();
    void on_networkButton_clicked();
    void on_hardwareButton_clicked();
    void on_optionsButton_clicked();

    // Cleaner tab slots
    void on_scanQuickButton_clicked();
    void on_cleanQuickButton_clicked();
    void on_scanSystemButton_clicked();
    void on_cleanSystemButton_clicked();
    void on_selectAllSystemButton_clicked();
    void on_scanBrowsersButton_clicked();
    void on_cleanBrowsersButton_clicked();

    // Software Uninstaller slots
    void on_refreshSoftwareButton_clicked();
    void on_searchSoftwareInput_textChanged(const QString &text);
    void on_softwareTable_itemSelectionChanged();
    void on_uninstallSoftwareButton_clicked();
    void on_forceUninstallButton_clicked();

private:
    Ui::MainWindow *ui;
    void setupConnections();
    void updateContent(const QString &title);
    void setActiveButton(QPushButton *activeButton);
    void resetAllButtons();
    void showCleanerPage();
    void populateSoftwareTable();
    // General tab slots
    void on_refreshSystemInfoButton_clicked();
    void on_generateReportButton_clicked();
    void on_disableStartupButton_clicked();
    void on_enableStartupButton_clicked();
    void on_startupTable_itemSelectionChanged();
    void on_saveSettingsButton_clicked();
    void on_resetSettingsButton_clicked();
};
#endif // MAINWINDOW_H
