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
#define LEVEL_WIDTH 2800
#define BACKGROUND_COLOR (Color){133, 31, 10, 255}

#define ANIMATION_FRAME_COUNT_CURSOR 19
#define ANIMATION_LENGTH_MILLIS_CURSOR 750
#define ANIMATION_FRAME_COUNT_GULL 19
#define ANIMATION_LENGTH_MILLIS_GULL 1000

// DEBUG
// -----------------------------------------------------------------------------
enum DEBUG_visual_type {
  DOT,
  RECTANGLE,
  ARROW,
};

typedef struct {
  enum DEBUG_visual_type type;
  Color color;
  Vector2 position;
  Vector2 size;
  bool is_bordered;
  Vector2 end_position;
} DEBUG_visual;

typedef struct {
  size_t draw_queue_length;
  DEBUG_visual *draw_queue;
} DEBUG;

void DEBUG__enqueue(DEBUG *self, DEBUG_visual object) {
  self->draw_queue = realloc(self->draw_queue, (self->draw_queue_length + 1) *
                                                   sizeof(DEBUG_visual));
  self->draw_queue[self->draw_queue_length] = object;
  self->draw_queue_length += 1;
}

void DEBUG__draw_point_(DEBUG *self, Vector2 pos, Color color) {
  int size = 12;
  DrawRectangle(pos.x - size / 2, pos.y - size / 2, size, size, color);
}

void DEBUG__draw_point(DEBUG *self, float x, float y, Color color) {
  DEBUG__enqueue(
      self, (DEBUG_visual){.type = DOT, .color = color, .position = {x, y}});
}

void DEBUG__draw_direction(DEBUG *self, float start_x, float start_y,
                           float dir_x, float dir_y, Color color) {
  float length = 66.0f;
  Vector2 end_position =
      (Vector2){start_x + dir_x * length, start_y + dir_y * length};

  DEBUG__enqueue(self, (DEBUG_visual){.type = ARROW,
                                      .color = color,
                                      .position = {start_x, start_y},
                                      .end_position = end_position});
}

void DEBUG__draw_arrow(DEBUG *self, float start_x, float start_y, float end_x,
                       float end_y, Color color) {
  DEBUG__enqueue(self, (DEBUG_visual){.type = ARROW,
                                      .color = color,
                                      .position = {start_x, start_y},
                                      .end_position = {end_x, end_y}});
}

void DEBUG__draw_bordered(DEBUG *self, Rectangle rec, Color color) {
  DEBUG__enqueue(self, (DEBUG_visual){
                           .type = RECTANGLE,
                           .color = color,
                           .position = {rec.x, rec.y},
                           .size = {rec.width, rec.height},
                           .is_bordered = true,
                       });
}

void DEBUG__draw(DEBUG *self) {
  for (size_t i = 0; i < self->draw_queue_length; i++) {
    DEBUG_visual object = self->draw_queue[i];

    switch (object.type) {
    case DOT:
      DEBUG__draw_point_(self, object.position, object.color);
      break;
    case RECTANGLE:
      Color base_color = object.color;
      Rectangle rec = (Rectangle){object.position.x, object.position.y,
                                  object.size.x, object.size.y};
      if (object.is_bordered) {
        // Make it so that the border sticks out of the shape a bit so that it
        // can be seen even if right at the edge of the view frame.
        rec.x -= 2.5f;
        rec.y -= 2.5f;
        rec.width += 5.0f;
        rec.height += 5.0f;
        DrawRectangleLinesEx(rec, 5, object.color);
        base_color = GetColor(ColorToInt(base_color) & 0xff00ff77);
      }
      DrawRectangleRec(rec, base_color);
      break;
    case ARROW:
      float size = 6.0f;
      DrawLineEx(object.position, object.end_position, size, object.color);
      /* The arrow tip's edge on the left side of the direction line.
       *     \
       *  -----
       *
       */
      float edge_length = 15.0f;
      Vector2 segment = Vector2Subtract(object.end_position, object.position);
      float arrow_angle = atan2(segment.y, segment.x);
      float l = arrow_angle + PI / 4.0f;
      DrawLineEx(object.end_position,
                 (Vector2){
                     object.end_position.x - cos(l) * edge_length,
                     object.end_position.y - sin(l) * edge_length,
                 },
                 size / 2, object.color);
      /* Then the right side.
       *    \
       * -----
       *    /
       */
      float r = arrow_angle - PI / 4.0f;
      DrawLineEx(object.end_position,
                 (Vector2){
                     object.end_position.x - cos(r) * edge_length,
                     object.end_position.y - sin(r) * edge_length,
                 },
                 size / 2, object.color);
      break;
    }
  }
}
// -----------------------------------------------------------------------------

