#pragma once

float waveOffset = 0;
float frequency = 440;

struct ButtonState{
    int halfTransitionCount;
    bool isDown;
};

struct GameInputState{
    union{
        ButtonState buttons[4];
        struct {
            ButtonState A_Button;
            ButtonState B_Button;
            ButtonState X_Button;
            ButtonState Y_Button;
        } ;
    };
    // TODO: handle analog sticks
};

// TODO: handle multiple controllers:
/*
struct game_input {
  game_controller_input Controllers[4];
};
*/