#include <Windows.h>

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
    OutputDebugStringA("WM_SIZE\n");
  }
  break;
  case WM_DESTROY:
  {
    OutputDebugStringA("WM_DESTROY\n");
  }
  break;
  case WM_CLOSE:
  {
    OutputDebugStringA("WM_CLOSE\n");
  }
  break;
  case WM_PAINT:{
    PAINTSTRUCT paintStruct;
    HDC deviceContext = BeginPaint(hwnd, &paintStruct);
    int width = paintStruct.rcPaint.right - paintStruct.rcPaint.left;
    int height = paintStruct.rcPaint.bottom - paintStruct.rcPaint.top;
    static DWORD color = WHITENESS;
    if (color == WHITENESS){
      color = BLACKNESS;
    }
    else{
      color = WHITENESS;
    }
    PatBlt(deviceContext, paintStruct.rcPaint.left, paintStruct.rcPaint.top , width, height, color);
    EndPaint(hwnd,&paintStruct);
  } break;
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
      for (;;)
      {
        MSG lpMsg;
        BOOL messageResult = GetMessage(&lpMsg, 0, 0, 0);
        if (messageResult > 0)
        {
          TranslateMessage(&lpMsg);
          DispatchMessage(&lpMsg);
        }
        else
        {
          break;
          //TODO: log error
        }
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