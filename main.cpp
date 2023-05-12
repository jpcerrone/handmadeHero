#include <iostream>
#include <windows.h>

struct Dimension{
    int width, height;
};

static bool gameRunning;
static Dimension windowSize;
void* bitsMapMemory;
BITMAPINFO bmInfo;

void resizeDibSection(int width, int height){
    if (bitsMapMemory){
        VirtualFree(bitsMapMemory, 0, MEM_RELEASE);
    }

    BITMAPINFOHEADER bmInfoHeader = {};
    bmInfoHeader.biSize = sizeof(bmInfoHeader);
    bmInfoHeader.biCompression = BI_RGB;
    bmInfoHeader.biWidth = width;
    bmInfoHeader.biHeight = -height;
    bmInfoHeader.biPlanes = 1; // MSDN sais it must be set to 1, legacy reasons
    bmInfoHeader.biBitCount = 32; // R+G+B+padding each 8bits
    bmInfo.bmiHeader = bmInfoHeader;

    bitsMapMemory = VirtualAlloc(0, width*height*4, MEM_COMMIT, PAGE_READWRITE);

    // pixel = 4B = 32b
    unsigned int *pixel = (unsigned int*) bitsMapMemory;
    for (int y=0; y < height;y++){
        for (int x=0; x < width;x++){
            *pixel = 0x00FF0000;
            pixel++;
        }
    }
}

void updateWindow(HDC deviceContext, int srcX, int srcY, int srcWidth, int srcHeight, int windowWidth, int windowHeight, void* bitMapMemory, BITMAPINFO bitmapInfo){
    StretchDIBits(deviceContext, 0, 0, srcWidth, srcHeight, 0, 0, windowWidth, windowHeight, bitMapMemory, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
    LRESULT returnVal = 0;
    switch (uMsg)
    {
        case WM_SIZE:
        {
            RECT windowRect;
            GetClientRect(hWnd, &windowRect);
            int width = windowRect.right - windowRect.left;
            int height = windowRect.bottom - windowRect.top;
            windowSize = {width, height};
            resizeDibSection(width, height);

        } break;
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
            PAINTSTRUCT paintStruct;
            HDC hdc = BeginPaint(hWnd, &paintStruct);
            int paintStructWidth = paintStruct.rcPaint.right - paintStruct.rcPaint.left;
            int paintStructHeight = paintStruct.rcPaint.bottom - paintStruct.rcPaint.top;
            int x = paintStruct.rcPaint.left;
            int y = paintStruct.rcPaint.top;
            updateWindow(hdc, x, y, paintStructWidth, paintStructHeight, windowSize.width, windowSize.height,  bitsMapMemory, bmInfo);
            EndPaint(hWnd, &paintStruct);
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