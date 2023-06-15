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
#include <cinttypes>
#include "main.cpp"

#define REFTIMES_PER_SEC 10'000'000 // 100 nanoscend units, 1 seconds
#define REFTIMES_PER_MILLISEC 10'000

#define LOG(p_string) std::cout << p_string << std::endl

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
    BYTE *buffer;
    UINT32 bufferFrameCount;
};
static AudioState_t AudioState;

void initWASAPI()
{
    int bufferSizeInSeconds = REFTIMES_PER_SEC/30;

    HRESULT hr;
    IMMDeviceEnumerator *enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
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

    hr = AudioState.audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferSizeInSeconds, 0, AudioState.myFormat, NULL);
    assert(SUCCEEDED(hr));

    hr = AudioState.audioClient->GetBufferSize(&AudioState.bufferFrameCount);
    assert(SUCCEEDED(hr));

    hr = AudioState.audioClient->GetService(IID_PPV_ARGS(&AudioState.renderClient));
    assert(SUCCEEDED(hr));

    AudioState.buffer = (BYTE*)VirtualAlloc(0, waveFormatExtensible->Format.nAvgBytesPerSec, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    assert(AudioState.buffer);
    
    hr = AudioState.audioClient->Start();
    assert(SUCCEEDED(hr));

    // Should release all these in a destructor maybe

    /*         AudioState.renderClient->Release();
        CoTaskMemFree(&AudioState.myFormat);
        audioClient->Release();
        device->Release();
        enumerator->Release(); */
}

void fillWASAPIBuffer(int framesToWrite)
{
    // Grab the next empty buffer from the audio device.
    BYTE* renderBuffer;
    HRESULT hr = AudioState.renderClient->GetBuffer(framesToWrite, &renderBuffer);
    assert(SUCCEEDED(hr));

    int16_t *renderSample = (int16_t *)renderBuffer;
    int16_t *inputSample = (int16_t *)AudioState.buffer;
    for(int i = 0; i < framesToWrite; i++){
        *renderSample = *inputSample;
        *(renderSample+1) = *(inputSample+1);
        renderSample+=2;
        inputSample+=2;
    }

    hr = AudioState.renderClient->ReleaseBuffer(framesToWrite, 0);
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
            LOG("SPACE" + wasDown);
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
        // TODO: Stop audio smoothly
        HRESULT messageBoxSucceded = MessageBox(windowHandle, "Sure you want to exit?", "Jodot - Exiting", MB_YESNO);
        if (messageBoxSucceded == IDYES){
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
/*     case WM_PAINT:
    {
        PAINTSTRUCT paintStruct;
        HDC hdc = BeginPaint(windowHandle, &paintStruct);
        //renderGraphics(globalBitmap.memory, globalBitmap.dimensions.width, globalBitmap.dimensions.height, xOffset);
        updateWindow(hdc, globalBitmap.dimensions.width, globalBitmap.dimensions.height, clientWindowDimensions.width, clientWindowDimensions.height, globalBitmap.memory, globalBitmap.info);
        EndPaint(windowHandle, &paintStruct);
    } 
    break; */
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
        LOG("Loaded Xinput1_4.dll");
    }
    else
    {
        LOG("Couldnt find Xinput dll");
        return;
    }
    xInputGetState = (XInputGetState_t)GetProcAddress(handle, "XInputGetState");
}

bool writeFile(char* path, void* content, uint64_t bytesToWrite){
    HANDLE fileHandle = CreateFile(path, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL , NULL);
    if (fileHandle){
        DWORD bytesWritten = 0;
        if (WriteFile(fileHandle, content, (DWORD)bytesToWrite, &bytesWritten, NULL) && (bytesToWrite == bytesWritten)){
        } else{
            // LOG ERROR
        }
        CloseHandle(fileHandle);
        return true;
    }
    return false;
};

