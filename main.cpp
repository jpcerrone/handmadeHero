#include <iostream>
#include <windows.h>

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
    LRESULT returnVal = 0;
    switch (uMsg)
    {
        case WM_CLOSE:
        {
            std::cout << "WM_CLOSE";
            
        } break;
        case WM_PAINT:
        {
            static DWORD color = BLACKNESS;
            HDC hdc; 
            PAINTSTRUCT lpPaint;
            hdc = BeginPaint(hWnd, &lpPaint);
            color = (color == BLACKNESS) ? WHITENESS : BLACKNESS;
            PatBlt(hdc, lpPaint.rcPaint.left, lpPaint.rcPaint.top, lpPaint.rcPaint.right - lpPaint.rcPaint.left, lpPaint.rcPaint.bottom - lpPaint.rcPaint.top, color);
            EndPaint(hWnd, &lpPaint);
        } break;
        default:
        {
            returnVal = DefWindowProc(hWnd, uMsg, wParam, lParam);
        } break;
    }
    return returnVal;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd){
    WNDCLASS wc = {};
    
    //wc.cbSize = sizeof(WNDCLASSEXW);

    wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance ? hInstance : GetModuleHandle(nullptr);
	wc.lpszClassName = "Engine";

    if (RegisterClass(&wc)){
        HWND windowHandle = CreateWindowEx(0, wc.lpszClassName, "Jodot Engine", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
        if (windowHandle){
            while(true){
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