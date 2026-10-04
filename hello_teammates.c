/* hello_teammates.c  (v2 — clipboard paste, same routine as WFE quick chat)
 *
 * Every 5 seconds, send "hello" to the allies (teammates) chat.
 *
 * Mechanism (the proven WFE/UDamageWatcher routine):
 *    1. put the text on the clipboard (CF_UNICODETEXT)
 *    2. Shift+Enter   -> opens the "To Allies" chat box
 *    3. Ctrl+V        -> paste
 *    4. Enter         -> send
 *
 * This still uses the game's chat box (required so teammates actually
 * receive the message over the network), but it is much faster and more
 * reliable than typing char-by-char, and works for non-ASCII too.
 *
 * Note: it overwrites the user's clipboard on every send.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define MSG_TEXT     L"hello"
#define MSG_LEN      5
#define INTERVAL_MS  5000

static void SendKey(WORD vk, DWORD flags)
{
    INPUT in;
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = 0;
    in.ki.dwFlags = flags;
    in.ki.time = 0;
    in.ki.dwExtraInfo = 0;
    SendInput(1, &in, sizeof(in));
}

static void SetClipboardText(const WCHAR *text, int len)
{
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(WCHAR));
    if (!h)
        return;
    WCHAR *p = (WCHAR *)GlobalLock(h);
    if (p) {
        int i;
        for (i = 0; i < len; ++i)
            p[i] = text[i];
        p[len] = 0;
        GlobalUnlock(h);
    }
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        SetClipboardData(CF_UNICODETEXT, h);
        CloseClipboard();
    } else {
        GlobalFree(h);
    }
}

static HWND FindGameWindow(void)
{
    HWND h = FindWindowA("Warcraft III", NULL);
    if (h)
        return h;
    h = FindWindowA(NULL, "Warcraft III");
    return h;
}

static void FocusGame(void)
{
    HWND h = FindGameWindow();
    if (!h)
        return;
    if (IsIconic(h))
        ShowWindow(h, SW_RESTORE);
    SetForegroundWindow(h);
    Sleep(120);
}

static void SendHelloToAllies(void)
{
    FocusGame();
    SetClipboardText(MSG_TEXT, MSG_LEN);

    /* Shift+Enter opens the "To Allies" chat box. */
    SendKey(VK_SHIFT, 0);
    Sleep(30);
    SendKey(VK_RETURN, 0);
    SendKey(VK_RETURN, KEYEVENTF_KEYUP);
    SendKey(VK_SHIFT, KEYEVENTF_KEYUP);
    Sleep(200);                 /* let the chat box open */

    /* Ctrl+V pastes the message. */
    SendKey(VK_CONTROL, 0);
    SendKey('V', 0);
    SendKey('V', KEYEVENTF_KEYUP);
    SendKey(VK_CONTROL, KEYEVENTF_KEYUP);
    Sleep(80);

    /* Enter sends it. */
    SendKey(VK_RETURN, 0);
    SendKey(VK_RETURN, KEYEVENTF_KEYUP);
}

static DWORD WINAPI Worker(LPVOID param)
{
    (void)param;
    Sleep(3000);                /* brief startup delay after injection */
    for (;;) {
        SendHelloToAllies();
        Sleep(INTERVAL_MS);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        CreateThread(NULL, 0, Worker, NULL, 0, NULL);
    }
    return TRUE;
}
