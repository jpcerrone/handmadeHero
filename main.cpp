#include <iostream>
#include <windows.h>
#include <Xinput.h>
#include <Audioclient.h>
#include <Audiopolicy.h>
#include <initguid.h>
#include <shobjidl.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include "test.h"
#include <avrt.h>
#include <assert.h>
#include <mmsystem.h>
#include <math.h>

#define REFTIMES_PER_SEC 10'000'000 // 100 nanoscend units, 1 seconds
#define REFTIMES_PER_MILLISEC 10'000

#define LOG(p_string) std::cout << p_string << std::endl

/* struct Dimension{
    int width, height;
    int getWidth();
}; */

int Dimension::getWidth()
{
    return width;
}

int xOffset = 0;
static bool gameRunning;

struct Bitmap
{
    void *memory;
    BITMAPINFO info;
    Dimension dimensions;
};

static Dimension clientWindowDimensions;
static Bitmap globalBitmap;

// Define a function pointer with the XInputGetState signature
typedef DWORD(WINAPI *XInputGetState_t)(DWORD dwUserIndex, XINPUT_STATE *pState);
XInputGetState_t xInputGetState;
#define XInputGetState xInputGetState // Use the same name as defined in the dll

void renderArgFlag()
{
    const uint32_t lightBlueColor = 0xadd8e6; // RGB
    const uint32_t whiteColor = 0xFFFFFF;     // RGB
    uint32_t *pixel = (uint32_t *)globalBitmap.memory;
    for (int y = 0; y < globalBitmap.dimensions.height / 3; y++)
    {
        for (int x = 0; x < globalBitmap.dimensions.width; x++)
        {
            *pixel = lightBlueColor;
            pixel++;
        }
    }
    for (int y = globalBitmap.dimensions.height / 3; y < globalBitmap.dimensions.height * 2 / 3; y++)
    {
        for (int x = 0; x < globalBitmap.dimensions.width; x++)
        {
            *pixel = whiteColor;
            pixel++;
        }
    }
    for (int y = globalBitmap.dimensions.height * 2 / 3; y < globalBitmap.dimensions.height; y++)
    {
        for (int x = 0; x < globalBitmap.dimensions.width; x++)
        {
            *pixel = lightBlueColor;
            pixel++;
        }
    }

    Dimension sunPosition = {globalBitmap.dimensions.width / 2, globalBitmap.dimensions.height / 2};
    int radius = 100;
    const uint32_t yellowColor = 0xFFBF00; // RGB
    pixel = (uint32_t *)globalBitmap.memory;
    pixel += (globalBitmap.dimensions.height / 2 - radius) * (globalBitmap.dimensions.width) + (globalBitmap.dimensions.width / 2 - radius);
    for (int y = globalBitmap.dimensions.height / 2 - radius; y < globalBitmap.dimensions.height / 2 + radius; y++)
    {
        for (int x = globalBitmap.dimensions.width / 2 - radius; x < globalBitmap.dimensions.width / 2 + radius; x++)
        {
            *pixel = yellowColor;
            pixel++;
        }
        pixel += globalBitmap.dimensions.width - 2 * radius;
    }
}

void renderGradient(int xOffset)
{
    // pixel = 4B = 32b
    uint32_t *pixel = (uint32_t *)globalBitmap.memory;
    for (int y = 0; y < globalBitmap.dimensions.height; y++)
    {
        for (int x = 0; x < globalBitmap.dimensions.width; x++)
        {
            uint8_t red = 0x00;
            uint8_t green = 0;
            uint8_t blue = x + xOffset;
            //*pixel = 0x000000FF; //xx RR GG BB -> little endian, most significant bits get loaded last (xx)
            *pixel = (green << 8) | blue;
            pixel++;
        }
    }
}

