#include <iostream>
#include <windows.h>

struct Dimension{
    int width, height;
};

int xOffset = 0;
static bool gameRunning;

struct Bitmap{
    void* memory;
    BITMAPINFO info;
    Dimension dimensions;
};

static Dimension clientWindowDimensions;
static Bitmap globalBitmap;

void renderGradient(int xOffset){
    // pixel = 4B = 32b
    uint32_t *pixel = (uint32_t*) globalBitmap.memory;
    for (int y=0; y < globalBitmap.dimensions.height;y++){
        for (int x=0; x < globalBitmap.dimensions.width;x++){
            uint8_t red = 0x00;
            uint8_t green = 0;
            uint8_t blue = x + xOffset;
            //*pixel = 0x000000FF; //xx RR GG BB -> little endian, most significant bits get loaded last (xx)
            *pixel = (green << 8 ) | blue;
            pixel++;
        }
    }
}

void resizeDibSection(int width, int height){
    if (globalBitmap.memory){
        VirtualFree(globalBitmap.memory, 0, MEM_RELEASE);
    }

    globalBitmap.dimensions = {width, height};

    BITMAPINFOHEADER bmInfoHeader = {};
    bmInfoHeader.biSize = sizeof(bmInfoHeader);
    bmInfoHeader.biCompression = BI_RGB;
    bmInfoHeader.biWidth = width;
    bmInfoHeader.biHeight = -height; // Negative means it'll be filled top-down
    bmInfoHeader.biPlanes = 1; // MSDN sais it must be set to 1, legacy reasons
    bmInfoHeader.biBitCount = 32; // R+G+B+padding each 8bits
    globalBitmap.info.bmiHeader = bmInfoHeader;

    globalBitmap.memory = VirtualAlloc(0, width*height*4, MEM_COMMIT, PAGE_READWRITE);
}

void updateWindow(HDC deviceContext, int srcX, int srcY, int srcWidth, int srcHeight, int windowWidth, int windowHeight, void* bitMapMemory, BITMAPINFO bitmapInfo){
    StretchDIBits(deviceContext, 0, 0, windowWidth, windowHeight, 0, 0, srcWidth, srcHeight, bitMapMemory, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

Dimension getWindowDimension(HWND windowHandle){
    RECT clientRect;
    GetClientRect(windowHandle, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;
    Dimension retDimension = {width, height};
    return retDimension;
}

LRESULT CALLBACK WindowProc(HWND windowHandle, UINT uMsg, WPARAM wParam, LPARAM lParam){
    LRESULT returnVal = 0;
    switch (uMsg)
    {
        case WM_SIZE:
        {
            clientWindowDimensions = getWindowDimension(windowHandle);
        } break;
        case WM_CLOSE:
        {
            #ifdef _DEBUG
                std::cout << "DEBUG: exit popup" << std::endl;
            #endif 
            if (MessageBox(windowHandle, "Sure you want to exit?", "Jodot - Exiting", MB_YESNO) == IDYES){
                gameRunning = false;
                DestroyWindow(windowHandle);            
            };
        } break;
        case WM_DESTROY:
        {
            // ??? Handle as an error
            DestroyWindow(windowHandle);            
        } break;
        case WM_PAINT:
        {   
            PAINTSTRUCT paintStruct;
            HDC hdc = BeginPaint(windowHandle, &paintStruct);
            renderGradient(xOffset);
            updateWindow(hdc, 0, 0, globalBitmap.dimensions.width, globalBitmap.dimensions.height, clientWindowDimensions.width, clientWindowDimensions.height,  globalBitmap.memory, globalBitmap.info);
            EndPaint(windowHandle, &paintStruct);      
        } break;
        default:
        {
            returnVal = DefWindowProc(windowHandle, uMsg, wParam, lParam);
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

    globalBitmap.dimensions = {1920,1080};
    resizeDibSection(globalBitmap.dimensions.width, globalBitmap.dimensions.height);

    if (RegisterClass(&wc)){
        HWND windowHandle = CreateWindowEx(0, wc.lpszClassName, "Jodot Engine", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
        if (windowHandle){
            while(gameRunning){
                MSG message;
                if (PeekMessage(&message, 0, 0, 0, PM_REMOVE)){
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
                if (message.message == WM_QUIT){
                    gameRunning = false;
                    break;
                }
                renderGradient(xOffset);
                xOffset++;
                HDC windowDeviceContext = GetDC(windowHandle);
                updateWindow(windowDeviceContext, 0, 0, globalBitmap.dimensions.width, globalBitmap.dimensions.height, clientWindowDimensions.width, clientWindowDimensions.height,  globalBitmap.memory, globalBitmap.info);
                ReleaseDC(windowHandle, windowDeviceContext);
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