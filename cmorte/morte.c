/*******************************************************************************************
 * Morte mysteria
 ********************************************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib/raylib.h"
#include "raylib/raymath.h"

#define UP (Vector2){0, -1}
#define E 0.00001f
/* 7/4 ratio */
#define ASPECT_RATIO 1.75
/* View[port] height is used to scale the visual size */
#define WINDOW_HEIGHT 800
#define WINDOW_WIDTH (WINDOW_HEIGHT * ASPECT_RATIO)
#define LEVEL_HEIGHT 400
#define LEVEL_WIDTH 2200
#define BACKGROUND_COLOR (Color){133, 31, 10, 255}
#define BACKGROUND_TEXTURE_COUNT 3

#define ANIMATION_FRAME_COUNT_CURSOR 19
#define ANIMATION_LENGTH_MILLIS_CURSOR 750

// DEBUG
// -----------------------------------------------------------------------------
bool is_debug_mode = true;

void debug__draw_point(float x, float y, Color color) {
  int size = 12;
  DrawRectangle(x - size / 2, y - size / 2, size, size, color);
}

void debug__draw_line(float x, float y, float dirx, float diry, Color color) {
  float length = 20.0f;
  Vector2 end_pos = (Vector2){x + dirx * length, y + diry * length};
  DrawLineEx((Vector2){x, y}, end_pos, 3.0, color);
  debug__draw_point(end_pos.x, end_pos.y, BLACK);
}
// -----------------------------------------------------------------------------

bool float__eq(float a, float b) { return b - E < a && a < b + E; }

bool float__is_positive(float a) { return !float__eq(a, 0) && -E < a; }

bool Vector2__eq(Vector2 a, Vector2 b) {
  return float__eq(a.x, b.x) && float__eq(a.y, b.y);
}

enum EnemyTag { SNAKE = 0, WACKO, GULL, HAND };
enum PropTag { GROUND };

typedef struct {
  // This replaces using NULL as a flag if there was no collision.
  bool happened;
  float depth;
  Vector2 normal;
} Collision;

enum PhysicsType { STATIC, KINETIC };

typedef struct {
  enum PhysicsType type;
  Rectangle aabb;
  float mass;
  float inverse_mass;
  Vector2 velocity;
  Vector2 force;
  bool is_grounded;
} PhysicsBody;

void debug__draw_body(PhysicsBody body, Color color) {
  DrawRectangleLinesEx(body.aabb, 3, color);
  DrawRectangleRec(body.aabb, GetColor(ColorToInt(color) & 0xff00ff77));
}

PhysicsBody *PhysicsBody__new(enum PhysicsType type, Rectangle aabb,
                              float mass) {
  PhysicsBody *self = malloc(sizeof(PhysicsBody));
  self->type = type;
  self->aabb = aabb;
  self->mass = mass;
  self->inverse_mass = 1.0f / mass;
  self->velocity = (Vector2){0, 0};
  self->force = (Vector2){0, 0};
  return self;
}

/* Return if and how the bodies collide.
 *
 * ## Kudos:
 * -
 * https://gamedevelopment.tutsplus.com/tutorials/how-to-create-a-custom-2d-physics-engine-the-basics-and-impulse-resolution--gamedev-6331
 * - https://textbooks.cs.ksu.edu/cis580/04-collisions/index.html
 */
Collision PhysicsBody__colliding(PhysicsBody *self, PhysicsBody other) {
  Vector2 self_half =
      Vector2Scale((Vector2){self->aabb.width, self->aabb.height}, 0.5f);
  Vector2 other_half =
      Vector2Scale((Vector2){other.aabb.width, other.aabb.height}, 0.5f);
  Vector2 segment = {
      other.aabb.x + other_half.x - (self->aabb.x + self_half.x),
      other.aabb.y + other_half.y - (self->aabb.y + self_half.y),
  };

  float x_overlap = self_half.x + other_half.x - fabs(segment.x);
  float y_overlap = self_half.y + other_half.y - fabs(segment.y);

  Collision collision = {.happened = false};
  if (float__is_positive(x_overlap) && float__is_positive(y_overlap)) {
    collision.happened = true;
    if (x_overlap < y_overlap) {
      collision.depth = x_overlap;
      collision.normal = (Vector2){segment.x > 0 ? 1 : -1, 0};
    } else {
      collision.depth = y_overlap;
      collision.normal = (Vector2){0, segment.y > 0 ? 1 : -1};
    }
  }
  return collision;
}

