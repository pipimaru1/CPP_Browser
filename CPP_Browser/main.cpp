// BrowserAppWebView2.cpp : Win32 + WebView2 サンプル
#include <windows.h>
#include <wrl.h>
#include <wil/com.h>
#include <wrl/wrappers/corewrappers.h>

#include "WebView2.h"          // WebView2 SDK のヘッダー
#include <WebView2EnvironmentOptions.h>  // ← これを追加

#pragma comment(lib, "ole32.lib")
//////////////////////////////////////////////////////////////////////////
// ★重要
// WevView2 Nugetパッケージをイントールすること。
// Implementation Nugetパッケージをインストールすること。
// C++17以降のバージョンを使用すること。
// プロジェクトのエントリポイントを「Windows アプリケーション」に変更すること。 [リンカー] → [システム]
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// 初期URL
#define STARTURL L"https://google.com/"

//////////////////////////////////////////////////////////////////////////
// プロキシ設定 特に指定しなくてもシステム設定を見に行く
//#define PROXURL L"--proxy-server=http://hoge:8080"
#define PROXURL L""

//////////////////////////////////////////////////////////////////////////
// インターフェース名をわかりやすく短縮するための別名定義（using）
// ccHdr は「Controller Completed Handler」、ceHdr は「Environment Completed Handler」
using namespace Microsoft::WRL;
using ccHdr = ICoreWebView2CreateCoreWebView2ControllerCompletedHandler;
using ceHdr = ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler;

//////////////////////////////////////////////////////////////////////////
// メニュー ID
#define ID_FILE_OPEN    1001
#define ID_FILE_BACK    1002
#define ID_FILE_FORWARD 1003

#define ID_FILE_CHATGPT  1010
#define ID_FILE_GOOGLE   1011
#define ID_FILE_YAHOO    1012

//////////////////////////////////////////////////////////////////////////
// ハンドラー
static ComPtr<ICoreWebView2Controller> g_webviewController = nullptr;
static ComPtr<ICoreWebView2>           g_webview = nullptr;

// Forward decl.
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

///////////////////////////////////////////////////////////////
// WebView2 環境の初期化
// コールバックで WebView2 コントローラーを生成
// コントローラーの Bounds を設定し、初期 URL にナビゲート
// 引数: hWnd - メインウィンドウのハンドル
// 戻り値: なし
// 注意: WebView2 の初期化は非同期で行われるため、
//       コールバックでコントローラーを生成する必要がある
//       コールバックの中で WebView2 コントローラーを保持する
//       変数をグローバルに持つ必要がある
//       コントローラーの Bounds を設定するため、
//       メインウィンドウのハンドルを引数に渡す
///////////////////////////////////////////////////////////////
void InitializeWebView2(HWND hWnd)
{
    // ① 環境オプションを作成し、プロキシ引数を設定
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments(PROXURL); // プロキシ設定

    // ② WebView2 環境を非同期で初期化
    CreateCoreWebView2EnvironmentWithOptions
    (
        nullptr,
        nullptr,
        options.Get(),  // ← ここに options.Get() を渡す！
        Callback<ceHdr>
        (
            // ラムダ式でコールバックを実装
            [hWnd](HRESULT envRes, ICoreWebView2Environment* env) -> HRESULT
            {
                if (FAILED(envRes))
                {
                    MessageBox(hWnd, L"WebView2 環境初期化失敗", L"Error", MB_OK);
                    return envRes;
                }
                return env->CreateCoreWebView2Controller
                (
                    hWnd,
                    Callback<ccHdr>
                    (
                        [hWnd](HRESULT ctrlRes, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            if (FAILED(ctrlRes) || !controller)
                            {
                                MessageBox(hWnd, L"WebView2 コントローラー生成失敗", L"Error", MB_OK);
                                return ctrlRes;
                            }
                            // コントローラーと WebView を保持
                            g_webviewController = controller;
                            controller->get_CoreWebView2(&g_webview);

                            // **① 初期バウンドを設定**  
                            RECT bounds;
                            GetClientRect(hWnd, &bounds);
                            g_webviewController->put_Bounds(bounds);

                            // 初期 URL をナビゲート
                            g_webview->Navigate(STARTURL);

                            return S_OK;
                        }
                    ).Get()
                );
            }
        ).Get()
    );
}


