/*******************************************************************************************
 * Morte mysteria
 ********************************************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib/raylib.h"
#include "raylib/raymath.h"

#define ASPECT_RATIO 1.75 // 7/4 ratio
#define VIEW_HEIGHT 400
#define VIEW_WIDTH (VIEW_HEIGHT * ASPECT_RATIO)
#define LEVEL_HEIGHT 400
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

void Animation__free(Animation *self) {
  for (size_t i = 0; i < self->frame_count; i++) {
    UnloadTexture(self->frames[i]);
  }

  free(self->frames);
}

typedef struct {
  Vector2 position;
  Animation animation;
} Cursor;

typedef struct {
  Vector2 offset;
  Texture2D texture;
} Background;

typedef struct {
  Vector2 *origin;
  Vector2 offset;
} ChildObject;

typedef struct {
  Vector2 position;
  Texture2D texture;
  Texture2D eye_texture;
} Priest;

float degrees_between(Vector2 from, Vector2 to) {
  float radians = atan2(to.y - from.y, to.x - from.x);
  return radians * (180.0f / PI);
}

void Priest__draw_eye(Priest *self, Camera2D camera, Cursor cursor,
                      bool left_side) {
  // Eye in own coordinates.
  Vector2 independent_eye_pos = {self->eye_texture.width / 2,
                                 self->eye_texture.height / 2};
  // Eye in Priest coordinates.
  Vector2 relative_eye_pos = {
      self->texture.width / 2 +
          (left_side ? -self->texture.width * 0.15 : 1.0f),
      self->texture.height * 0.06};

  // Eye in world coordinates.
  Vector2 absolute_eye_pos = Vector2Add(
      Vector2Add(independent_eye_pos, relative_eye_pos), self->position);

  // Rotate the eyes to look at the cursor.
  DrawTexturePro(
      self->eye_texture,
      (Rectangle){0, 0, self->eye_texture.width, self->eye_texture.height},
      (Rectangle){absolute_eye_pos.x, absolute_eye_pos.y,
                  self->eye_texture.width, self->eye_texture.height},
      independent_eye_pos, degrees_between(absolute_eye_pos, cursor.position),
      WHITE);
}

void Priest__draw(Priest *self, Camera2D camera, Cursor cursor) {
  DrawTexture(self->texture, self->position.x, self->position.y, WHITE);

  Priest__draw_eye(self, camera, cursor, true);
  Priest__draw_eye(self, camera, cursor, false);
}

typedef struct {
  Texture2D border;
} HUD;

typedef struct {
  Level level;
  Vector2 gravity;
  Cursor cursor;
  Camera2D camera;
  Priest player;
  Background backgrounds[BACKGROUND_TEXTURE_COUNT];
  HUD hud;
} MorteGame;

void MorteGame__free(MorteGame *self) {
  Animation__free(&self->cursor.animation);

  UnloadTexture(self->player.texture);
  UnloadTexture(self->player.eye_texture);

  for (size_t i = 0; i < BACKGROUND_TEXTURE_COUNT; i++) {
    UnloadTexture(self->backgrounds[i].texture);
  }

  UnloadTexture(self->hud.border);
}

void initialize_level(MorteGame *game) {
  game->level = (Level){
      .tag = GROUND,
      .bounds = {0, 0, LEVEL_WIDTH, LEVEL_HEIGHT},
  };

  game->gravity = (Vector2){0, -700};
}

void load_content(MorteGame *game) {
  game->cursor = (Cursor){
      .position = {0, 0},
      .animation = {
          .state = LOOPING,
          .frame_count = ANIMATION_FRAME_COUNT_CURSOR,
          .frames = malloc(ANIMATION_FRAME_COUNT_CURSOR * sizeof(Texture2D)),
          .current_frame = 0,
          .length_millis = ANIMATION_LENGTH_MILLIS_CURSOR,
          .elapsed_millis = 0,
          .timing = LINEAR,
      }};

  for (size_t i = 0; i < game->cursor.animation.frame_count; i++) {
    char *filename = malloc((25 + 2 + 4 + 1) * sizeof(char));
    sprintf(filename, "content/kursori/kursori00%02d.png", (int)i + 1);
    game->cursor.animation.frames[i] = LoadTexture(filename);
    free(filename);
  }

  Texture2D player_texture = LoadTexture("content/uggies/pappi.png");
  Texture2D eye_texture = LoadTexture("content/silma.png");
  game->player = (Priest){
      .position =
          {
              game->level.bounds.width / 2,
              game->level.bounds.height - player_texture.height,
          },
      .texture = player_texture,
      .eye_texture = eye_texture,
  };
  game->camera = (Camera2D){
      .target = {game->player.position.x + player_texture.width / 2,
                 game->player.position.y + player_texture.height / 2},
      .offset = {VIEW_WIDTH / 2, VIEW_HEIGHT - player_texture.height / 2},
      .rotation = 0.0f,
      .zoom = 1.0f,
  };

  game->hud = (HUD){.border = LoadTexture("content/border.png")};

  Texture2D background0 = LoadTexture("content/tausta/tausta-0.jpg");
  game->backgrounds[0] = (Background){
      .texture = background0,
      .offset = {(game->level.bounds.width - background0.width) / 2, 0}};
  Texture2D background1 = LoadTexture("content/tausta/tausta-1.png");
  game->backgrounds[1] = (Background){
      .texture = background1,
      .offset = {(game->level.bounds.width - background1.width) / 2, 0}};
  Texture2D background2 = LoadTexture("content/tausta/edusta.png");
  game->backgrounds[2] = (Background){
      .texture = background2,
      .offset = {(game->level.bounds.width - background2.width) / 2,
                 game->level.bounds.height - background2.height}};
}

int main(void) {
  float window_scale = 2.0f;

  InitWindow(VIEW_WIDTH * window_scale, VIEW_HEIGHT * window_scale,
             "Morte mysteria");

  SetTargetFPS(60);
  DisableCursor();

  MorteGame game;

  initialize_level(&game);

  load_content(&game);

  while (!WindowShouldClose()) {
    float delta = GetFrameTime();

    // Update.
    //----------------------------------------------------------------------------------
    game.cursor.position = GetScreenToWorld2D(GetMousePosition(), game.camera);

    Animation__update(&game.cursor.animation, delta);

    if (IsKeyDown(KEY_D)) {
      game.player.position.x += 2;
      game.backgrounds[0].offset.x += 1.58f;
      game.backgrounds[1].offset.x += 1.2f;
    }
    if (IsKeyDown(KEY_A)) {
      game.player.position.x -= 2;
      game.backgrounds[0].offset.x -= 1.58f;
      game.backgrounds[1].offset.x -= 1.2f;
    }

    if (IsKeyDown(KEY_J)) {
      window_scale -= 0.05;
      window_scale = Clamp(window_scale, 1.0, 2.0);
      SetWindowSize(VIEW_WIDTH * window_scale, VIEW_HEIGHT * window_scale);
    }
    if (IsKeyDown(KEY_K)) {
      window_scale += 0.05;
      window_scale = Clamp(window_scale, 1.0, 2.0);
      SetWindowSize(VIEW_WIDTH * window_scale, VIEW_HEIGHT * window_scale);
    }
    game.camera.zoom = window_scale;
    game.camera.offset =
        Vector2Scale((Vector2){(VIEW_WIDTH / 2),
                               (VIEW_HEIGHT - game.player.texture.height / 2)},
                     window_scale);

    game.camera.target =
        (Vector2){game.player.position.x + game.player.texture.width / 2,
                  game.player.position.y + game.player.texture.height / 2};

    //----------------------------------------------------------------------------------

    // Draw.
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(BACKGROUND_COLOR);

    BeginMode2D(game.camera);

    for (size_t i = 0; i < 2; i++) {
      DrawTexture(game.backgrounds[i].texture, game.backgrounds[i].offset.x,
                  game.backgrounds[i].offset.y, WHITE);
    }

    Priest__draw(&game.player, game.camera, game.cursor);

    DrawTexture(game.backgrounds[2].texture, game.backgrounds[2].offset.x,
                game.backgrounds[2].offset.y, WHITE);

    DrawTexture(game.hud.border,
                game.camera.target.x - game.camera.offset.x / 2,
                game.camera.target.y - game.camera.offset.y / 2, WHITE);

    DrawTexture(
        game.cursor.animation.frames[game.cursor.animation.current_frame],
        game.cursor.position.x, game.cursor.position.y, WHITE);

    EndMode2D();

    EndDrawing();

    //----------------------------------------------------------------------------------
  }

  // De-Initialization.
  //--------------------------------------------------------------------------------------
  // XXX: NOTE That textures need to be unloaded before closing window.
  MorteGame__free(&game);

  CloseWindow();
  //--------------------------------------------------------------------------------------

  return 0;
}
