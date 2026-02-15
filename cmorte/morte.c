/*******************************************************************************************
 * Morte mysteria
 ********************************************************************************************/

#include "raylib/raylib.h"

#define WINDOW_WIDTH 700
#define WINDOW_HEIGHT 400

int main(void) {
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Morte mysteria");

  SetTargetFPS(60);
  DisableCursor();

  while (!WindowShouldClose()) {
    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(MAGENTA);

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}
