#include <iostream>
#include <windows.h>

bool gameRunning;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
    PAINTSTRUCT lpPaint;
    LRESULT returnVal = 0;
    int width = lpPaint.rcPaint.right - lpPaint.rcPaint.left;
    int height = lpPaint.rcPaint.bottom - lpPaint.rcPaint.top;
    switch (uMsg)
    {
        case WM_CLOSE:
        {
            if (MessageBox(hWnd, "Sure you want to exit?", "Jodot - Exiting", MB_YESNO) == IDYES){
                gameRunning = false;
                DestroyWindow(hWnd);            
            };
        } break;
        case WM_DESTROY:
        {
            // ??? Handle as an error
            DestroyWindow(hWnd);            
        } break;
        case WM_PAINT:
        {
            static DWORD color = BLACKNESS;
            HDC hdc; 
            hdc = BeginPaint(hWnd, &lpPaint);
            color = (color == BLACKNESS) ? WHITENESS : BLACKNESS;
            PatBlt(hdc, lpPaint.rcPaint.left, lpPaint.rcPaint.top, width, height, color);
            EndPaint(hWnd, &lpPaint);
        } break;
        default:
        {
            returnVal = DefWindowProc(hWnd, uMsg, wParam, lParam);
        } break;
    }
    return returnVal;
};

// hInstance: handle to the .exe
// hPrevInstance: not used since 16bit windows
// WINAPI: calling convention, tells compiler order of parameters
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd){
    WNDCLASS wc = {};
    gameRunning = true;

    wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance ? hInstance : GetModuleHandle(nullptr);
	wc.lpszClassName = "Engine";

    if (RegisterClass(&wc)){
        HWND windowHandle = CreateWindowEx(0, wc.lpszClassName, "Jodot Engine", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
        if (windowHandle){
            while(gameRunning){
                MSG message;
                BOOL messageResult = GetMessage(&message, 0, 0, 0);
                if (messageResult > 0){
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
            }
        }
        else{
            std::cout << "Failure getting window handle";
        }
    
    }else{
        std::cout << "Failure registering class";
    }
    return 0;
};