void resizeDibSection(int width, int height)
{
    if (globalBitmap.memory)
    {
        VirtualFree(globalBitmap.memory, 0, MEM_RELEASE);
    }

    globalBitmap.dimensions = {width, height};

    BITMAPINFOHEADER bmInfoHeader = {};
    bmInfoHeader.biSize = sizeof(bmInfoHeader);
    bmInfoHeader.biCompression = BI_RGB;
    bmInfoHeader.biWidth = width;
    bmInfoHeader.biHeight = -height; // Negative means it'll be filled top-down
    bmInfoHeader.biPlanes = 1;       // MSDN sais it must be set to 1, legacy reasons
    bmInfoHeader.biBitCount = 32;    // R+G+B+padding each 8bits
    globalBitmap.info.bmiHeader = bmInfoHeader;

    globalBitmap.memory = VirtualAlloc(0, width * height * 4, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void updateWindow(HDC deviceContext, int srcWidth, int srcHeight, int windowWidth, int windowHeight, void *bitMapMemory, BITMAPINFO bitmapInfo)
{
    StretchDIBits(deviceContext, 0, 0, windowWidth, windowHeight, 0, 0, srcWidth, srcHeight, bitMapMemory, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

Dimension getWindowDimension(HWND windowHandle)
{
    RECT clientRect;
    GetClientRect(windowHandle, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;
    Dimension retDimension = {width, height};
    return retDimension;
}

void openFileAndDisplayName()
{
    IFileOpenDialog *openDialog;
    // Create the FileOpenDialog object.
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_PPV_ARGS(&openDialog))))
    {
        if (SUCCEEDED(openDialog->Show(NULL)))
        {
            IShellItem *itemSelected;
            if (SUCCEEDED(openDialog->GetResult(&itemSelected)))
            {
                PWSTR filePathOfItem;
                if (SUCCEEDED(itemSelected->GetDisplayName(SIGDN_FILESYSPATH, &filePathOfItem)))
                {
                    MessageBoxW(NULL, filePathOfItem, L"File Path", MB_OK);
                    CoTaskMemFree(filePathOfItem);
                }
                itemSelected->Release();
            }
        }
        openDialog->Release();
    }
}
struct AudioState_t
{
    WAVEFORMATEX *myFormat;
    IAudioClient *audioClient;
    IAudioRenderClient *renderClient;
    BYTE *data;
    UINT32 bufferFrameCount;
    float waveOffset;
    float frequency = 440;
};
static AudioState_t AudioState;

HRESULT LoadSineWave(UINT32 framesToWrite, BYTE *bufferLocation, int samplesPerSec)
{
    int samplesPerWave = samplesPerSec / AudioState.frequency;

    int16_t volume = 3000;
    int16_t *sample = (int16_t *)bufferLocation;
    for (int i =0; i < framesToWrite; i++)
    { // The size of an audio frame is the number of channels in the stream multiplied by the sample size
        {
            AudioState.waveOffset += ((float)1 / (float)samplesPerWave);
            float sinValue = sinf(2.0f * (float)M_PI * AudioState.waveOffset);
            *sample = sinValue * volume;
            *(sample + 1) = sinValue * volume;
        }
        sample += 2;
    }
    AudioState.waveOffset -= (int)AudioState.waveOffset; // Keep it between 0 and 1 to avoid overflow.
    return S_OK;
}

HRESULT LoadData(UINT32 framesToWrite, BYTE *bufferLocation, int samplesPerSec)
{
    return LoadSineWave(framesToWrite, bufferLocation, samplesPerSec);
}

void initAudioStream()
{
    int bufferSizeInSeconds = REFTIMES_PER_SEC/15;

    HRESULT hr;
    IMMDeviceEnumerator *enumerator;
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    assert(SUCCEEDED(hr));

    IMMDevice *device;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    assert(SUCCEEDED(hr));

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&(AudioState.audioClient));
    assert(SUCCEEDED(hr));

    hr = AudioState.audioClient->GetMixFormat(&AudioState.myFormat);
    assert(SUCCEEDED(hr));

    WAVEFORMATEXTENSIBLE *waveFormatExtensible = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(AudioState.myFormat);
    waveFormatExtensible->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    waveFormatExtensible->Format.wBitsPerSample = 16;
    waveFormatExtensible->Format.nBlockAlign = (AudioState.myFormat->wBitsPerSample / 8) * AudioState.myFormat->nChannels;
    waveFormatExtensible->Format.nAvgBytesPerSec = waveFormatExtensible->Format.nSamplesPerSec * waveFormatExtensible->Format.nBlockAlign;
    waveFormatExtensible->Samples.wValidBitsPerSample = 16;

    hr = AudioState.audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferSizeInSeconds /* 1 sec*/, 0, AudioState.myFormat, NULL);
    assert(SUCCEEDED(hr));

    hr = AudioState.audioClient->GetBufferSize(&AudioState.bufferFrameCount);
    assert(SUCCEEDED(hr));

    hr = AudioState.audioClient->GetService(IID_PPV_ARGS(&AudioState.renderClient));
    assert(SUCCEEDED(hr));

    hr = AudioState.renderClient->GetBuffer(AudioState.bufferFrameCount, &AudioState.data);
    assert(SUCCEEDED(hr));

    // Load Data
    AudioState.waveOffset = 0;
    hr = LoadData(AudioState.bufferFrameCount, AudioState.data, AudioState.myFormat->nSamplesPerSec);
    assert(SUCCEEDED(hr));

    hr = AudioState.renderClient->ReleaseBuffer(AudioState.bufferFrameCount, AUDCLNT_BUFFERFLAGS_SILENT); // This flag eliminates the need for the client to explicitly write silence AudioState.data to the rendering buffer.
    assert(SUCCEEDED(hr));

    hr = AudioState.audioClient->Start();
    assert(SUCCEEDED(hr));

    // Should release all these in a destructor maybe

    /*         AudioState.renderClient->Release();
        CoTaskMemFree(&AudioState.myFormat);
        audioClient->Release();
        device->Release();
        enumerator->Release(); */
}

