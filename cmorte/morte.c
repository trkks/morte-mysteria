/*******************************************************************************************
 * Morte mysteria
 ********************************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "raylib/raylib.h"

#define WINDOW_WIDTH 700
#define WINDOW_HEIGHT 400
#define LEVEL_WIDTH 2200
#define BACKGROUND_COLOR (Color){133, 31, 10, 255}

#define ANIMATION_FRAME_COUNT_CURSOR 19

enum Tag { GROUND };

typedef struct {
  enum Tag tag;
  Rectangle level_bounds;
} Level;

typedef struct {
  Texture2D *frames;
  size_t frame_count;
  size_t current_frame;
} Animation;

void Animation__update(Animation *self) {
  self->current_frame += 1;
  self->current_frame %= self->frame_count;
}

typedef struct {
  Level level;
  Vector2 gravity;
  Animation cursor_animation;
} MorteGame;

void MorteGame__free(MorteGame *self) { free(self->cursor_animation.frames); }

MorteGame game;

void initialize_level();
void load_content();

int main(void) {
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Morte mysteria");

  SetTargetFPS(60);
  DisableCursor();

  initialize_level();

  load_content();

  while (!WindowShouldClose()) {
    // Update.
    //----------------------------------------------------------------------------------
    Animation__update(&game.cursor_animation);
    //----------------------------------------------------------------------------------

    // Draw.
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(BACKGROUND_COLOR);

    DrawTexture(
        game.cursor_animation.frames[game.cursor_animation.current_frame],
        GetMouseX(), GetMouseY(), WHITE);

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // De-Initialization.
  //--------------------------------------------------------------------------------------
  CloseWindow();

  MorteGame__free(&game);
  //--------------------------------------------------------------------------------------

  return 0;
}

void initialize_level() {
  game.level = (Level){
      .tag = GROUND,
      .level_bounds = {0, 0, LEVEL_WIDTH, WINDOW_HEIGHT},
  };

  game.gravity = (Vector2){0, -700};
}

void load_content() {
  game.cursor_animation = (Animation){
      .frames =
          (Texture2D *)malloc(ANIMATION_FRAME_COUNT_CURSOR * sizeof(Texture2D)),
      .frame_count = ANIMATION_FRAME_COUNT_CURSOR,
      .current_frame = 0,
  };
  for (size_t i = 0; i < game.cursor_animation.frame_count; i++) {
    char *filename = (char *)malloc((25 + 2 + 4 + 1) * sizeof(char));
    sprintf(filename, "content/kursori/kursori00%02d.png", (int)i + 1);
    game.cursor_animation.frames[i] = LoadTexture(filename);
    free(filename);
  }
}
