// KvinsInjector.cpp - VERSIONE FINALE - Nessuna striscia bianca
// Target: javaw.exe (Zulu x64)

#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <shlobj.h>
#include <gdiplus.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// Variabili globali
HWND hMainWnd;
HWND hInjectButton;
HWND hJavawStatus;
HWND hDllNameStatus;
HWND hLoadButton;
HWND hCloseButton;
DWORD g_targetProcessId = 0;
bool g_javawReady = false;
bool g_dllValid = false;
bool g_dllLoaded = false;
std::wstring g_dllPath;
std::wstring g_dllName;
HINSTANCE g_hInst;

// Variabili per il logo
GdiplusStartupInput g_gdiplusStartupInput;
ULONG_PTR g_gdiplusToken;
Image* g_pLogo = NULL;
int g_logoWidth = 0;
int g_logoHeight = 0;
const int LOGO_MAX_WIDTH = 200;
const int LOGO_MAX_HEIGHT = 80;

// Font
HFONT hNormalFont;

// Funzioni
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void CheckJavawProcess();
bool InjectDLL(DWORD processId, const std::wstring& dllPath);
void BrowseForDll();
void UpdateStatus();
void CenterWindow(HWND hwnd);
bool IsDLLFile(const std::wstring& path);
void LoadLogo();

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;

    // Inizializza GDI+
    GdiplusStartup(&g_gdiplusToken, &g_gdiplusStartupInput, NULL);

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_HAND);
    wc.hbrBackground = CreateSolidBrush(RGB(18, 18, 22));  // Sfondo nero
    wc.lpszClassName = L"KvinsInjectorClass";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassEx(&wc);

    // Finestra senza barra del titolo
    hMainWnd = CreateWindowEx(
        0, L"KvinsInjectorClass", L"",
        WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        NULL, NULL, hInstance, NULL
    );

    // Carica il logo
    LoadLogo();

    CenterWindow(hMainWnd);
    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);
    SetTimer(hMainWnd, 1, 2000, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(g_gdiplusToken);
    return 0;
}

void LoadLogo() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring path(exePath);
    size_t pos = path.find_last_of(L"\\");
    std::wstring logoPath = path.substr(0, pos + 1) + L"logo.png";

    g_pLogo = Image::FromFile(logoPath.c_str(), FALSE);

    if (g_pLogo && g_pLogo->GetLastStatus() == Ok) {
        int originalWidth = g_pLogo->GetWidth();
        int originalHeight = g_pLogo->GetHeight();

        float ratio = min((float)LOGO_MAX_WIDTH / originalWidth,
            (float)LOGO_MAX_HEIGHT / originalHeight);

        if (ratio < 1.0f) {
            g_logoWidth = (int)(originalWidth * ratio);
            g_logoHeight = (int)(originalHeight * ratio);
        }
        else {
            g_logoWidth = originalWidth;
            g_logoHeight = originalHeight;
        }
    }
}

void CenterWindow(HWND hwnd) {
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

bool IsDLLFile(const std::wstring& path) {
    size_t dotPos = path.find_last_of(L".");
    if (dotPos == std::wstring::npos) return false;
    std::wstring ext = path.substr(dotPos + 1);
    return (ext == L"dll" || ext == L"DLL");
}

void BrowseForDll() {
    OPENFILENAMEW ofn = { 0 };
    wchar_t fileName[MAX_PATH] = { 0 };

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hMainWnd;
    ofn.lpstrFilter = L"DLL Files\0*.dll\0All Files\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = L"Seleziona DLL da injectare";

    if (GetOpenFileNameW(&ofn)) {
        g_dllPath = std::wstring(fileName);

        // Estrai solo il nome del file
        std::wstring filenameOnly = fileName;
        size_t pos = filenameOnly.find_last_of(L"\\");
        if (pos != std::wstring::npos) {
            g_dllName = filenameOnly.substr(pos + 1);
        }
        else {
            g_dllName = filenameOnly;
        }

        g_dllLoaded = true;
        g_dllValid = IsDLLFile(g_dllPath);
        UpdateStatus();
    }
}

void CheckJavawProcess() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    g_targetProcessId = 0;
    g_javawReady = false;

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, L"javaw.exe") == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    wchar_t processPath[MAX_PATH] = { 0 };
                    DWORD pathSize = MAX_PATH;
                    if (QueryFullProcessImageNameW(hProcess, 0, processPath, &pathSize)) {
                        std::wstring pathStr(processPath);
                        if (pathStr.find(L"zulu") != std::wstring::npos ||
                            pathStr.find(L"Zulu") != std::wstring::npos) {
                            g_targetProcessId = pe32.th32ProcessID;
                            g_javawReady = true;
                        }
                    }
                    CloseHandle(hProcess);
                }
                break;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);

    UpdateStatus();
}

