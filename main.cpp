#include "mainwindow.h"

#include <QApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>

// Request admin privileges on Windows
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Check if we're already running as admin, if not, restart as admin
    BOOL isAdmin = FALSE;
    HANDLE hToken = NULL;
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD dwSize;
        
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
            isAdmin = elevation.TokenIsElevated;
        }
        
        CloseHandle(hToken);
    }
    
    if (!isAdmin) {
        // Relaunch as admin
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath))) {
            SHELLEXECUTEINFO sei = { sizeof(sei) };
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;
            
            if (ShellExecuteEx(&sei)) {
                return 0; // Exit current instance
            }
        }
    }
#endif

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