float frand() { return (float)rand() / (float)RAND_MAX; }

bool float__eq(float a, float b) { return b - E < a && a < b + E; }

bool float__is_positive(float a) { return !float__eq(a, 0) && -E < a; }

bool Vector2__eq(Vector2 a, Vector2 b) {
  return float__eq(a.x, b.x) && float__eq(a.y, b.y);
}

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

/*
 * Return if and how the bodies collide.
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
  Vector2 distance = {
      other.aabb.x + other_half.x - (self->aabb.x + self_half.x),
      other.aabb.y + other_half.y - (self->aabb.y + self_half.y),
  };

  Vector2 overlap = {self_half.x + other_half.x - fabs(distance.x),
                     self_half.y + other_half.y - fabs(distance.y)};

  Collision collision = {.happened = false};
  if (float__is_positive(overlap.x) && float__is_positive(overlap.y)) {
    collision.happened = true;

    if (overlap.x < overlap.y) {
      collision.depth = overlap.x;
      collision.normal = (Vector2){distance.x > 0 ? 1 : -1, 0};
    } else {
      collision.depth = overlap.y;
      collision.normal = (Vector2){0, distance.y > 0 ? 1 : -1};
    }
  }

  return collision;
}

enum AnimationState { STOPPED, PLAYING_ONCE, LOOPING };

enum AnimationTiming { LINEAR, EASE_IN, EASE_OUT };

typedef struct {
  enum AnimationState state;
  size_t frame_count;
  size_t current_frame;
  // Duration of the whole animation in milliseconds.
  unsigned length_ms;
  unsigned elapsed_ms;
  enum AnimationTiming timing;
  Texture2D *frames;
} Animation;

/*
 * Copy `frames` for animation.
 */
Animation Animation__from_frames(size_t frame_count, Texture2D *frames,
                                 unsigned length_ms) {
  Animation self = {
      .state = STOPPED,
      .frame_count = frame_count,
      .frames = malloc(frame_count * sizeof(Texture2D)),
      .current_frame = 0,
      .length_ms = length_ms,
      .elapsed_ms = 0,
      .timing = LINEAR,
  };

  for (size_t i = 0; i < self.frame_count; i++) {
    self.frames[i] = frames[i];
  }

  return self;
}
/*
 * Initialize and load animation frames from a string template filepath using
 * zero-left-padded indexes [0, `frame_count`).
 *
 * NOTE: This method assumes using the `template` will yield constant length
 * strings (as per the padding condition).
 *
 * NOTE: The maximum path length cannot exceed 500 ASCII-characters.
 */
Animation Animation__from_path_template(const char *template,
                                        size_t frame_count,
                                        unsigned length_ms) {
  Texture2D frames[frame_count];
  // Limit path length to 500 characters.
  char filename[501];
  for (size_t i = 0; i < frame_count; i++) {
    sprintf(filename, template, (int)i + 1);
    frames[i] = LoadTexture(filename);
  }
  return Animation__from_frames(frame_count, frames, length_ms);
}