void playAudioStream()
{
    // Calculate the actual duration of the allocated buffer.
    REFERENCE_TIME hnsActualDuration = (double)REFTIMES_PER_SEC *
                                       AudioState.bufferFrameCount / AudioState.myFormat->nSamplesPerSec;
    // See how much buffer space is available.
    UINT32 numFramesPadding;
    HRESULT hr = AudioState.audioClient->GetCurrentPadding(&numFramesPadding);
    assert(SUCCEEDED(hr));

    // NOTE(nick): output sound
    UINT32 numFramesAvailable = AudioState.bufferFrameCount - numFramesPadding;

    // Grab the next empty buffer from the audio device.
    hr = AudioState.renderClient->GetBuffer(numFramesAvailable, &AudioState.data);
    assert(SUCCEEDED(hr));

    // Load the buffer with AudioState.data from the audio source.
    hr = LoadData(numFramesAvailable, AudioState.data, AudioState.myFormat->nSamplesPerSec);
    assert(SUCCEEDED(hr));

    hr = AudioState.renderClient->ReleaseBuffer(numFramesAvailable, 0);
    assert(SUCCEEDED(hr));
}

LRESULT CALLBACK WindowProc(HWND windowHandle, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT returnVal = 0;
    switch (uMsg)
    {
    case WM_SYSKEYDOWN:
    {
        bool isAltDown = lParam & (1 << 29);
        if (wParam == VK_F4 && isAltDown)
        {
            gameRunning = false;
        }
    }
    break;
    case WM_SYSKEYUP:
    {
    }
    break;
    case WM_KEYUP:
    {
    }
    break;
    case WM_KEYDOWN:
    {
        bool wasDown = lParam & (1 << 30);
        if (wParam == VK_SPACE)
        {
            if (!wasDown)
            {
                openFileAndDisplayName();
            }
            std::cout << "SPACE" << wasDown << std::endl;
        }
        if (wParam == VK_UP){
            AudioState.frequency *= 2;
        }
        if (wParam == VK_DOWN){
            AudioState.frequency /= 2;
        }
    }
    break;
    case WM_SIZE:
    {
        clientWindowDimensions = getWindowDimension(windowHandle);
    }
    break;
    case WM_CLOSE:
    {
#ifdef _DEBUG
        std::cout << "DEBUG: exit popup" << std::endl;
#endif
        if (MessageBox(windowHandle, "Sure you want to exit?", "Jodot - Exiting", MB_YESNO) == IDYES)
        {
            gameRunning = false;
            DestroyWindow(windowHandle);
        };
    }
    break;
    case WM_DESTROY:
    {
        // ??? Handle as an error
        DestroyWindow(windowHandle);
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT paintStruct;
        HDC hdc = BeginPaint(windowHandle, &paintStruct);
        // renderGradient(xOffset);
        renderArgFlag();
        updateWindow(hdc, globalBitmap.dimensions.width, globalBitmap.dimensions.height, clientWindowDimensions.width, clientWindowDimensions.height, globalBitmap.memory, globalBitmap.info);
        EndPaint(windowHandle, &paintStruct);
    }
    break;
    default:
    {
        returnVal = DefWindowProc(windowHandle, uMsg, wParam, lParam);
    }
    break;
    }
    return returnVal;
};

