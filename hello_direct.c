/* hello_direct.c  (v5 — WFE 做法：直接调游戏 SetText，无框 + 中文)
 *
 * WarCraft III 1.27a (build 52240 = 0xCC10).
 *
 * 发送链路（照 WFE 的 alt+点击发属性逆向出来的，全部是游戏内部函数）：
 *   1. chatObj  = get_unit(0,0) -> [obj+0x3FC]      Game.dll + 0x34F3A0
 *   2. clear_chat(chatObj, 0, 0)                     Game.dll + 0x392060
 *   3. clear_chat(chatObj, 0, 1)                     Game.dll + 0x392060
 *   4. set_recipient(chatObj, 0, 1)  1=盟友           Game.dll + 0x3B2DE0
 *   5. SetText(chatObj, UTF8文本)                     Game.dll + 0x3B3B80
 *   6. wndproc(hwnd, WM_KEYDOWN, VK_RETURN, 0)        Game.dll + 0x153710
 *   7. wndproc(hwnd, WM_KEYUP,   VK_RETURN, 0)        Game.dll + 0x153710
 *
 * SetText(0x3B3B80) 内部把文本逐字节 memcpy 进输入框缓冲区，不做编码转换，
 * 所以直接传 UTF-8 中文就能正确显示（游戏聊天框内部是 UTF-8）。
 *
 * 全部游戏函数用 __fastcall（前两个参数在 ECX/EDX，其余在栈上）。
 * 定时器回调落在游戏主线程，避免后台线程调游戏函数崩溃。
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winver.h>

#define MSG_TEXT      "\u4f60\u597d"   /* "你好" —— 任意 UTF-8 文本均可 */
#define INTERVAL_MS   5000

static HMODULE g_base;
static HMODULE g_self;

static DWORD GetBuild(void)
{
    char path[MAX_PATH];
    DWORD dummy, size;
    BYTE buf[2048];
    VS_FIXEDFILEINFO *info;
    UINT infolen;

    if (!g_base)
        return 0;
    if (!GetModuleFileNameA(g_base, path, MAX_PATH))
        return 0;
    size = GetFileVersionInfoSizeA(path, &dummy);
    if (!size || size > sizeof(buf))
        return 0;
    if (!GetFileVersionInfoA(path, 0, size, buf))
        return 0;
    if (!VerQueryValueA(buf, "\\", (void**)&info, &infolen))
        return 0;
    return (info->dwFileVersionLS >> 16) & 0xFFFF;
}

/* Game.dll offsets for 1.27a (build 52240 / 0xCC10) */
#define OFF_GET_UNIT        0x34F3A0
#define OFF_CLEAR_CHAT      0x392060
#define OFF_SET_RECIPIENT   0x3B2DE0
#define OFF_SET_TEXT        0x3B3B80   /* 游戏 SetText */
#define OFF_GAME_MSG        0x153710
#define OFF_GET_GAME_WND    0x14D670

typedef void*   (__fastcall *t_get_unit)     (int, int);
typedef void    (__fastcall *t_chat_fn)      (void*, int, int);
typedef void    (__fastcall *t_set_text)     (void*, int, const char*);
typedef LRESULT (__fastcall *t_game_msg)     (HWND, UINT, WPARAM, LPARAM);
typedef HWND    (__fastcall *t_get_game_wnd) (int, int);

static t_get_unit     pGetUnit;
static t_chat_fn      pClearChat;
static t_chat_fn      pSetRecipient;
static t_set_text     pSetText;
static t_game_msg     pGameMsg;
static t_get_game_wnd pGetGameWnd;
static char g_logpath[MAX_PATH];