void Animation__update(Animation *self, float delta) {
  if (self->state == STOPPED) {
    return;
  }

  self->elapsed_ms += 1000 * delta;
  float t = fmin(1.0f, (float)self->elapsed_ms / (float)self->length_ms);

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
    self->elapsed_ms = 0;

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

enum UggyType { PRIEST = 0, SNAKE, WACKO, GULL, HAND };

/*
 * Represents objects/characters animated in the game world.
 */
typedef struct {
  enum UggyType type;
  PhysicsBody *body;
  Animation animation;
  // For PRIEST type.
  Texture2D eye_texture;
} Uggy;

Uggy *Uggy__new(enum UggyType type, PhysicsBody *body, Animation animation) {
  Uggy *self = malloc(sizeof(Uggy));
  self->type = type;
  self->body = body;
  self->animation = animation;
  return self;
}

enum PropType { LOOT };

/*
 * Represents objects/items stationary in the game world.
 */
typedef struct {
  enum PropType type;
  PhysicsBody *body;
  Animation animation;
} Prop;

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

float degrees_between(Vector2 from, Vector2 to) {
  float radians = atan2(to.y - from.y, to.x - from.x);
  return radians * (180.0f / PI);
}

void Uggy__draw_priest_eye(Uggy *self, Camera2D camera, Cursor cursor,
                           bool left_side) {
  // Eye in own coordinates.
  Vector2 independent_eye_pos = {self->eye_texture.width / 2,
                                 self->eye_texture.height / 2};
  // Eye in Priest coordinates.
  Vector2 relative_eye_pos = {
      self->animation.frames[0].width / 2 +
          (left_side ? -self->animation.frames[0].width * 0.15 : 1.0f),
      self->animation.frames[0].height * 0.06};

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

void Uggy__draw(Uggy *self, Camera2D camera, Cursor cursor) {
  DrawTexture(self->animation.frames[self->animation.current_frame],
              self->body->aabb.x, self->body->aabb.y, WHITE);

  switch (self->type) {
  case PRIEST:
    Uggy__draw_priest_eye(self, camera, cursor, true);
    Uggy__draw_priest_eye(self, camera, cursor, false);
    break;
  case SNAKE:
    break;
  case WACKO:
    break;
  case GULL:
    break;
  case HAND:
    break;
  }
}

typedef struct {
  Texture2D border;
} HUD;

typedef struct {
  float player_walk_speed;
  float player_jump_speed;
} MorteGameConstants;

const MorteGameConstants DEFAULT_MORTE_GAME_CONSTANTS = {
    .player_walk_speed = 50.0f,
    .player_jump_speed = -350.0f,
};

typedef struct {
  bool is_paused;
  MorteGameConstants constants;
  // Bounds consisting of ground, "ceiling" and two vertical walls.
  PhysicsBody *level_bounds[4];
  Vector2 view_size;
  float gravity;
  Cursor cursor;
  Camera2D camera;
  // Convenience handle to the player Uggy.
  Uggy *player;
  size_t physics_body_count;
  size_t uggy_count;
  PhysicsBody **physics_bodies;
  Uggy **uggies;
  Background backgrounds[3];
  HUD hud;
  DEBUG *debug;
} MorteGame;

void MorteGame__free(MorteGame *self) {
  Animation__free(&self->cursor.animation);

  for (size_t i = 0; i < self->physics_body_count; i++) {
    free(self->physics_bodies[i]);
  }
  free(self->physics_bodies);

  UnloadTexture(self->player->eye_texture);

  for (size_t i = 0; i < self->uggy_count; i++) {
    Animation__free(&self->uggies[i]->animation);
    free(self->uggies[i]);
  }
  free(self->uggies);

  for (size_t i = 0; i < 3; i++) {
    UnloadTexture(self->backgrounds[i].texture);
  }

  UnloadTexture(self->hud.border);
}

void MorteGame__add_physics_body(MorteGame *self, PhysicsBody *body) {
  self->physics_bodies =
      realloc(self->physics_bodies,
              (self->physics_body_count + 1) * sizeof(PhysicsBody));
  self->physics_bodies[self->physics_body_count] = body;
  self->physics_body_count += 1;
}

void MorteGame__add_uggy(MorteGame *self, Uggy *uggy) {
  self->uggies = realloc(self->uggies, (self->uggy_count + 1) * sizeof(Uggy));
  self->uggies[self->uggy_count] = uggy;
  self->uggy_count += 1;
  MorteGame__add_physics_body(self, uggy->body);
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
    PhysicsBody *a = self->physics_bodies[i];

    for (size_t j = 0; j < self->physics_body_count; j++) {
      if (i == j) {
        continue;
      }
      PhysicsBody *b = self->physics_bodies[j];

      Collision collision = PhysicsBody__colliding(a, *b);
      if (collision.happened) {

        if (self->debug) {
          DEBUG__draw_bordered(self->debug, a->aabb, YELLOW);
          DEBUG__draw_bordered(self->debug, b->aabb, YELLOW);
          DEBUG__draw_direction(self->debug, b->aabb.x + b->aabb.width / 2,
                                b->aabb.y + b->aabb.height / 2,
                                collision.normal.x, collision.normal.y, YELLOW);
        }

        // Resolve collisions physics i.e., how the bodies change position,
        // force etc. resulting from collision.

        if (a->type == STATIC && b->type == STATIC) {
          // Do nothing as both bodies should always remain stationary.
          return;
        }

        // Static bodies directly change the other body's position. Otherwise
        // the bodies alter each others' forces (NOTE to apply delta at caller)
        // in order to push away "softer".
        if (a->type == STATIC) {
          b->aabb.x += collision.normal.x * collision.depth;
          b->aabb.y += collision.normal.y * collision.depth;
        } else if (b->type == STATIC) {
          a->aabb.x += collision.normal.x * collision.depth;
          a->aabb.y += collision.normal.y * collision.depth;
        } else /* a->type == KINETIC && b->type == KINETIC */ {
          float a_over_b = Clamp(a->mass * b->inverse_mass, E, 1.0);
          float b_over_a = Clamp(b->mass * a->inverse_mass, E, 1.0);
          a->force.x += collision.normal.x * collision.depth * b_over_a;
          a->force.y += collision.normal.y * collision.depth * b_over_a;
          b->force.x += -collision.normal.x * collision.depth * a_over_b;
          b->force.y += -collision.normal.y * collision.depth * a_over_b;
        }
        a->force = Vector2Scale(a->force, delta);
        b->force = Vector2Scale(b->force, delta);

        if (self->debug) {
          self->is_paused = true;
        }

        // Check for game specific resolutions.

        // De-ground bodies that are off the ground.
        if (!float__eq(a->force.y, 0)) {
          a->is_grounded = false;
        }

        // Ground when dropping straight down on a static body.
        if (Vector2__eq(collision.normal, UP)) {
          if (a->type == KINETIC && b->type == STATIC) {
            a->is_grounded = true;
            b->velocity.y = 0;
          } else if (b->type == KINETIC && a->type == STATIC) {
            b->is_grounded = true;
            b->velocity.y = 0;
          }
        }
      }
    }

    if (self->debug) {
      DEBUG__draw_bordered(self->debug, self->physics_bodies[i]->aabb, MAGENTA);
    }
  }
}

/*
 * While keeping the view inside the level bounds, focus camera's center on
 * the `object` center.
 */
void MorteGame__focus_view_on(MorteGame *self, Rectangle object) {
  // Offset the target if moving too close to level edges.
  self->camera.target =
      (Vector2){Clamp(object.x + object.width / 2,
                      -LEVEL_WIDTH / 2 + self->view_size.x / 4,
                      LEVEL_WIDTH / 2 - self->view_size.x / 4),
                fmax(object.y + (object.height - LEVEL_HEIGHT), 0)};

  self->camera.offset = (Vector2){self->view_size.x / 2, 0};

  if (self->debug) {
    DEBUG__draw_point(self->debug, self->camera.target.x, self->camera.target.y,
                      GREEN);
    DEBUG__draw_point(self->debug,
                      self->camera.target.x - self->camera.offset.x,
                      self->camera.target.y - self->camera.offset.y, SKYBLUE);
    DEBUG__draw_point(self->debug, 0, 0, WHITE);
  }
}

void MorteGame__spawn_uggy(MorteGame *self, enum UggyType type) {
  switch (type) {
  case PRIEST:
    if (self->player != NULL) {
      puts("\033[31mAttempted adding player twice\033[0m");
      exit(1);
    }

    // Loading content.
    Texture2D player_texture = LoadTexture("content/uggies/pappi.png");
    Texture2D eye_texture = LoadTexture("content/silma.png");

    // Initialization.
    self->player =
        Uggy__new(PRIEST,
                  PhysicsBody__new(
                      KINETIC,
                      (Rectangle){-LEVEL_WIDTH / 2 + player_texture.width,
                                  LEVEL_HEIGHT - player_texture.height,
                                  player_texture.width, player_texture.height},
                      100),
                  Animation__from_frames(1, &player_texture, 0));

    // Specialization.
    self->player->eye_texture = eye_texture;

    // Adding to sim.
    MorteGame__add_uggy(self, self->player);
    break;
  case SNAKE:
    break;
  case WACKO:
    break;
  case GULL:
    Animation gull_animation = Animation__from_path_template(
        "content/uggies/gull/lokki%04d.png", ANIMATION_FRAME_COUNT_GULL,
        ANIMATION_LENGTH_MILLIS_GULL);
    Uggy *gull =
        Uggy__new(GULL,
                  PhysicsBody__new(KINETIC,
                                   (Rectangle){self->player->body->aabb.x + 50,
                                               self->player->body->aabb.y - 50,
                                               gull_animation.frames[0].width,
                                               gull_animation.frames[0].height},
                                   20),
                  gull_animation);
    gull->animation.state = LOOPING;
    MorteGame__add_uggy(self, gull);
    break;
  case HAND:
    break;
  }
}
MorteGame MorteGame__initialize() {
  MorteGame game = {0};

  game.camera = (Camera2D){0};
  float window_scale = WINDOW_HEIGHT / LEVEL_HEIGHT;
  game.camera.zoom = window_scale;
  game.view_size = (Vector2){WINDOW_WIDTH, WINDOW_HEIGHT};

  // ---------------------------------------------------------------------------
  // Load (and position static'ish) content.
  game.cursor = (Cursor){.position = {0, 0},
                         .animation = Animation__from_path_template(
                             "content/kursori/kursori00%02d.png",
                             ANIMATION_FRAME_COUNT_CURSOR,
                             ANIMATION_LENGTH_MILLIS_CURSOR)};
  game.cursor.animation.state = LOOPING;

  MorteGame__spawn_uggy(&game, PRIEST);

  game.hud = (HUD){.border = LoadTexture("content/border.png")};

  Texture2D background0 = LoadTexture("content/tausta/tausta-0.jpg");
  game.backgrounds[0] = (Background){.texture = background0,
                                     .position = {-background0.width / 2, 0}};
  Texture2D background1 = LoadTexture("content/tausta/tausta-1.png");
  game.backgrounds[1] = (Background){.texture = background1,
                                     .position = {-background1.width / 2, 0}};
  Texture2D background2 = LoadTexture("content/tausta/edusta.png");
  game.backgrounds[2] = (Background){
      .texture = background2,
      .position = {-background2.width / 2, LEVEL_HEIGHT - background2.height}};
  // ---------------------------------------------------------------------------

  // ---------------------------------------------------------------------------
  // Initialize level
  srand(666);
  game.constants = DEFAULT_MORTE_GAME_CONSTANTS;

  // Ground.
  game.level_bounds[0] = PhysicsBody__new(
      STATIC, (Rectangle){-LEVEL_WIDTH / 2, LEVEL_HEIGHT, LEVEL_WIDTH, 50}, 0);
  // Ceiling.
  game.level_bounds[1] = PhysicsBody__new(
      STATIC, (Rectangle){-LEVEL_WIDTH / 2, -50, LEVEL_WIDTH, 50}, 0);
  // Left wall.
  game.level_bounds[2] = PhysicsBody__new(
      STATIC, (Rectangle){-LEVEL_WIDTH / 2 - 50, 0, 50, LEVEL_HEIGHT}, 0);
  // Right wall.
  game.level_bounds[3] = PhysicsBody__new(
      STATIC, (Rectangle){LEVEL_WIDTH / 2, 0, 50, LEVEL_HEIGHT}, 0);

  MorteGame__add_physics_body(&game, game.level_bounds[0]);
  MorteGame__add_physics_body(&game, game.level_bounds[1]);
  MorteGame__add_physics_body(&game, game.level_bounds[2]);
  MorteGame__add_physics_body(&game, game.level_bounds[3]);

  game.gravity = 400;
  // ---------------------------------------------------------------------------

  return game;
}

/* (What a mess this function's idea is...) */
MorteGame MorteGame__reset(MorteGame *self, DEBUG *debug_instance) {
  if (self != NULL) {
    MorteGame__free(self);
    *self = MorteGame__initialize();
    self->debug = debug_instance;
    return *self;
  } else {
    MorteGame game = MorteGame__initialize();
    game.debug = debug_instance;
    return game;
  }
}

void MorteGame__update_uggy(MorteGame *self, Uggy *uggy, float delta) {
  Animation__update(&uggy->animation, delta);

  switch (uggy->type) {
  case PRIEST:
    // Player character input handling.

    // Horizontal movement control.
    if (IsKeyDown(KEY_D)) {
      self->player->body->force.x =
          self->constants.player_walk_speed *
          (self->player->body->is_grounded ? 1.0f : 0.1f);
    } else if (IsKeyDown(KEY_A)) {
      self->player->body->force.x =
          -self->constants.player_walk_speed *
          (self->player->body->is_grounded ? 1.0f : 0.1f);
    } else {
      self->player->body->force.x = 0;
    }

    // Vertical movement control.
    if (IsKeyDown(KEY_SPACE) && self->player->body->is_grounded) {
      self->player->body->force.y += self->constants.player_jump_speed;
      self->player->body->is_grounded = false;
    }
    break;
  case SNAKE:
    break;
  case WACKO:
    break;
  case GULL:
    if (uggy->body->aabb.y > 50.0f) {
      uggy->body->force = (Vector2){
          .x =
              fmin(30.0f, fabs(30.0f - uggy->body->velocity.x)) *
              (self->player->body->aabb.x > uggy->body->aabb.x ? 1.0f : -1.0f) *
              Clamp(frand(), 0.8f, 1.0f),
          .y = -10.0 * Clamp(frand(), 0.8f, 1.0f),
      };
    }
    break;
  case HAND:
    break;
  }
}

void MorteGame__draw(MorteGame *self) {
  BeginDrawing();

  ClearBackground(BACKGROUND_COLOR);

  BeginMode2D(self->camera);

  for (size_t i = 0; i < 2; i++) {
    DrawTexture(self->backgrounds[i].texture, self->backgrounds[i].position.x,
                self->backgrounds[i].position.y, WHITE);
  }

  for (size_t i = 0; i < self->uggy_count; i++) {
    Uggy__draw(self->uggies[i], self->camera, self->cursor);
  }

  DrawTexture(self->backgrounds[2].texture, self->backgrounds[2].position.x,
              self->backgrounds[2].position.y, WHITE);

  DrawTexture(
      self->cursor.animation.frames[self->cursor.animation.current_frame],
      self->cursor.position.x, self->cursor.position.y, WHITE);

  if (self->debug) {
    DEBUG__draw(self->debug);
  }

  EndMode2D();

  DrawTextureEx(self->hud.border, (Vector2){0}, 0, self->camera.zoom, WHITE);

  if (self->debug) {
    DrawText("DEBUG", 45, 35, 50, GREEN);
  }

  if (self->is_paused) {
    char *text = "PAUSED";
    int font_size = 50;
    int text_width = MeasureText(text, font_size);
    DrawText(text, WINDOW_WIDTH / 2 - text_width / 2, WINDOW_HEIGHT / 2,
             font_size, RED);
  }

  EndDrawing();
}

enum GameStatus {
  GAME_RUNNING,
  GAME_PAUSED,
  GAME_RESET,
  GAME_DEBUGGING,
};

/*
 * User control updates.
 *
 * Returns true if the game loop should continue to the end of this frame and
 * false if not.
 */
enum GameStatus MorteGame__process_meta_input(MorteGame *self,
                                              DEBUG *debug_instance) {
  if (self->debug) {
    self->constants.player_walk_speed = 500.0;

    self->camera.zoom += ((float)GetMouseWheelMove() * 0.05f);

    // Enemy spawn control.
    for (size_t i = SNAKE; i < HAND; i++) {
      if (IsKeyPressed(KEY_ZERO + i)) {
        MorteGame__spawn_uggy(self, i);
      }
    }
  } else {
    self->constants = DEFAULT_MORTE_GAME_CONSTANTS;
  }

  if (IsKeyDown(KEY_LEFT_CONTROL)) {
    if (IsKeyPressed(KEY_R)) {
      return GAME_RESET;
    }

    if (IsKeyPressed(KEY_D)) {
      return GAME_DEBUGGING;
    }
  }

  if (IsKeyPressed(KEY_P)) {
    self->is_paused = !self->is_paused;
  }

  if (self->is_paused) {
    return GAME_PAUSED;
  }

  return GAME_RUNNING;
}

/* Perform game logic updates. */
void MorteGame__update(MorteGame *self, float delta) {
  MorteGame__update_physics(self, delta);

  self->cursor.position = GetScreenToWorld2D(GetMousePosition(), self->camera);

  for (size_t i = 0; i < self->uggy_count; i++) {
    MorteGame__update_uggy(self, self->uggies[i], delta);
  }

  Animation__update(&self->cursor.animation, delta);

  MorteGame__focus_view_on(self, self->player->body->aabb);

  // Update parallax according to updated camera target position.
  float relative_level_offset_x = self->camera.target.x / LEVEL_WIDTH;
  for (size_t i = 0; i < 2; i++) {
    float magic_alignment_factor =
        (double)LEVEL_WIDTH /
        (2.0 * LEVEL_WIDTH - self->backgrounds[i].texture.width);
    self->backgrounds[i].position.x =
        // Follow camera.
        self->camera.target.x -
        self->backgrounds[i].texture.width / 2.0f
        // Offset relative to own size.
        + self->backgrounds[i].texture.width *
              relative_level_offset_x
              // Align the far-ends of the images with each other.
              * magic_alignment_factor
              // Move opposite to camera travel direction.
              * -1.0f;
  }
}

int main(void) {
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
             "Morte Mysteria dom Domine dem Daemonium");

  SetTargetFPS(60);
  DisableCursor();

  // DEBUG
  DEBUG debug_instance = {0};

  MorteGame game = MorteGame__reset(NULL, &debug_instance);

  while (!WindowShouldClose()) {
    float delta = GetFrameTime();

    switch (MorteGame__process_meta_input(&game, &debug_instance)) {
    case GAME_DEBUGGING:
      if (game.debug) {
        game.debug = NULL;
      } else {
        game.debug = &debug_instance;
      }
      // Fall to game state update.

    case GAME_RUNNING:
      // Refresh debug drawing ready for this next frame frame.
      if (game.debug) {
        game.debug->draw_queue_length = 0;
      }

      MorteGame__update(&game, delta);
      // Fall to draw.

    case GAME_PAUSED:
      // Skip game logic updates.
      MorteGame__draw(&game);
      break;

    case GAME_RESET:
      // Start the game loop from the beginning.
      MorteGame__reset(&game, &debug_instance);
      // FIXME? For some reason the (keyboard) input presses stays "on" if a
      // drawing cycle is not completed.
      // Draw nothing so as not to flash the uninitialized scene in between.
      BeginDrawing();
      EndDrawing();
      break;
    }
  }

  // De-Initialization.
  // ---------------------------------------------------------------------------
  // XXX: NOTE That textures need to be unloaded before closing window.
  MorteGame__free(&game);

  CloseWindow();
  // ---------------------------------------------------------------------------

  return 0;
}