void loadXInput()
{
    HMODULE handle = LoadLibrary("Xinput1_4.dll");
    if (handle)
    {
        std::cout << "Loaded Xinput1_4.dll" << std::endl;
    }
    else
    {
        std::cout << "Couldnt find Xinput dll" << std::endl;
        return;
    }
    xInputGetState = (XInputGetState_t)GetProcAddress(handle, "XInputGetState");
}

// hInstance: handle to the .exe
// hPrevInstance: not used since 16bit windows
// WINAPI: calling convention, tells compiler order of parameters
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    WNDCLASS wc = {};
    gameRunning = true;

    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance ? hInstance : GetModuleHandle(nullptr);
    wc.lpszClassName = "Engine";

    globalBitmap.dimensions = {1920, 1080};
    resizeDibSection(globalBitmap.dimensions.width, globalBitmap.dimensions.height);

    CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    //  __FILE__, __LINE__,  try these out !
    loadXInput();
    initAudioStream();

    if (RegisterClass(&wc))
    {
        HWND windowHandle = CreateWindowEx(0, wc.lpszClassName, "Jodot Engine", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
        if (windowHandle)
        {
            HDC windowDeviceContext = GetDC(windowHandle);
            while (gameRunning)
            {
                // Input...
                DWORD dwResult;
                for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
                {
                    XINPUT_STATE state;
                    ZeroMemory(&state, sizeof(XINPUT_STATE));

                    // Simply get the state of the controller from XInput.
                    dwResult = XInputGetState(i, &state);

                    if (dwResult == ERROR_SUCCESS)
                    {
                        // Controller is connected
                        WORD buttons = state.Gamepad.wButtons;
                        if (buttons & XINPUT_GAMEPAD_A)
                        { // ex: buttons:0101, A:0001, buttons & A:0001, casting anything other than 0 to bool returns true.
                            std::cout << "A" << std::endl;
                        }
                        if (buttons & XINPUT_GAMEPAD_B)
                        {
                            std::cout << "B" << std::endl;
                        }
                    }
                    else
                    {
                        // Controller is not connected
                    }
                }

                MSG message;
                if (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
                if (message.message == WM_QUIT)
                {
                    gameRunning = false;
                    break;
                }
                // renderGradient(xOffset);
                renderArgFlag();
                playAudioStream();
                xOffset++;
                updateWindow(windowDeviceContext, globalBitmap.dimensions.width, globalBitmap.dimensions.height, clientWindowDimensions.width, clientWindowDimensions.height, globalBitmap.memory, globalBitmap.info);
            }
        }
        else
        {
            std::cout << "Failure getting window handle";
        }
    }
    else
    {
        std::cout << "Failure registering class";
    }
    CoUninitialize();
    return 0;
};