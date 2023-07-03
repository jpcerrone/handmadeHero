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
#include <mmsystem.h>
#include <math.h>
#include <cinttypes>

#include "gameCode.h"

#define REFTIMES_PER_SEC 10'000'000 // 100 nanoscend units, 1 seconds
#define REFTIMES_PER_MILLISEC 10'000 


static int desiredFPS = 60;
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

void concatenateStrings(char* firstString, char* secondString, char* outString) {
    while (*firstString != '\0') {
        *outString++ = *firstString++;
    };
    while(*secondString != '\0') {
        *outString++ = *secondString++;
    };
    *outString = '\0';
}

void copyString(char* ogString, int ogSize, char* newString, int newSize) {
    for (int i = 0; i < newSize; i++) {
        *newString = *ogString;
        newString++;
        ogString++;
    }
    *newString = '\0';
};

void getDirectoryFromPath(char* path, DWORD exeFileNameLength, char* directory) {
    char* charPtr = path + exeFileNameLength;
    int newSize = exeFileNameLength;
    while (*charPtr != '\\') {
        newSize --;
        charPtr--;
    }
    copyString(path, exeFileNameLength, directory, newSize+1);
};

struct GameCode_t {
    HMODULE dllHandle;
    FILETIME lastWriteTime = { 0, 0 };
    updateAndRender_t *updateAndRender;
};
static GameCode_t GameCode; // TODO: casey makes this a local variable. It may make more sense to do so.
void loadGameCode()
{
    char* dllName = "gameCode.dll";
    char* loadedDllName = "gameCode_load.dll";

    char exeFileName[MAX_PATH];
    DWORD exeFileNameLength = GetModuleFileName(0, exeFileName, sizeof(exeFileName));
    char folderPath[MAX_PATH];
    getDirectoryFromPath(exeFileName, exeFileNameLength, folderPath);

    char fullDllPath[MAX_PATH];
    concatenateStrings(folderPath, dllName, fullDllPath);
    char loadedDllPath[MAX_PATH];
    concatenateStrings(folderPath, loadedDllName, loadedDllPath);

    WIN32_FILE_ATTRIBUTE_DATA info;
    Assert(GetFileAttributesEx(fullDllPath, GetFileExInfoStandard, &info));

    LONG fileTimeComparisson = CompareFileTime(&info.ftLastWriteTime, &GameCode.lastWriteTime);
    Assert(fileTimeComparisson != -1);
    if (fileTimeComparisson != 0){
        if (GameCode.dllHandle) { // The first time through there'll be no handle.
            bool couldFree = FreeLibrary(GameCode.dllHandle);
            Assert(couldFree);
        }
        GameCode.lastWriteTime = info.ftLastWriteTime;
        while (!CopyFile(fullDllPath, loadedDllPath, FALSE)) {};// Perform CopyFile until it succeeds. Suposedely day 39 has a solution for this.
        GameCode.dllHandle = LoadLibrary(loadedDllPath);
        if (GameCode.dllHandle)
        {
            LOG(loadedDllPath);
        }
        else
        {
            LOG("Couldnt find gameCode dll");
            return;
        }
        GameCode.updateAndRender = (updateAndRender_t*)GetProcAddress(GameCode.dllHandle, "updateAndRender");
        Assert(GameCode.updateAndRender); //TODO: change
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
    // For now we ignore the size of the window to get 1:1 pixel rendering
    StretchDIBits(deviceContext, 0, 0, srcWidth, srcHeight, 0, 0, srcWidth, srcHeight, bitMapMemory, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
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

struct Record_t {
    int inputsWritten;
    int inputsRead;
    void* gameState;
    bool recording;
    bool playing;
};
static Record_t Record;

void initWASAPI()
{
    int framesOfLatency = 2; // 1 frame of latency seems to not be possible.
    int bufferSizeInSeconds = (int)(REFTIMES_PER_SEC / (desiredFPS / (float)framesOfLatency));

    HRESULT hr;
    IMMDeviceEnumerator *enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    Assert(SUCCEEDED(hr));

    IMMDevice *device;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    Assert(SUCCEEDED(hr));

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&(AudioState.audioClient));
    Assert(SUCCEEDED(hr));

    hr = AudioState.audioClient->GetMixFormat(&AudioState.myFormat);
    Assert(SUCCEEDED(hr));

    WAVEFORMATEXTENSIBLE *waveFormatExtensible = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(AudioState.myFormat);
    waveFormatExtensible->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    waveFormatExtensible->Format.wBitsPerSample = 16;
    waveFormatExtensible->Format.nBlockAlign = (AudioState.myFormat->wBitsPerSample / 8) * AudioState.myFormat->nChannels;
    waveFormatExtensible->Format.nAvgBytesPerSec = waveFormatExtensible->Format.nSamplesPerSec * waveFormatExtensible->Format.nBlockAlign;
    waveFormatExtensible->Samples.wValidBitsPerSample = 16;

    hr = AudioState.audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferSizeInSeconds, 0, AudioState.myFormat, NULL);
    Assert(SUCCEEDED(hr));

    hr = AudioState.audioClient->GetBufferSize(&AudioState.bufferFrameCount);
    Assert(SUCCEEDED(hr));

    hr = AudioState.audioClient->GetService(IID_PPV_ARGS(&AudioState.renderClient));
    Assert(SUCCEEDED(hr));

    AudioState.buffer = (BYTE *)VirtualAlloc(0, waveFormatExtensible->Format.nAvgBytesPerSec, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Assert(AudioState.buffer);

    hr = AudioState.audioClient->Start();
    Assert(SUCCEEDED(hr));

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
    BYTE *renderBuffer;
    HRESULT hr = AudioState.renderClient->GetBuffer(framesToWrite, &renderBuffer);
    Assert(SUCCEEDED(hr));

    int16_t *renderSample = (int16_t *)renderBuffer;
    int16_t *inputSample = (int16_t *)AudioState.buffer;
    for (int i = 0; i < framesToWrite; i++)
    {
        *renderSample = *inputSample;
        *(renderSample + 1) = *(inputSample + 1);
        renderSample += 2;
        inputSample += 2;
    }

    hr = AudioState.renderClient->ReleaseBuffer(framesToWrite, 0);
    Assert(SUCCEEDED(hr));
}

LRESULT CALLBACK WindowProc(HWND windowHandle, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT returnVal = 0;
    switch (uMsg)
    {
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYUP:
    case WM_KEYDOWN:
    {
        Assert(!"no keyboard message here");
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
        if (messageBoxSucceded == IDYES)
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
        LOG("Loaded Xinput1_4.dll");
    }
    else
    {
        LOG("Couldnt find Xinput dll");
        return;
    }
    xInputGetState = (XInputGetState_t)GetProcAddress(handle, "XInputGetState");
}

WRITE_FILE(writeFile)
{
    HANDLE fileHandle = CreateFile(path, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle)
    {
        DWORD bytesWritten = 0;
        DWORD error;
        if (WriteFile(fileHandle, content, (DWORD)bytesToWrite, &bytesWritten, NULL) && (bytesToWrite == bytesWritten))
        {
        }
        else
        {
            error = GetLastError();
        }
        CloseHandle(fileHandle);
        return true;
    }
    return false;
};

APPEND_TO_FILE(appendToFile) {
    HANDLE fileHandle = CreateFile(path, FILE_APPEND_DATA, NULL, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle)
    {
        DWORD bytesWritten = 0;
        DWORD error;
        if (WriteFile(fileHandle, content, (DWORD)bytesToWrite, &bytesWritten, NULL) && (bytesToWrite == bytesWritten))
        {
        }
        else
        {
            error = GetLastError();
        }
        CloseHandle(fileHandle);
        return true;
    }
    return false;
}

READ_FILE(readFile)
{
    HANDLE fileHandle = CreateFile(path, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    FileReadResult result = {};
    if (fileHandle)
    {
        LARGE_INTEGER size;
        if (GetFileSizeEx(fileHandle, &size))
        {
            result.size = size.QuadPart;
            result.memory = VirtualAlloc(0, (SIZE_T)result.size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (result.memory)
            {
                DWORD bytesRead = 0;
                if (ReadFile(fileHandle, result.memory, (DWORD)result.size, &bytesRead, NULL) && (bytesRead == result.size))
                {
                }
                else
                {
                    // Log failure
                }
            }
        }
        else
        {
            // Log failure
        }
        CloseHandle(fileHandle);
    }
    return result;
}

FREE_FILE_MEMORY(freeFileMemory)
{
    VirtualFree(memory, 0, MEM_RELEASE);
}

LARGE_INTEGER getEndPerformanceCount(){
    LARGE_INTEGER endPerformanceCount;
    QueryPerformanceCounter(&endPerformanceCount);
    return endPerformanceCount;
}

float getEllapsedMS(LARGE_INTEGER endPerformanceCount, LARGE_INTEGER startPerformanceCount, LARGE_INTEGER performanceFrequency){
    return ((float)(endPerformanceCount.QuadPart - startPerformanceCount.QuadPart) / (float)performanceFrequency.QuadPart) * 1000;          
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

    MMRESULT canQueryEveryMs = timeBeginPeriod(1); // TODO: maybe call timeEndPeriod?
    Assert(canQueryEveryMs == TIMERR_NOERROR);

    globalBitmap.dimensions = {1080, 720};
    resizeDibSection(globalBitmap.dimensions.width, globalBitmap.dimensions.height);

    HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    Assert(SUCCEEDED(init));

    loadXInput();
    loadGameCode();

    ThreadContext thread = {};

    if (RegisterClass(&wc))
    {
        HWND windowHandle = CreateWindowEx(0, wc.lpszClassName, "Jodot Engine", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, globalBitmap.dimensions.width, globalBitmap.dimensions.height, 0, 0, hInstance, 0);
        if (windowHandle)
        {
            HDC windowDeviceContext = GetDC(windowHandle);
            int monitorRefreshRate = GetDeviceCaps(windowDeviceContext, VREFRESH);
            if (monitorRefreshRate) { // This sets the refresh rate to 59 on my monitor.
                desiredFPS = monitorRefreshRate;
            }
            float desiredFrameTimeInMS = 1000.0f / desiredFPS;

            initWASAPI();

            // Timing
            LARGE_INTEGER startPerformanceCount;
            QueryPerformanceCounter(&startPerformanceCount);
            LARGE_INTEGER performanceFrequency;
            QueryPerformanceFrequency(&performanceFrequency);

            // Main loop
            GameMemory memory;
            // Note: compile in 64 bit if allocating >= 1GB of memory. with dev console 64.
            memory.permanentStorageSize = 1 * 512 * 1024 * 1024; // MB;
            memory.permanentStorage = VirtualAlloc(0, (SIZE_T)memory.permanentStorageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            memory.transientStorageSize = 1 * 512 * 1024 * 1024; // MB;
            memory.transientStorage = VirtualAlloc(0, (SIZE_T)memory.transientStorageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            memory.isinitialized = false;
            memory.readFile = readFile;
            memory.writeFile = writeFile;
            memory.freeFileMemory = freeFileMemory;
            // Memory.TransientStorage = (uint8 *)Memory.PermanentStorage + Memory.PermanentStorageSize; //TODO: only 1 virtual alloc call
            GameInputState newState = {};

            Record = {};
            HANDLE stateRecHandle = CreateFile("gameState.rec", GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
            DWORD sizeHigh = (DWORD)(memory.permanentStorageSize << 32);
            DWORD sizeLow = memory.permanentStorageSize & 0x00000000FFFFFFFF;
            HANDLE stateFileMap = CreateFileMapping(stateRecHandle, 0, PAGE_READWRITE, sizeHigh , sizeLow, 0);
            Record.gameState = MapViewOfFile(stateFileMap, FILE_MAP_ALL_ACCESS, 0, 0, (SIZE_T)memory.permanentStorageSize);
            //DWORD error = GetLastError();
            while (gameRunning)
            {
                loadGameCode();

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
                            if (newState.A_Button.isDown)
                            {
                                newState.A_Button = {1, true};
                            }
                            else
                            {
                                newState.A_Button = {0, true};
                            }
                        } else{
                            newState.A_Button = {0, false};
                        }
                        if (buttons & XINPUT_GAMEPAD_B)
                        {
                            if (newState.B_Button.isDown)
                            {
                                newState.B_Button = {1, true};
                            }
                            else
                            {
                                newState.B_Button = {0, true};
                            }
                        } else{
                            newState.B_Button = {0, false};
                        }
                        if (state.Gamepad.sThumbLX){
                            if(state.Gamepad.sThumbLX >= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE){
                                newState.Left_Stick.xPosition = (state.Gamepad.sThumbLX - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / (32767.0f - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                LOG(newState.Left_Stick.xPosition);
                            } else if (state.Gamepad.sThumbLX <= -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE){
                                newState.Left_Stick.xPosition = (state.Gamepad.sThumbLX + XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / (32767.0f - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                LOG(newState.Left_Stick.xPosition);
                            } else{
                                newState.Left_Stick.xPosition = 0;
                            }
                        }
                        if (state.Gamepad.sThumbLY){
                            if(state.Gamepad.sThumbLY <= -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || state.Gamepad.sThumbLY >= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE){    
                                newState.Left_Stick.yPosition = state.Gamepad.sThumbLY / 32767.0f;
                            } else{
                                    newState.Left_Stick.yPosition = 0;
                            }
                        }
                    }
                    else
                    {
                        // Controller is not connected
                    }
                }
                POINT mousePos;
                GetCursorPos(&mousePos);
                ScreenToClient(windowHandle, &mousePos);
                newState.mousePosition.x = mousePos.x;
                newState.mousePosition.y = mousePos.y;

                MSG message;
                while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
                {
                    switch (message.message)
                    {
                    case WM_SYSKEYDOWN:
                    {   // Alt F4
                        bool isAltDown = message.lParam & (1 << 29);
                        if (message.wParam == VK_F4 && isAltDown)
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
                        switch(message.wParam){
                            case 'Z':{
                                newState.A_Button = {0, false};
                            } break;
                            case 'X':{
                                newState.B_Button = {0, false};
                            } break;
                            case VK_RIGHT:{
                                newState.Left_Stick.xPosition = 0.0;
                            }break;
                            case VK_LEFT:{
                                newState.Left_Stick.xPosition = 0.0;
                            }break;
                            default: break;
                        }
                    }
                    break;
                    case WM_KEYDOWN:
                    {
                        bool wasDown = message.lParam & (1 << 30);
                        switch(message.wParam){
                            case 'Z':{
                                newState.A_Button = {1, true};
                            } break;
                            case 'X':{
                                newState.B_Button = {1, true};
                            } break;
                            case 'R': { // Windows only for now.
                                if (!wasDown)
                                {
                                    if (!Record.recording) {
                                        CopyMemory(Record.gameState, memory.permanentStorage, (size_t)memory.permanentStorageSize);
                                        writeFile(&thread, "inputRecord.rec", &newState, 0);
                                        Record.recording = true;
                                        Record.inputsWritten = 0;
                                    } else {
                                        CopyMemory(memory.permanentStorage, Record.gameState, (size_t)memory.permanentStorageSize);
                                        Record.recording = false;
                                        Record.inputsRead = 0;
                                        Record.playing = true;
                                    }
                                }
                            } break;
                            case VK_RIGHT:{
                                newState.Left_Stick.xPosition = 1.0;
                            }break;
                            case VK_LEFT:{
                                newState.Left_Stick.xPosition = -1.0;
                            }break;
                            case VK_SPACE:
                            {
                                if (!wasDown)
                                {
                                    openFileAndDisplayName();
                                }
                                LOG("SPACE" + wasDown);
                            } break;
                            default: break;
                        }
                    }
                    break;
                    case WM_RBUTTONDOWN: { //TODO: handle isDown, (hh day 25)
                        newState.Mouse_R.isDown = true;
                    } break;
                    case WM_RBUTTONUP: {
                        newState.Mouse_R.isDown = false;
                    } break;
                    case WM_LBUTTONDOWN: {
                        newState.Mouse_L.isDown = true;
                    } break;
                    case WM_LBUTTONUP: {
                        newState.Mouse_L.isDown = false;
                    } break;
                    case WM_QUIT:
                    {
                        gameRunning = false;
                    }
                    break;
                    default:
                    {
                        TranslateMessage(&message);
                        DispatchMessage(&message);
                    }
                    break;
                    }
                }
                if (Record.recording) {
                    appendToFile(&thread, "inputRecord.rec", &newState, sizeof(newState));
                    Record.inputsWritten++;
                }
                if (Record.playing) {
                    FileReadResult inputRecord = readFile(&thread, "inputRecord.rec");
                    if (Record.inputsRead <= Record.inputsWritten) {
                        GameInputState* input = ((GameInputState*)inputRecord.memory) + Record.inputsRead;
                        Assert(input != nullptr);
                        newState = *input;
                        Record.inputsRead++;
                    }
                    else {
                        Record.inputsRead = 0;
                        CopyMemory(memory.permanentStorage, Record.gameState, (size_t)memory.permanentStorageSize);
                    }
                }

                UINT32 numFramesPadding;
                HRESULT hr = AudioState.audioClient->GetCurrentPadding(&numFramesPadding);
                Assert(SUCCEEDED(hr));

                UINT32 numFramesAvailable = AudioState.bufferFrameCount - numFramesPadding;
                GameCode.updateAndRender(&thread, &memory, numFramesAvailable, AudioState.buffer, AudioState.myFormat->nSamplesPerSec, globalBitmap.memory, globalBitmap.dimensions.width, globalBitmap.dimensions.height, newState);
                fillWASAPIBuffer(numFramesAvailable);
                updateWindow(windowDeviceContext, globalBitmap.dimensions.width, globalBitmap.dimensions.height, clientWindowDimensions.width, clientWindowDimensions.height, globalBitmap.memory, globalBitmap.info);

                // Timing
                LARGE_INTEGER endPerformanceCount = getEndPerformanceCount();
                float elapsedMilliseconds = getEllapsedMS(endPerformanceCount, startPerformanceCount, performanceFrequency);         
                while (elapsedMilliseconds < desiredFrameTimeInMS){
                    DWORD timeToSleep = (DWORD) (desiredFrameTimeInMS - elapsedMilliseconds);
                    if (timeToSleep > 0) {
                        Sleep(timeToSleep);
                    }
                    elapsedMilliseconds = getEllapsedMS(getEndPerformanceCount(), startPerformanceCount, performanceFrequency);
                }

                endPerformanceCount = getEndPerformanceCount();
                elapsedMilliseconds = getEllapsedMS(endPerformanceCount, startPerformanceCount, performanceFrequency);         
                float fps = 1000.0f / elapsedMilliseconds;
                printf("Frame time: %0.01fms. FPS: %0.01f\n ", elapsedMilliseconds, fps);
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