FileReadResult readFile(char* path){
    HANDLE fileHandle = CreateFile(path, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    FileReadResult result = {};
    if (fileHandle){
        LARGE_INTEGER size;
        if (GetFileSizeEx(fileHandle, &size)){
            result.size = size.QuadPart;
            result.memory = VirtualAlloc(0, (SIZE_T)result.size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (result.memory){
                DWORD bytesRead = 0;
                if (ReadFile(fileHandle, result.memory, (DWORD)result.size, &bytesRead, NULL) && (bytesRead == result.size)){
                }else{
                    // Log failure
                }
            }
        }else{
            // Log failure
        }
        CloseHandle(fileHandle);
    }
    return result;
}

void freeFileMemory(void* memory){
    VirtualFree(memory, 0 , MEM_RELEASE);
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

    HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    assert(SUCCEEDED(init));

    initWASAPI();
    loadXInput();

    if (RegisterClass(&wc))
    {
        HWND windowHandle = CreateWindowEx(0, wc.lpszClassName, "Jodot Engine", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
        if (windowHandle)
        {
            HDC windowDeviceContext = GetDC(windowHandle);

            // Timing
            LARGE_INTEGER startPerformanceCount;
            QueryPerformanceCounter(&startPerformanceCount);
            LARGE_INTEGER performanceFrequency;
            QueryPerformanceFrequency(&performanceFrequency);

            // Main loop
            GameMemory memory;
            memory.permanentStorageSize = 64*1024*1024; //MB;
            memory.permanentStorage = VirtualAlloc(0, (SIZE_T)memory.permanentStorageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            memory.transientStorageSize = 1*1024*1024*1024; //GB;
            memory.transientStorage = VirtualAlloc(0, (SIZE_T)memory.transientStorageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            memory.isinitialized = false;
            //Memory.TransientStorage = (uint8 *)Memory.PermanentStorage + Memory.PermanentStorageSize; //TODO: only 1 virtual alloc call
            GameInputState oldState = {};
            GameInputState newState = {};

            while (gameRunning)
            {
                oldState = newState;
                newState = {};
                // Joypad Input  
                DWORD dwResult;
                for (DWORD i = 0; i < 1 /*XUSER_MAX_COUNT - 1 player only for now*/; i++)
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
                            if (oldState.A_Button.isDown){
                                newState.A_Button = {1, true};
                            } else{
                                newState.A_Button = {0, true};
                            }
                        }
                        if (buttons & XINPUT_GAMEPAD_B)
                        {
                            if (oldState.B_Button.isDown){
                                newState.B_Button = {1, true};
                            } else{
                                newState.B_Button = {0, true};
                            }
                        }
                        if (state.Gamepad.sThumbLX || state.Gamepad.sThumbLY){ // TODO: implement deadzone
                            newState.Left_Stick.xPosition = state.Gamepad.sThumbLX / 32767.0f;
                            newState.Left_Stick.yPosition = state.Gamepad.sThumbLY / 32767.0f;
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
                UINT32 numFramesPadding;
                HRESULT hr = AudioState.audioClient->GetCurrentPadding(&numFramesPadding);
                assert(SUCCEEDED(hr));

                UINT32 numFramesAvailable = AudioState.bufferFrameCount - numFramesPadding;
                updateAndRender(&memory, numFramesAvailable, AudioState.buffer, AudioState.myFormat->nSamplesPerSec, globalBitmap.memory, globalBitmap.dimensions.width, globalBitmap.dimensions.height, newState);
                fillWASAPIBuffer(numFramesAvailable);
                updateWindow(windowDeviceContext, globalBitmap.dimensions.width, globalBitmap.dimensions.height, clientWindowDimensions.width, clientWindowDimensions.height, globalBitmap.memory, globalBitmap.info);
                
                // Timing
                LARGE_INTEGER endPerformanceCount;
                QueryPerformanceCounter(&endPerformanceCount);
                float elapsedMilliseconds = ((float)(endPerformanceCount.QuadPart - startPerformanceCount.QuadPart) / (float)performanceFrequency.QuadPart)*1000;
                int fps = (int)((float)performanceFrequency.QuadPart / (float)(endPerformanceCount.QuadPart - startPerformanceCount.QuadPart));
                printf("Frame time: %0.01fms. FPS: %d\n ",elapsedMilliseconds, fps);

                startPerformanceCount = endPerformanceCount;
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