/*
 * NOTE: This method assumes the bodies `a` and `b` are not the same body AND
 * that their mass is non-zero.
 */
void Collision__resolve(Collision *self, PhysicsBody *a, PhysicsBody *b) {
  if (a->type == STATIC && b->type == STATIC) {
    // Do nothing as both bodies should always remain stationary.
    return;
  }

  if (a->type == STATIC) {
    b->aabb.x += self->normal.x * self->depth;
    b->aabb.y += self->normal.y * self->depth;
  } else if (b->type == STATIC) {
    a->aabb.x += self->normal.x * self->depth;
    a->aabb.y += self->normal.y * self->depth;
  } else /* a->type == KINETIC && b->type == KINETIC */ {
    a->aabb.x += self->normal.x * self->depth * b->mass / a->mass;
    a->aabb.y += self->normal.y * self->depth * b->mass / a->mass;
    b->aabb.x += self->normal.x * self->depth * a->mass / b->mass;
    b->aabb.y += self->normal.y * self->depth * a->mass / b->mass;
  }
}

typedef struct {
  PhysicsBody *body;
  Texture2D texture;
} RenderBody;

RenderBody *RenderBody__new(PhysicsBody *body, Texture2D texture) {
  RenderBody *self = malloc(sizeof(RenderBody));
  self->body = body;
  self->texture = texture;
  return self;
}

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
  enum PropTag tag;
  Rectangle bounds;
  PhysicsBody *body;
} Level;

typedef struct {
  Vector2 position;
  Animation animation;
} Cursor;

typedef struct {
  Vector2 position;
  Texture2D texture;
} Background;

typedef struct {
  Vector2 *origin;
  Vector2 offset;
} ChildObject;

typedef struct {
  Texture2D texture;
  Texture2D eye_texture;
  PhysicsBody *body;
} Priest;

typedef struct {
  enum EnemyTag tag;
  Texture2D texture;
  PhysicsBody *body;
} Gull;

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
  Vector2 absolute_eye_pos =
      Vector2Add(Vector2Add(independent_eye_pos, relative_eye_pos),
                 (Vector2){self->body->aabb.x, self->body->aabb.y});

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
  DrawTexture(self->texture, self->body->aabb.x, self->body->aabb.y, WHITE);

  Priest__draw_eye(self, camera, cursor, true);
  Priest__draw_eye(self, camera, cursor, false);
}

typedef struct {
  Texture2D border;
} HUD;

typedef struct {
  Level level;
  Vector2 view_size;
  float gravity;
  Cursor cursor;
  Camera2D camera;
  Priest player;
  size_t physics_body_count;
  size_t render_body_count;
  PhysicsBody **physics_bodies;
  RenderBody **render_bodies;
  Background backgrounds[BACKGROUND_TEXTURE_COUNT];
  HUD hud;
} MorteGame;

void MorteGame__free(MorteGame *self) {
  Animation__free(&self->cursor.animation);

  UnloadTexture(self->player.texture);
  UnloadTexture(self->player.eye_texture);

  for (size_t i = 0; i < self->physics_body_count; i++) {
    free(self->physics_bodies[i]);
  }
  free(self->physics_bodies);

  for (size_t i = 0; i < self->render_body_count; i++) {
    UnloadTexture(self->render_bodies[i]->texture);
    free(self->render_bodies[i]);
  }
  free(self->render_bodies);

  for (size_t i = 0; i < BACKGROUND_TEXTURE_COUNT; i++) {
    UnloadTexture(self->backgrounds[i].texture);
  }

  UnloadTexture(self->hud.border);
}

void MorteGame__add_render_body(MorteGame *self, RenderBody *body) {
  self->render_bodies = realloc(
      self->render_bodies, (self->render_body_count + 1) * sizeof(RenderBody));
  self->render_bodies[self->render_body_count] = body;
  self->render_body_count += 1;
}

void MorteGame__add_physics_body(MorteGame *self, PhysicsBody *body) {
  self->physics_bodies =
      realloc(self->physics_bodies,
              (self->physics_body_count + 1) * sizeof(PhysicsBody));
  self->physics_bodies[self->physics_body_count] = body;
  self->physics_body_count += 1;
}

/*
 * ## Kudos:
 * -
 * https://www.raylib.com/examples/core/loader.html?name=core_2d_camera_platformer
 */