static void Log(const char* msg)
{
    HANDLE f = CreateFileA(g_logpath, FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    DWORD n;
    WriteFile(f, msg, lstrlenA(msg), &n, NULL);
    WriteFile(f, "\r\n", 2, &n, NULL);
    CloseHandle(f);
}

static void LogHex(const char* prefix, DWORD v)
{
    char buf[64];
    wsprintfA(buf, "%s%08X", prefix, v);
    Log(buf);
}

static BOOL Init(void)
{
    char dllpath[MAX_PATH];
    DWORD n = GetModuleFileNameA(g_self, dllpath, MAX_PATH);
    if (n > 0) {
        int i = n - 1;
        while (i >= 0 && dllpath[i] != '\\' && dllpath[i] != '/') i--;
        lstrcpynA(g_logpath, dllpath, i + 1);
        lstrcatA(g_logpath, "hello_direct.log");
    } else {
        lstrcpyA(g_logpath, "hello_direct.log");
    }

    Log("=== hello_direct v5 (WFE SetText) init ===");

    g_base = GetModuleHandleA("Game.dll");
    if (!g_base) { Log("FAIL: Game.dll not found"); return FALSE; }
    LogHex("Game.dll base = ", (DWORD)g_base);
    LogHex("Game build = ", GetBuild());

    pGetUnit      = (t_get_unit)      ((BYTE*)g_base + OFF_GET_UNIT);
    pClearChat    = (t_chat_fn)       ((BYTE*)g_base + OFF_CLEAR_CHAT);
    pSetRecipient = (t_chat_fn)       ((BYTE*)g_base + OFF_SET_RECIPIENT);
    pSetText      = (t_set_text)      ((BYTE*)g_base + OFF_SET_TEXT);
    pGameMsg      = (t_game_msg)      ((BYTE*)g_base + OFF_GAME_MSG);
    pGetGameWnd   = (t_get_game_wnd)  ((BYTE*)g_base + OFF_GET_GAME_WND);
    Log("offsets resolved");
    return TRUE;
}

/* Runs on the GAME MAIN THREAD (via WM timer). */
static void SendHelloToAllies(void)
{
    void *obj, *chatObj;
    HWND hwnd;

    if (!pGetUnit) { Log("skip: pGetUnit null"); return; }

    obj = pGetUnit(0, 0);
    LogHex("get_unit -> ", (DWORD)obj);
    if (!obj) return;

    chatObj = *(void**)((BYTE*)obj + 0x3FC);
    LogHex("chatObj -> ", (DWORD)chatObj);
    if (!chatObj) return;

    Log("clear_chat(0)");
    pClearChat(chatObj, 0, 0);
    Log("clear_chat(1)");
    pClearChat(chatObj, 0, 1);
    Log("set_recipient(1=allies)");
    pSetRecipient(chatObj, 0, 1);
    Log("SetText");
    pSetText(chatObj, 0, MSG_TEXT);

    hwnd = pGetGameWnd ? pGetGameWnd(0, 0) : NULL;
    LogHex("get_game_wnd -> ", (DWORD)hwnd);
    if (!hwnd)
        hwnd = FindWindowA("Warcraft III", NULL);
    if (!hwnd) { Log("skip: no hwnd"); return; }

    Log("send Enter");
    pGameMsg(hwnd, WM_KEYDOWN, VK_RETURN, 0);
    pGameMsg(hwnd, WM_KEYUP,   VK_RETURN, 0);
    Log("done");
}

static VOID CALLBACK TimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
    (void)hwnd; (void)msg; (void)id; (void)time;
    SendHelloToAllies();
}

static DWORD WINAPI Worker(LPVOID param)
{
    (void)param;
    Sleep(3000);
    HWND hwnd = FindWindowA("Warcraft III", NULL);
    if (!hwnd) {
        Log("FAIL: game window not found");
        return 0;
    }
    LogHex("game hwnd = ", (DWORD)hwnd);
    UINT_PTR t = SetTimer(hwnd, 0x575, INTERVAL_MS, TimerProc);
    LogHex("SetTimer -> ", (DWORD)t);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        g_self = inst;
        if (Init())
            CreateThread(NULL, 0, Worker, NULL, 0, NULL);
    }
    return TRUE;
}
