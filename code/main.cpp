#include <Windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <stdio.h>
#include <xaudio2.h>
#define _USE_MATH_DEFINES
#include <math.h>

static bool running;
static int xOffset = 0;
static int cycles = 512;
static IXAudio2SourceVoice *pSourceVoice;

struct audioBuffer {
  int wBitsPerSample = 16;
  int sampleRate = 44100;
  int seconds = 1;
  int16_t volume = 1000;
  int latencySamples = sampleRate / 30;
  int bufferElements = (wBitsPerSample * sampleRate * seconds * 2) / 8;
  BYTE *pDataBuffer = new BYTE[(wBitsPerSample * sampleRate * seconds * 2) / 8];
};

struct backBuffer
{
  BITMAPINFO info;
  void *memory;
  int width;
  int height;
  int bytesPerPixel = 4;
  int pitch;
};

struct Dimension
{
  int width;
  int height;
};

static backBuffer globalBackBuffer;
static audioBuffer globalAudioBuffer;
Dimension getWindowDimension(HWND window)
{
  RECT rect;
  GetWindowRect(window, &rect);
  Dimension d;
  d.width = rect.right - rect.left;
  d.height = rect.bottom - rect.top;
  return d;
}

void fillBufferWithSquareWave(BYTE *pDataBuffer, int sampleRate, int bufferElements)
{
  int16_t volume = 1000;
  int16_t *sample = (int16_t *)pDataBuffer;
  int frecuencyHz = 200;
  int cycles = (sampleRate / frecuencyHz);
  int counter = 0;
  for (int i = 0; i < bufferElements / 8; i++)
  {
    // one sample at a time
    if (counter < cycles)
    {
      *(sample) = volume;
      *(sample + 1) = volume;
      *(sample + 2) = volume;
      *(sample + 3) = volume;
    }
    else
    {
      *(sample) = -volume;
      *(sample + 1) = -volume;
      *(sample + 2) = -volume;
      *(sample + 3) = -volume;
    }
    counter++;
    if (counter == cycles * 2)
    {
      counter = 0;
    }
    else
      sample = sample + 4;
  }
}

void fillBufferWithSineWave(audioBuffer buffer)
{
  XAUDIO2_VOICE_STATE bufferState;
  int samplesPlayed;
  if(pSourceVoice){
    pSourceVoice->GetState(&bufferState);
    samplesPlayed = bufferState.SamplesPlayed % buffer.sampleRate;
  }
  else{
    samplesPlayed = 0;
  }
  /*
  int16_t *sample = (int16_t *)buffer.pDataBuffer;
  for (int i = 0; i < buffer.sampleRate; i++)
  {
    // one sample at a time
    float value = i*cycles*2*M_PI/buffer.sampleRate;
    *(sample) = buffer.volume * sin(value);
    *(sample + 1) = buffer.volume * sin(value);
    sample = sample + 2;
  }*/
  int16_t *sample = (int16_t *)buffer.pDataBuffer;
  sample += samplesPlayed*2;
  for(int i = 0; i < buffer.sampleRate - samplesPlayed;i++){
    // one sample at a time
    float value = i*cycles*2*M_PI/buffer.sampleRate;
    *(sample) = buffer.volume * sin(value);
    *(sample + 1) = buffer.volume * sin(value);
    sample = sample + 2;
  }
  sample = (int16_t *)buffer.pDataBuffer;
  for(int i = buffer.sampleRate - samplesPlayed; i < buffer.sampleRate;i++){
    // one sample at a time
    float value = i*cycles*2*M_PI/buffer.sampleRate;
    *(sample) = buffer.volume * sin(value);
    *(sample + 1) = buffer.volume * sin(value);
    sample = sample + 2;
  }
}

HRESULT initSound()
{
  //Sound
  HRESULT hr;
  hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr))
    return hr;
  IXAudio2 *audioObject;
  if (FAILED(hr = XAudio2Create(&audioObject, 0, XAUDIO2_DEFAULT_PROCESSOR)))
  {
    return hr;
  };
  IXAudio2MasteringVoice *masteringVoice;
  if (FAILED(hr = audioObject->CreateMasteringVoice(&masteringVoice)))
  {
    return hr;
  }

  WAVEFORMATEX wfx = {WAVE_FORMAT_PCM, 2, globalAudioBuffer.sampleRate, globalAudioBuffer.sampleRate * (2 * globalAudioBuffer.wBitsPerSample) / 8, (2 * globalAudioBuffer.wBitsPerSample) / 8, globalAudioBuffer.wBitsPerSample, 0};

  fillBufferWithSineWave(globalAudioBuffer);

  XAUDIO2_BUFFER buffer = {0};
  buffer.Flags = XAUDIO2_END_OF_STREAM;
  buffer.AudioBytes = globalAudioBuffer.bufferElements;
  buffer.pAudioData = globalAudioBuffer.pDataBuffer;
  buffer.LoopLength = globalAudioBuffer.sampleRate;
  buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
  if (FAILED(hr = audioObject->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX *)&wfx)))
    return hr;

  if (FAILED(hr = pSourceVoice->SubmitSourceBuffer(&buffer)))
    return hr;

  if (FAILED(hr = pSourceVoice->Start(0)))
    return hr;
}