void MorteGame__update_physics(MorteGame *self, float delta) {
  // Apply forces.
  for (size_t i = 0; i < self->physics_body_count; i++) {
    PhysicsBody *body = self->physics_bodies[i];
    if (body->type != KINETIC) {
      continue;
    }

    if (!body->is_grounded) {
      body->force.y += self->gravity * delta;
      body->velocity.x *= 0.95;
    } else {
      body->velocity.x *= 0.6;
    }

    body->velocity.x += body->force.x;
    body->velocity.y += body->force.y;

    body->aabb.x += body->velocity.x * delta;
    body->aabb.y += body->velocity.y * delta;

    // This seems to make the jump ramp nicely on the fall.
    body->force.y = body->is_grounded ? 0 : self->gravity * delta;
  }

  // Check collisions.
  for (size_t i = 0; i < self->physics_body_count; i++) {
    PhysicsBody *body = self->physics_bodies[i];

    for (size_t j = 0; j < self->physics_body_count; j++) {
      if (i == j) {
        continue;
      }
      PhysicsBody *other_body = self->physics_bodies[j];
      Collision collision = PhysicsBody__colliding(body, *other_body);
      if (collision.happened) {
        if (is_debug_mode) {
          BeginDrawing();

          BeginMode2D(self->camera);
          debug__draw_body(*body, YELLOW);
          debug__draw_body(*other_body, YELLOW);
          debug__draw_line(body->aabb.x, body->aabb.y, collision.normal.x,
                           collision.normal.y, YELLOW);
          EndMode2D();
          EndDrawing();
        }

        // Resolve collisions.
        Collision__resolve(&collision, body, other_body);

        // Check for game specific resolutions.
        if (Vector2__eq(collision.normal, UP)) {
          if (body->type == KINETIC && other_body->type == STATIC) {
            body->is_grounded = true;
            other_body->velocity.y = 0;
          } else if (other_body->type == KINETIC && body->type == STATIC) {
            other_body->is_grounded = true;
            other_body->velocity.y = 0;
          }
        }
      }
    }
  }
}

/*
 * While keeping the view inside the level bounds, focus camera's center on
 * the `object` center.
 */
void MorteGame__focus_view_on(MorteGame *self, Rectangle object) {
  self->camera.target = (Vector2){object.x, object.y};
  self->camera.offset = Vector2Scale(self->view_size, 0.5);
}

void initialize_level(MorteGame *game) {
  Rectangle level_bounds = {0, 0, LEVEL_WIDTH, LEVEL_HEIGHT};
  game->level = (Level){
      .tag = GROUND,
      .bounds = level_bounds,
      .body = PhysicsBody__new(
          STATIC,
          (Rectangle){-level_bounds.width / 2, level_bounds.height,
                      level_bounds.width, level_bounds.height},
          0),
  };

  MorteGame__add_physics_body(game, game->level.body);

  game->gravity = 400;
}

void MorteGame__spawn_priest(MorteGame *self) {
  Texture2D player_texture = LoadTexture("content/uggies/pappi.png");
  Texture2D eye_texture = LoadTexture("content/silma.png");
  self->player = (Priest){
      .texture = player_texture,
      .eye_texture = eye_texture,
      .body = PhysicsBody__new(
          KINETIC,
          (Rectangle){-self->level.bounds.width / 2 + player_texture.width,
                      self->level.bounds.height - player_texture.height,
                      player_texture.width, player_texture.height},
          100),
  };
  MorteGame__add_physics_body(self, self->player.body);
}

void MorteGame__spawn_enemy(MorteGame *self, enum EnemyTag type) {
  switch (type) {
  case SNAKE:
    break;
  case WACKO:
    break;
  case GULL:
    Texture2D gull_texture = LoadTexture("content/uggies/gull/lokki0001.png");
    Gull gull = {.tag = GULL,
                 .texture = gull_texture,
                 .body = PhysicsBody__new(
                     KINETIC,
                     (Rectangle){self->player.body->aabb.x + 50,
                                 self->player.body->aabb.y - 50,
                                 gull_texture.width, gull_texture.height},
                     20)};
    MorteGame__add_physics_body(self, gull.body);
    MorteGame__add_render_body(self, RenderBody__new(gull.body, gull.texture));
    break;
  case HAND:
    break;
  }
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

  MorteGame__spawn_priest(game);

  game->hud = (HUD){.border = LoadTexture("content/border.png")};

  Texture2D background0 = LoadTexture("content/tausta/tausta-0.jpg");
  game->backgrounds[0] = (Background){.texture = background0,
                                      .position = {-background0.width / 2, 0}};
  Texture2D background1 = LoadTexture("content/tausta/tausta-1.png");
  game->backgrounds[1] = (Background){.texture = background1,
                                      .position = {-background1.width / 2, 0}};
  Texture2D background2 = LoadTexture("content/tausta/edusta.png");
  game->backgrounds[2] = (Background){
      .texture = background2,
      .position = {-background2.width / 2,
                   game->level.bounds.height - background2.height}};
}

