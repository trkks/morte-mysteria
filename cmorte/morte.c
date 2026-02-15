/*******************************************************************************************
 * Morte mysteria
 ********************************************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib/raylib.h"

#define WINDOW_WIDTH 700
#define WINDOW_HEIGHT 400
#define LEVEL_WIDTH 2200
#define BACKGROUND_COLOR (Color){133, 31, 10, 255}
#define BACKGROUND_TEXTURE_COUNT 3

#define ANIMATION_FRAME_COUNT_CURSOR 19
#define ANIMATION_LENGTH_MILLIS_CURSOR 750

enum Tag { GROUND };

typedef struct {
  enum Tag tag;
  Rectangle bounds;
} Level;

enum AnimationState { STOPPED, PLAYING_ONCE, LOOPING };

enum AnimationTiming { LINEAR, EASE_IN, EASE_OUT };

typedef struct {
  enum AnimationState state;
  size_t frame_count;
  size_t current_frame;
  unsigned length_millis;
  unsigned elapsed_millis;
  enum AnimationTiming timing;
  Texture2D *frames;
} Animation;

void Animation__update(Animation *self, float delta) {
  if (self->state == STOPPED) {
    return;
  }

  self->elapsed_millis += 1000 * delta;
  float t =
      fmin(1.0f, (float)self->elapsed_millis / (float)self->length_millis);

  switch (self->timing) {
  case LINEAR:
    self->current_frame = t * self->frame_count;
    break;
  case EASE_IN:
    self->current_frame = (1.0f - cosf(t * PI / 2.0f)) * self->frame_count;
    break;
  case EASE_OUT:
    self->current_frame = sinf(t * PI / 2.0f) * self->frame_count;
    break;
  }

  if (self->current_frame >= self->frame_count) {
    self->current_frame = 0;
    self->elapsed_millis = 0;

    if (self->state == PLAYING_ONCE) {
      self->state = STOPPED;
    }
  }
}

void Animation__free(Animation *self) { free(self->frames); }

typedef struct {
  Vector2 position;
  Animation animation;
} Cursor;

typedef struct {
  Vector2 position;
  Texture2D texture;
} Background;

typedef struct {
  Level level;
  Vector2 gravity;
  Cursor cursor;
  Background backgrounds[BACKGROUND_TEXTURE_COUNT];
} MorteGame;

void MorteGame__free(MorteGame *self) {
  Animation__free(&self->cursor.animation);
}

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
    float delta = GetFrameTime();

    // Update.
    //----------------------------------------------------------------------------------
    game.cursor.position = GetMousePosition();

    Animation__update(&game.cursor.animation, delta);
    //----------------------------------------------------------------------------------

    // Draw.
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(BACKGROUND_COLOR);

    for (size_t i = 0; i < BACKGROUND_TEXTURE_COUNT; i++) {
      DrawTexture(game.backgrounds[i].texture, game.backgrounds[i].position.x,
                  game.backgrounds[i].position.y, WHITE);
    }

    DrawTexture(
        game.cursor.animation.frames[game.cursor.animation.current_frame],
        game.cursor.position.x, game.cursor.position.y, WHITE);

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
      .bounds = {0, 0, LEVEL_WIDTH, WINDOW_HEIGHT},
  };

  game.gravity = (Vector2){0, -700};
}

void load_content() {
  game.cursor =
      (Cursor){.position = {0, 0},
               .animation = {
                   .state = LOOPING,
                   .frame_count = ANIMATION_FRAME_COUNT_CURSOR,
                   .frames = (Texture2D *)malloc(ANIMATION_FRAME_COUNT_CURSOR *
                                                 sizeof(Texture2D)),
                   .current_frame = 0,
                   .length_millis = ANIMATION_LENGTH_MILLIS_CURSOR,
                   .elapsed_millis = 0,
                   .timing = LINEAR,
               }};

  for (size_t i = 0; i < game.cursor.animation.frame_count; i++) {
    char *filename = (char *)malloc((25 + 2 + 4 + 1) * sizeof(char));
    sprintf(filename, "content/kursori/kursori00%02d.png", (int)i + 1);
    game.cursor.animation.frames[i] = LoadTexture(filename);
    free(filename);
  }

  game.backgrounds[0] =
      (Background){.texture = LoadTexture("content/tausta/tausta-0.jpg"),
                   .position = {0, 0}};
  game.backgrounds[1] =
      (Background){.texture = LoadTexture("content/tausta/tausta-1.png"),
                   .position = {0, 0}};
  Texture background2 = LoadTexture("content/tausta/edusta.png");
  game.backgrounds[2] = (Background){
      .texture = background2,
      .position = {0, game.level.bounds.height - background2.height}};
}
