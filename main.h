#pragma once

struct GameMemory{
    bool isinitialized;
    uint64_t transientStorageSize;
    void* transientStorage;
    uint64_t permanentStorageSize;
    void* permanentStorage;
};

struct GameState{
    int xOffset;
    float waveOffset;
    float frequency;
};

struct ButtonState{
    int halfTransitionCount;
    bool isDown;
};

struct AxisState{
    float xPosition;
    float yPosition;
};

struct GameInputState{
    union{
        ButtonState buttons[4];
        AxisState axis[2];
        struct {
            ButtonState A_Button;
            ButtonState B_Button;
            ButtonState X_Button;
            ButtonState Y_Button;
            AxisState Left_Stick;
            AxisState Right_Stick;
        } ;
    };
};

// TODO: handle multiple controllers:
/*
struct game_input {
  game_controller_input Controllers[4];
};
*/