/////////////////////////////////////////////////////////////////
// メイン関数
/////////////////////////////////////////////////////////////////
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShowCmd) 
{
    // STA モードで OLE 初期化
    if (FAILED(OleInitialize(NULL))) {
        MessageBox(NULL, L"OleInitialize に失敗", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // ウィンドウクラス登録
    const wchar_t CLASS_NAME[] = L"WebView2BrowserClass";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    // メニュー作成
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_OPEN, L"開く");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(hFileMenu, MF_STRING, ID_FILE_CHATGPT, L"ChatGPT を開く");
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_GOOGLE, L"Google を開く");
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_YAHOO, L"Yahoo! Japan を開く");

    HMENU hMenu = CreateMenu();
    AppendMenuW(hMenu, MF_STRING, ID_FILE_BACK, L"戻る");
    AppendMenuW(hMenu, MF_STRING, ID_FILE_FORWARD, L"進む");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"開く");

    // メインウィンドウ生成
    HWND hWnd = CreateWindowExW(
        0, CLASS_NAME, L"Simple Browser",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        1024, 768, nullptr, hMenu, hInst, nullptr
    );
    if (!hWnd) {
        MessageBox(NULL, L"メインウィンドウ生成失敗", L"Error", MB_OK | MB_ICONERROR);
        OleUninitialize();
        return 1;
    }

    ShowWindow(hWnd, nShowCmd);
    UpdateWindow(hWnd);

    // WebView2 の非同期初期化
    InitializeWebView2(hWnd);

    // メッセージループ
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    OleUninitialize();
    return 0;
}

/////////////////////////////////////////////////////////////////
// ウィンドウプロシージャ
/////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg)
    {
        //ウィンドウサイズ変更処理
    case WM_SIZE:
        if (g_webviewController) {
            RECT rc;
            GetClientRect(hWnd, &rc);
            // コントローラーにバウンドを直接設定
            g_webviewController->put_Bounds(rc);
        }
        break;

        //メニュー処理
    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case ID_FILE_OPEN:
        {
            // ローカル HTML ファイル選択
            OPENFILENAMEW ofn{ sizeof(ofn) };
            wchar_t szFile[MAX_PATH]{};
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"HTML Files\0*.htm;*.html\0All Files\0*.*\0";
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&ofn))
            {
                // file:/// に変換して Navigate
                std::wstring uri = L"file:///" + std::wstring(szFile);
                g_webview->Navigate(uri.c_str());
            }
            break;
        }

        case ID_FILE_BACK:
        {
            BOOL canGoBack = FALSE;
            if (g_webview && SUCCEEDED(g_webview->get_CanGoBack(&canGoBack)) && canGoBack)
            {
                g_webview->GoBack();
            }
            break;
        }

        case ID_FILE_FORWARD:
        {
            BOOL canGoForward = FALSE;
            if (g_webview && SUCCEEDED(g_webview->get_CanGoForward(&canGoForward)) && canGoForward)
            {
                g_webview->GoForward();
            }
            break;
        }
        // 追加したメニュー
		case ID_FILE_CHATGPT:
			if (g_webview) {
				g_webview->Navigate(L"https://chat.openai.com/chat");
			}
			break;

        case ID_FILE_GOOGLE:
            if (g_webview) {
                g_webview->Navigate(L"https://www.google.com");
            }
            break;

        case ID_FILE_YAHOO:
            if (g_webview) {
                g_webview->Navigate(L"https://www.yahoo.co.jp");
            }
            break;

        }
        break;

        //ウィンドウが閉じられたとき
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, msg, wp, lp);
    }
    return 0;
}