void renderGradient(backBuffer buffer, int xOffset)
{

  uint8_t *row = (uint8_t *)buffer.memory;

  for (int y = 0; y < buffer.height; y++)
  {
    uint32_t *pixel = (uint32_t *)row;
    for (int x = 0; x < buffer.width; x++)
    {
      *pixel = (0xAA << 8) | ((x + xOffset) * 255 / buffer.width);
      pixel++;
    }
    row += buffer.pitch;
  }
}

void resizeDIBSecion(backBuffer *buffer, int width, int height)
{
  if (buffer->memory)
  {
    VirtualFree(buffer->memory, 0, MEM_RELEASE);
  }

  buffer->width = width;
  buffer->height = height;

  buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
  buffer->info.bmiHeader.biWidth = buffer->width;
  buffer->info.bmiHeader.biHeight = -buffer->height;
  buffer->info.bmiHeader.biPlanes = 1;
  buffer->info.bmiHeader.biBitCount = 32;
  buffer->info.bmiHeader.biCompression = BI_RGB;

  int bitmapMemorySize = buffer->bytesPerPixel * buffer->width * buffer->height;
  buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  buffer->pitch = buffer->bytesPerPixel * buffer->width;
}

void displayBufferInWindow(backBuffer *buffer, Dimension windowDimension, HDC deviceContext)
{
  StretchDIBits(deviceContext,
                0, 0, windowDimension.width, windowDimension.height,
                0, 0, buffer->width, buffer->height,
                buffer->memory,
                &(buffer->info),
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
    renderGradient(globalBackBuffer, 100);
    Dimension dimension = getWindowDimension(hwnd);
    displayBufferInWindow(&globalBackBuffer, dimension, deviceContext);
    EndPaint(hwnd, &paintStruct);
  }
  break;
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP:
  {
    bool wasDown = (lParam & (1 << 30)) != 0;
    bool isDown = (lParam & (1 << 31)) != 0;
    bool isAltPressed = (lParam & (1 << 29)) != 0;
    if (wasDown)
      OutputDebugStringA("was down\n");
    if (isDown)
      OutputDebugStringA("is down\n");
    switch (wParam)
    {
    case VK_RIGHT:
    {
      OutputDebugStringA("RIGHT");
      xOffset += 50;
    } break;
    case 'A':
    {
      cycles += 32;
      fillBufferWithSineWave(globalAudioBuffer);

    } break;
    case VK_F4:
    {
      if (isAltPressed)
      {
        running = false;
      }
    }
    }
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

  //Window
  WNDCLASSA windowClass = {};
  resizeDIBSecion(&globalBackBuffer, 800, 800);
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
    if (SUCCEEDED(initSound()))
    {
      OutputDebugStringA("Sound initialized\n");
    }

    if (windowHandle)
    {
      running = true;
      while (running)
      {
        MSG message;
        while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
        {
          if (message.message == WM_QUIT)
          {
            running = false;
          }
          TranslateMessage(&message);
          DispatchMessage(&message);
        }
        /*
    DWORD dwResult;    
    for (DWORD i=0; i< XUSER_MAX_COUNT; i++ )
    {
        XINPUT_STATE state;
        ZeroMemory( &state, sizeof(XINPUT_STATE) );

        // Simply get the state of the controller from XInput.
        dwResult = XInputGetState( i, &state );
        char buffer[32];
        sprintf(buffer, "%d", state.dwPacketNumber);
        OutputDebugStringA(buffer);
        
        if( dwResult == ERROR_SUCCESS )
        {
            // Controller is connected
        }
        else
        {
            // Controller is not connected
        }
    }
*/
        //playSound();
        renderGradient(globalBackBuffer, xOffset);
        HDC deviceContext = GetDC(windowHandle);
        Dimension dimension = getWindowDimension(windowHandle);
        displayBufferInWindow(&globalBackBuffer, dimension, deviceContext);
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