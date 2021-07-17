#include <Windows.h>
int CALLBACK WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR     lpCmdLine,
  int       nShowCmd
)
{
  if(MessageBoxA(0, "Handmade Hero", "Hero Handmade", MB_OKCANCEL|MB_ICONWARNING ) == IDOK){
    MessageBoxA(0, "Gato", "Gato", MB_RIGHT|MB_ICONWARNING );
  };
  return(0);
}