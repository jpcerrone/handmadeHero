#include <Windows.h>
static bool running;
static BITMAPINFO bmpInfo;
static void *bitmapMemory;
static int bitmapWidth;
static int bitmapHeight;
static int bytesPerPixel = 4;
void renderGradient(int xOffset){
  int pitch = bytesPerPixel*bitmapWidth;

  unsigned char *row = (unsigned char *) bitmapMemory;

  for(int y = 0; y < bitmapHeight; y++){
    unsigned long *pixel = (unsigned long *) row;
    for(int x = 0; x < bitmapWidth; x++){
      *pixel = (0xAA << 8) | ((x+xOffset)*255/bitmapWidth);
      pixel++;
    }
    row += pitch;
  }
}



void resizeDIBSecion(int width, int height)
{
  if(bitmapMemory){
    VirtualFree(bitmapMemory, 0, MEM_RELEASE);
  }

  bitmapWidth = width;
  bitmapHeight = height;

  bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
  bmpInfo.bmiHeader.biWidth = bitmapWidth;
  bmpInfo.bmiHeader.biHeight = -bitmapHeight;
  bmpInfo.bmiHeader.biPlanes = 1;
  bmpInfo.bmiHeader.biBitCount = 32;
  bmpInfo.bmiHeader.biCompression = BI_RGB;

  int bitmapMemorySize = bytesPerPixel*bitmapWidth*bitmapHeight;
  bitmapMemory = VirtualAlloc(0,bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

void updateWindow(RECT *WindowRect, HDC deviceContext)
{
  int windowWidth = WindowRect->right - WindowRect->left;
  int windowHeight = WindowRect->bottom - WindowRect->top;
  StretchDIBits(deviceContext,
                0, 0, bitmapWidth, bitmapHeight,
                0, 0, windowWidth, windowHeight,
                bitmapMemory,
                &bmpInfo,
                DIB_RGB_COLORS,
                SRCCOPY);
}

LRESULT CALLBACK MainWindowCallback(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
  LRESULT result = 0;
  switch (uMsg)
  {
  case WM_ACTIVATEAPP:
  {
    OutputDebugStringA("WM_ACTIVATEAPP\n");
  }
  break;
  case WM_SIZE:
  {
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;
    resizeDIBSecion(width, height);
    OutputDebugStringA("WM_SIZE\n");
  }
  break;
  case WM_DESTROY:
  {
    running = false;
    OutputDebugStringA("WM_DESTROY\n");
  }
  break;
  case WM_CLOSE:
  {
    running = false;
    OutputDebugStringA("WM_CLOSE\n");
  }
  break;
  case WM_PAINT:
  {
    PAINTSTRUCT paintStruct;
    HDC deviceContext = BeginPaint(hwnd, &paintStruct);
    int x = paintStruct.rcPaint.left;
    int y = paintStruct.rcPaint.top;
    int width = paintStruct.rcPaint.right - paintStruct.rcPaint.left;
    int height = paintStruct.rcPaint.bottom - paintStruct.rcPaint.top;
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    renderGradient(100);
    updateWindow(&clientRect, deviceContext);
    EndPaint(hwnd, &paintStruct);
  }
  break;
  default:
  {
    result = DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  break;
  }
  return (result);
}

int CALLBACK WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nShowCmd)
{
  WNDCLASSA windowClass = {};
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = MainWindowCallback;
  windowClass.hInstance = hInstance;
  windowClass.lpszClassName = "HandmadeHeroWindowClass";
  if (RegisterClassA(&windowClass))
  {
    HWND windowHandle = CreateWindowExA(0, windowClass.lpszClassName,
                                        "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                        0, 0, hInstance, 0);
    if (windowHandle)
    {
      running = true;
      int xOffset = 0;
      while (running)
      {
        MSG lpMsg;
        while (PeekMessageA(&lpMsg, 0, 0, 0, PM_REMOVE))
        {
          TranslateMessage(&lpMsg);
          DispatchMessage(&lpMsg);
        }
        
        renderGradient(xOffset++);
        HDC deviceContext = GetDC(windowHandle);
        RECT clientRect;
        GetClientRect(windowHandle, &clientRect);
        updateWindow(&clientRect, deviceContext);
        ReleaseDC(windowHandle, deviceContext);
      }
    }
    else
    {
      //TODO logging
    }
  }
  else
  {
    //TODO log results
  }
  return (0);
}