void UpdateStatus() {
    InvalidateRect(hJavawStatus, NULL, TRUE);
    InvalidateRect(hDllNameStatus, NULL, TRUE);
    InvalidateRect(hInjectButton, NULL, TRUE);
    InvalidateRect(hLoadButton, NULL, TRUE);
}

bool InjectDLL(DWORD processId, const std::wstring& dllPath) {
    // Apri il processo con TUTTI i permessi necessari
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD |
        PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE |
        PROCESS_VM_READ,
        FALSE,
        processId);

    if (!hProcess) {
        return false;
    }

    // Verifica l'architettura
    BOOL isWow64 = FALSE;
    IsWow64Process(hProcess, &isWow64);

#ifdef _WIN64
    if (isWow64) {
        CloseHandle(hProcess);
        return false;
    }
#else
    if (!isWow64) {
        CloseHandle(hProcess);
        return false;
    }
#endif

    // Alloca memoria nel processo target
    size_t pathSize = (dllPath.length() + 1) * sizeof(wchar_t);
    LPVOID pRemoteMemory = VirtualAllocEx(hProcess, NULL, pathSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMemory) {
        CloseHandle(hProcess);
        return false;
    }

    // Scrivi il percorso della DLL nella memoria allocata
    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(hProcess, pRemoteMemory, dllPath.c_str(), pathSize, &bytesWritten)) {
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Ottieni l'indirizzo di LoadLibraryW
    LPVOID pLoadLibraryW = (LPVOID)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!pLoadLibraryW) {
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Crea un thread remoto che chiama LoadLibraryW
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)pLoadLibraryW,
        pRemoteMemory, 0, NULL);

    if (!hThread) {
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Aspetta che il thread completi
    WaitForSingleObject(hThread, 5000);

    // Pulizia
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return true;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND: {
        // Previene lo sfondo bianco durante il ridisegno
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Sfondo nero
        RECT rc;
        GetClientRect(hWnd, &rc);
        HBRUSH hBrush = CreateSolidBrush(RGB(18, 18, 22));
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        // Disegna il logo centrato
        if (g_pLogo) {
            Graphics graphics(hdc);
            int x = (400 - g_logoWidth) / 2;
            int y = 20;
            graphics.DrawImage(g_pLogo, x, y, g_logoWidth, g_logoHeight);
        }

        EndPaint(hWnd, &ps);
        break;
    }

    case WM_CREATE: {
        hNormalFont = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        // Status javaw.exe
        hJavawStatus = CreateWindowW(L"STATIC", L"javaw.exe: in attesa",
            WS_VISIBLE | WS_CHILD | SS_LEFT, 30, 110, 300, 25, hWnd, NULL, g_hInst, NULL);

        // Testo "select a dll" in bianco (appare sempre)
        hDllNameStatus = CreateWindowW(L"STATIC", L"select a dll",
            WS_VISIBLE | WS_CHILD | SS_LEFT, 30, 140, 250, 25, hWnd, NULL, g_hInst, NULL);

        // Bottone LOAD
        hLoadButton = CreateWindowW(L"BUTTON", L"Select Dll",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT,
            280, 137, 90, 28, hWnd, (HMENU)1, g_hInst, NULL);

        // Bottone INJECT
        hInjectButton = CreateWindowW(L"BUTTON", L"INJECT",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT,
            125, 190, 150, 40, hWnd, (HMENU)2, g_hInst, NULL);

        // Bottone X per chiudere
        hCloseButton = CreateWindowW(L"BUTTON", L"X",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT,
            365, 5, 25, 25, hWnd, (HMENU)3, g_hInst, NULL);

        // Applica font
        SendMessage(hJavawStatus, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
        SendMessage(hDllNameStatus, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
        SendMessage(hLoadButton, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
        SendMessage(hInjectButton, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
        SendMessage(hCloseButton, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

        CheckJavawProcess();
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hwnd = (HWND)lParam;

        SetBkColor(hdc, RGB(18, 18, 22));
        SetBkMode(hdc, TRANSPARENT);

        if (hwnd == hJavawStatus) {
            if (g_javawReady) {
                SetTextColor(hdc, RGB(0, 255, 0));
                SetWindowTextW(hJavawStatus, L"javaw.exe: ready");
            }
            else {
                SetTextColor(hdc, RGB(255, 0, 0));
                SetWindowTextW(hJavawStatus, L"javaw.exe: waiting");
            }
        }
        else if (hwnd == hDllNameStatus) {
            if (g_dllLoaded) {
                SetWindowTextW(hDllNameStatus, g_dllName.c_str());
                SetTextColor(hdc, g_dllValid ? RGB(0, 255, 0) : RGB(255, 0, 0));
            }
            else {
                SetWindowTextW(hDllNameStatus, L"select a dll");
                SetTextColor(hdc, RGB(255, 255, 255));
            }
        }

        // Restituisce un brush nero per lo sfondo
        return (LRESULT)CreateSolidBrush(RGB(18, 18, 22));
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        HWND hwnd = (HWND)lParam;

        SetBkColor(hdc, RGB(18, 18, 22));
        SetBkMode(hdc, TRANSPARENT);

        if (hwnd == hCloseButton) {
            SetTextColor(hdc, RGB(140, 140, 150));
        }
        else if (hwnd == hLoadButton) {
            SetTextColor(hdc, RGB(200, 200, 220));
        }
        else if (hwnd == hInjectButton) {
            bool enabled = (g_javawReady && g_dllLoaded && g_dllValid);
            SetTextColor(hdc, enabled ? RGB(0, 255, 0) : RGB(255, 0, 0));
        }

        // Restituisce un brush nero per lo sfondo
        return (LRESULT)CreateSolidBrush(RGB(18, 18, 22));
    }

    case WM_NCHITTEST: {
        LRESULT result = DefWindowProc(hWnd, msg, wParam, lParam);
        if (result == HTCLIENT) {
            return HTCAPTION;
        }
        return result;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case 1: // LOAD
            BrowseForDll();
            break;

        case 2: // INJECT
            if (!g_javawReady) {
                MessageBoxW(hWnd, L"javaw.exe (Zulu x64) non trovato!", L"Kvins", MB_ICONWARNING);
                break;
            }
            if (!g_dllLoaded) {
                MessageBoxW(hWnd, L"Nessuna DLL selezionata!", L"Kvins", MB_ICONWARNING);
                break;
            }
            if (!g_dllValid) {
                MessageBoxW(hWnd, L"Il file selezionato non è una DLL valida!", L"Kvins", MB_ICONWARNING);
                break;
            }

            EnableWindow(hInjectButton, FALSE);
            EnableWindow(hLoadButton, FALSE);
            KillTimer(hWnd, 1);

            if (InjectDLL(g_targetProcessId, g_dllPath)) {
                MessageBoxW(hWnd, L"DLL injectata con successo!", L"Kvins", MB_OK);
                PostQuitMessage(0);
            }
            else {
                MessageBoxW(hWnd, L"Iniezione fallita! Verifica i permessi.", L"Kvins", MB_ICONERROR);
                EnableWindow(hInjectButton, TRUE);
                EnableWindow(hLoadButton, TRUE);
                SetTimer(hWnd, 1, 2000, NULL);
            }
            break;

        case 3: // X
            PostQuitMessage(0);
            break;
        }
        break;
    }

    case WM_TIMER:
        CheckJavawProcess();
        break;

    case WM_DESTROY:
        KillTimer(hWnd, 1);
        DeleteObject(hNormalFont);
        if (g_pLogo) delete g_pLogo;
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}