int main(void) {

  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
             "Morte Mysteria dom Domine dem Daemonium");

  SetTargetFPS(60);
  DisableCursor();

  MorteGame game = {0};
  game.camera = (Camera2D){0};
  game.camera.zoom = WINDOW_HEIGHT / LEVEL_HEIGHT;
  game.view_size = (Vector2){WINDOW_WIDTH, WINDOW_HEIGHT};

  initialize_level(&game);

  load_content(&game);

  while (!WindowShouldClose()) {
    float delta = GetFrameTime();

    // Update.
    // -------------------------------------------------------------------------
    MorteGame__update_physics(&game, delta);

    game.cursor.position = GetScreenToWorld2D(GetMousePosition(), game.camera);

    Animation__update(&game.cursor.animation, delta);

    MorteGame__focus_view_on(&game, game.player.body->aabb);

    if (is_debug_mode) {
      game.camera.zoom += ((float)GetMouseWheelMove() * 0.05f);
      float target_relative_offset_x =
          game.camera.target.x / game.level.bounds.width;

      // Enemy spawn control.
      for (size_t i = SNAKE; i < HAND; i++) {
        if (IsKeyPressed(KEY_ONE + i)) {
          MorteGame__spawn_enemy(&game, i);
        }
      }
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_P)) {
      is_debug_mode = !is_debug_mode;
    }

    // Horizontal movement control.
    if (IsKeyDown(KEY_D)) {
      game.player.body->force.x =
          50.0f * (game.player.body->is_grounded ? 1.0f : 0.1f);
    } else if (IsKeyDown(KEY_A)) {
      game.player.body->force.x =
          -50.0f * (game.player.body->is_grounded ? 1.0f : 0.1f);
    } else {
      game.player.body->force.x = 0;
    }

    // Vertical movement control.
    if (IsKeyDown(KEY_SPACE) && game.player.body->is_grounded) {
      game.player.body->force.y -= 350;
      game.player.body->is_grounded = false;
    }

    // Update parallax according to updated camera target position.
    for (size_t i = 0; i < 2; i++) {
      Background background = game.backgrounds[i];
      background.position.x = 0;
    }
    // -------------------------------------------------------------------------

    // Draw.
    // -------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(BACKGROUND_COLOR);

    BeginMode2D(game.camera);

    for (size_t i = 0; i < 2; i++) {
      DrawTexture(game.backgrounds[i].texture, game.backgrounds[i].position.x,
                  game.backgrounds[i].position.y, WHITE);
    }

    Priest__draw(&game.player, game.camera, game.cursor);

    for (size_t i = 0; i < game.render_body_count; i++) {
      RenderBody *body = game.render_bodies[i];
      DrawTexture(body->texture, body->body->aabb.x, body->body->aabb.y, WHITE);
    }

    for (size_t i = 2; i < 4; i++) {
      DrawTexture(game.backgrounds[i].texture, game.backgrounds[i].position.x,
                  game.backgrounds[i].position.y, WHITE);
    }

    DrawTexture(game.hud.border,
                game.camera.target.x - game.hud.border.width / 2,
                game.camera.target.y - game.hud.border.height / 2, WHITE);

    DrawTexture(
        game.cursor.animation.frames[game.cursor.animation.current_frame],
        game.cursor.position.x, game.cursor.position.y, WHITE);

    if (is_debug_mode) {
      for (size_t i = 0; i < game.physics_body_count; i++) {
        debug__draw_body(*game.physics_bodies[i], MAGENTA);
      }

      debug__draw_point(game.camera.target.x, game.camera.target.y, GREEN);
      debug__draw_point(game.camera.target.x - game.camera.offset.x,
                        game.camera.target.y - game.camera.offset.y, SKYBLUE);
      debug__draw_point(0, 0, WHITE);
    }

    EndMode2D();

    if (is_debug_mode) {
      DrawText("DEBUG", 45, 35, 50, GREEN);
    }

    EndDrawing();
    // -------------------------------------------------------------------------
  }

  // De-Initialization.
  // ---------------------------------------------------------------------------
  // XXX: NOTE That textures need to be unloaded before closing window.
  MorteGame__free(&game);

  CloseWindow();
  // ---------------------------------------------------------------------------

  return 0;
}
