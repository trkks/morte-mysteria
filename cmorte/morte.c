/*******************************************************************************************
 * Morte mysteria
 ********************************************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib/raylib.h"
#include "raylib/raymath.h"

#define DOWN (Vector2){0, 1}
#define E 0.00001f
/* 7/4 ratio */
#define ASPECT_RATIO 1.75
/* View[port] height is used to scale the visual size */
#define WINDOW_HEIGHT 800
#define WINDOW_WIDTH (WINDOW_HEIGHT * ASPECT_RATIO)
#define LEVEL_HEIGHT 400
#define LEVEL_WIDTH 2800
#define BACKGROUND_COLOR (Color){133, 31, 10, 255}

#define MAX_SPEED 666.0f

#define ANIMATION_FRAME_COUNT_CURSOR 19
#define ANIMATION_LENGTH_MILLIS_CURSOR 750
#define ANIMATION_FRAME_COUNT_GULL 19
#define ANIMATION_LENGTH_MILLIS_GULL 1000

#define APPEND(list, length, type, item)                                       \
  list = realloc(list, (length + 1) * sizeof(type));                           \
  list[length] = item;                                                         \
  length += 1;

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
  Vector2 end_position;
} DEBUG_visual;

typedef struct {
  size_t draw_queue_length;
  DEBUG_visual *draw_queue;
  bool pause_game_after_this_frame;
} DEBUG;

void DEBUG__enqueue(DEBUG *self, DEBUG_visual object) {
  APPEND(self->draw_queue, self->draw_queue_length, DEBUG_visual, object);
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
                           float dir_x, float dir_y, float length,
                           Color color) {
  float scaled_length = 250.0f * (length / MAX_SPEED);
  Vector2 end_position = (Vector2){start_x + dir_x * scaled_length,
                                   start_y + dir_y * scaled_length};

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

void DEBUG__draw_rectangle(DEBUG *self, Rectangle rec, Color color) {
  DEBUG__enqueue(self, (DEBUG_visual){
                           .type = RECTANGLE,
                           .color = color,
                           .position = {rec.x, rec.y},
                           .size = {rec.width, rec.height},
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
      Rectangle rec = (Rectangle){object.position.x, object.position.y,
                                  object.size.x, object.size.y};
      // Make it so that the border sticks out of the shape a bit so that it
      // can be seen even if right at the edge of the view frame.
      rec.x -= 2.5f;
      rec.y -= 2.5f;
      rec.width += 5.0f;
      rec.height += 5.0f;
      DrawRectangleLinesEx(rec, 2, object.color);
      break;
    case ARROW:
      float size = 2.0f;
      DrawLineEx(object.position, object.end_position, size, object.color);
      /* The arrow tip's edge on the left side of the direction line.
       *     \
       *  -----
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

void DEBUG__free(DEBUG *self) { free(self->draw_queue); }
// -----------------------------------------------------------------------------

float frand() { return (float)rand() / (float)RAND_MAX; }

bool float__eq(float a, float b) { return b - E < a && a < b + E; }

bool float__is_positive(float a) { return !float__eq(a, 0) && -E < a; }

bool Vector2__eq(Vector2 a, Vector2 b) {
  return float__eq(a.x, b.x) && float__eq(a.y, b.y);
}

Vector2 Rectangle__center(Rectangle self) {
  return (Vector2){self.x + self.width / 2.0f, self.y + self.height / 2.0f};
}

typedef struct {
  Rectangle aabb;
  float mass;
  float inverse_mass;
  Vector2 velocity;
  /* This stores interactions for physics simulation during a single frame. */
  Vector2 impulse;
} PhysicsBody;

typedef struct {
  bool happened;
  float depth;
  /* This is the direction where the collider was moving when collision
   * happened. */
  Vector2 direction;
} Collision;

PhysicsBody *PhysicsBody__new(Rectangle aabb, float mass) {
  PhysicsBody *self = malloc(sizeof(PhysicsBody));
  self->aabb = aabb;
  self->mass = mass;
  self->inverse_mass = 1.0f / mass;
  self->velocity = (Vector2){0, 0};
  self->impulse = (Vector2){0, 0};
  return self;
}

/* Return if and how self collides to other.
 *
 * ## Kudos:
 * -
 * https://gamedevelopment.tutsplus.com/tutorials/how-to-create-a-custom-2d-physics-engine-the-basics-and-impulse-resolution--gamedev-6331
 * - https://textbooks.cs.ksu.edu/cis580/04-collisions/index.html
 */
Collision PhysicsBody__colliding(PhysicsBody *self, PhysicsBody *other) {
  Vector2 self_half =
      Vector2Scale((Vector2){self->aabb.width, self->aabb.height}, 0.5f);
  Vector2 other_half =
      Vector2Scale((Vector2){other->aabb.width, other->aabb.height}, 0.5f);
  Vector2 distance = {
      other->aabb.x + other_half.x - (self->aabb.x + self_half.x),
      other->aabb.y + other_half.y - (self->aabb.y + self_half.y),
  };

  Vector2 overlap = {self_half.x + other_half.x - fabs(distance.x),
                     self_half.y + other_half.y - fabs(distance.y)};

  Collision collision = {.happened = false};
  if (float__is_positive(overlap.x) && float__is_positive(overlap.y)) {
    collision.happened = true;
    if (overlap.x < overlap.y) {
      collision.depth = overlap.x;
      collision.direction = (Vector2){distance.x > 0 ? 1 : -1, 0};
    } else {
      collision.depth = overlap.y;
      collision.direction = (Vector2){0, distance.y > 0 ? 1 : -1};
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

/* Copy `frames` for animation.
 */
Animation *Animation__from_frames(size_t frame_count, Texture2D *frames,
                                  unsigned length_ms) {
  Animation *self = malloc(sizeof(Animation));
  self->state = STOPPED;
  self->frame_count = frame_count;
  self->frames = malloc(frame_count * sizeof(Texture2D));
  self->current_frame = 0;
  self->length_ms = length_ms;
  self->elapsed_ms = 0;
  self->timing = LINEAR;

  for (size_t i = 0; i < self->frame_count; i++) {
    self->frames[i] = frames[i];
  }

  return self;
}

/* Initialize and load animation frames from a string template filepath using
 * zero-left-padded indexes [0, `frame_count`).
 *
 * NOTE: This method assumes using the `template` will yield constant length
 * strings (as per the padding condition).
 *
 * NOTE: The maximum path length cannot exceed 500 ASCII-characters.
 */
Animation *Animation__from_path_template(const char *template,
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

/* This enum works as a mask to match into the different categories of
 * EntityTypes (poor man's polymorphism).
 */
enum EntityCategory {
  PROP = 0x0000ff,
  UGGY = 0x00ff00,
  LOOT = 0xff0000,
};

enum EntityType {
  WALL = PROP,

  WACKO = UGGY,
  HAND,
  SNAKE,
  GULL,
  PRIEST,

  GRENADE = LOOT,
  HAT,
  CANNABIS,
  SAW,
  MUSHROOM,
  WINE,
};

enum EntityState {
  NONE = 0,
  DRAGGING,
};

/* Represents objects/characters in the game world.
 */
typedef struct {
  enum EntityCategory category;
  enum EntityType type;
  enum EntityState state;
  PhysicsBody *body;
  Animation *animation;
  // For PRIEST type.
  Texture2D eye_texture;
} Entity;

Entity *Entity__new(enum EntityType type, PhysicsBody *body,
                    Animation *animation) {
  Entity *self = malloc(sizeof(Entity));
  if (type < (int)LOOT) {
    if (type < (int)UGGY) {
      self->category = PROP;
    } else {
      self->category = UGGY;
    }
  } else {
    self->category = LOOT;
  }
  self->type = type;
  self->body = body;
  self->animation = animation;
  return self;
}

typedef struct {
  Vector2 position;
  Animation *animation;
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

void Entity__draw_priest_eye(Entity *self, Camera2D camera, Cursor cursor,
                             bool left_side) {
  // Eye in own coordinates.
  Vector2 independent_eye_pos = {self->eye_texture.width / 2,
                                 self->eye_texture.height / 2};
  // Eye in Priest coordinates.
  Vector2 relative_eye_pos = {self->animation->frames[0].width / 2,
                              self->animation->frames[0].height * 0.06};

  // Eye in world coordinates.
  Vector2 absolute_eye_pos =
      Vector2Add(Vector2Add(independent_eye_pos, relative_eye_pos),
                 (Vector2){self->body->aabb.x, self->body->aabb.y});

  // Translate based on eye's side.
  if (left_side) {
    // NOTE: For some reason not translating by whole number makes the eye
    // shaky...
    absolute_eye_pos.x -= 11.0f;
  } else {
    absolute_eye_pos.x += 1.0f;
  }

  // Rotate the eyes to look at the cursor.
  DrawTexturePro(
      self->eye_texture,
      (Rectangle){0, 0, self->eye_texture.width, self->eye_texture.height},
      // Floor()ing prevents jittering when moving the character along.
      (Rectangle){floor(absolute_eye_pos.x), absolute_eye_pos.y,
                  self->eye_texture.width, self->eye_texture.height},
      independent_eye_pos, degrees_between(absolute_eye_pos, cursor.position),
      WHITE);
}

void Entity__draw(Entity *self, Camera2D camera, Cursor cursor) {
  if (self->animation) {
    DrawTexture(self->animation->frames[self->animation->current_frame],
                self->body->aabb.x, self->body->aabb.y, WHITE);
  }

  switch (self->type) {
  case WALL:
    break;
  case WACKO:
    break;
  case HAND:
    break;
  case SNAKE:
    break;
  case GULL:
    break;
  case PRIEST:
    Entity__draw_priest_eye(self, camera, cursor, true);
    Entity__draw_priest_eye(self, camera, cursor, false);
    break;
  }
}

/* Because of how collisions is implemented, sometimes the "order" of collision
 * matters for collision resolution thus actor and target are specified.
 */
typedef struct {
  Entity *actor;
  Entity *target;
  Collision collision;
} CollisionPair;

typedef struct {
  Texture2D border;
} HUD;

typedef struct {
  float player_walk_speed;
  float player_jump_speed;
} MorteGameConstants;

const MorteGameConstants DEFAULT_MORTE_GAME_CONSTANTS = {
    .player_walk_speed = 100.0f,
    .player_jump_speed = 350.0f,
};

typedef struct {
  bool is_paused;
  MorteGameConstants constants;
  Vector2 view_size;
  float gravity;
  Cursor cursor;
  Camera2D camera;
  // Convenience handle to the player Entity.
  Entity *player;
  size_t animation_count;
  size_t physics_body_count;
  size_t entity_count;
  PhysicsBody **physics_bodies;
  Entity **entities;
  Animation **animations;
  CollisionPair *collision_pairs;

  Background backgrounds[3];
  HUD hud;
  DEBUG *debug;
} MorteGame;

void MorteGame__free(MorteGame *self) {
  for (size_t i = 0; i < self->animation_count; i++) {
    Animation__free(self->animations[i]);
  }
  free(self->animations);

  for (size_t i = 0; i < self->physics_body_count; i++) {
    free(self->physics_bodies[i]);
  }
  free(self->physics_bodies);

  // NOTE: Need to destroy this before destroying player along with other
  // entities.
  UnloadTexture(self->player->eye_texture);

  for (size_t i = 0; i < self->entity_count; i++) {
    free(self->entities[i]);
  }
  free(self->entities);

  for (size_t i = 0; i < 3; i++) {
    UnloadTexture(self->backgrounds[i].texture);
  }

  UnloadTexture(self->hud.border);

  free(self->collision_pairs);

  DEBUG__free(self->debug);
}

void MorteGame__add_animation(MorteGame *self, Animation *animation) {
  APPEND(self->animations, self->animation_count, Animation *, animation);
}

void MorteGame__add_physics_body(MorteGame *self, PhysicsBody *body) {
  APPEND(self->physics_bodies, self->physics_body_count, PhysicsBody *, body);

  // Amount of possible collisions is increased by addition of a new body.
  size_t max_collisions = (self->physics_body_count * self->physics_body_count -
                           self->physics_body_count) /
                          2;
  self->collision_pairs =
      realloc(self->collision_pairs, max_collisions * sizeof(CollisionPair));
}

void MorteGame__add_entity(MorteGame *self, Entity *entity) {
  APPEND(self->entities, self->entity_count, Entity *, entity);

  MorteGame__add_physics_body(self, entity->body);

  if (entity->animation) {
    MorteGame__add_animation(self, entity->animation);
  }
}

/* Check for and report collisions between physics bodies preventing.
 */
size_t MorteGame__collisions(MorteGame *self, float delta) {
  size_t k = 0;

  for (size_t i = 0; i < self->entity_count; i++) {
    Entity *a = self->entities[i];

    for (size_t j = i + 1; j < self->entity_count; j++) {
      Entity *b = self->entities[j];

      Collision collision = PhysicsBody__colliding(a->body, b->body);
      if (collision.happened) {
        if (self->debug) {
          DEBUG__draw_rectangle(self->debug, a->body->aabb, YELLOW);
          DEBUG__draw_rectangle(self->debug, b->body->aabb, YELLOW);
        }

        self->collision_pairs[k] =
            (CollisionPair){.actor = a, .target = b, .collision = collision};
        k++;
      }
    }
  }

  return k;
}

void MorteGame__collide_to_wall(Entity *e, Collision collision) {
  // Separate the collider from the wall.
  e->body->aabb.x -= collision.direction.x * collision.depth;
  e->body->aabb.y -= collision.direction.y * collision.depth;

  // Stop when dropping onto a platform.
  if (float__eq(collision.direction.y, DOWN.y)) {
    e->body->velocity.y = 0;
    e->body->impulse.y = 0;
  }
}

void MorteGame__resolve_collision_WALL(MorteGame *self, CollisionPair c) {
  if (c.target->type & UGGY) {
    Collision flipped = c.collision;
    flipped.direction = Vector2Scale(flipped.direction, -1.0f);
    MorteGame__collide_to_wall(c.target, flipped);
  }
}

void MorteGame__resolve_collision_WACKO(MorteGame *self, CollisionPair c) {}

void MorteGame__resolve_collision_HAND(MorteGame *self, CollisionPair c) {}

void MorteGame__resolve_collision_SNAKE(MorteGame *self, CollisionPair c) {}

void MorteGame__resolve_collision_GULL(MorteGame *self, CollisionPair c) {
  switch (c.target->type) {
  case PRIEST:
    // Pick up the priest with talons.
    c.actor->state = DRAGGING;
    break;
  }
}

void MorteGame__resolve_collision_PRIEST(MorteGame *self, CollisionPair c) {}

void MorteGame__resolve_collision_GRENADE(MorteGame *self, CollisionPair c) {}

void MorteGame__resolve_collision_HAT(MorteGame *self, CollisionPair c) {}

void MorteGame__resolve_collision_CANNABIS(MorteGame *self, CollisionPair c) {}

void MorteGame__resolve_collision_SAW(MorteGame *self, CollisionPair c) {}

void MorteGame__resolve_collision_MUSHROOM(MorteGame *self, CollisionPair c) {}

void MorteGame__resolve_collision_WINE(MorteGame *self, CollisionPair c) {}

void MorteGame__resolve_collision(MorteGame *self, CollisionPair c) {
  switch (c.actor->type) {
    /////////////////////////////////////////////////////////////////////////////
    // PROPS
  case WALL:
    MorteGame__resolve_collision_WALL(self, c);
    break;
    /////////////////////////////////////////////////////////////////////////////
    // UGGIES
  case WACKO:
    MorteGame__resolve_collision_WACKO(self, c);
    break;
  case HAND:
    MorteGame__resolve_collision_HAND(self, c);
    break;
  case SNAKE:
    MorteGame__resolve_collision_SNAKE(self, c);
    break;
  case GULL:
    MorteGame__resolve_collision_GULL(self, c);
    break;
  case PRIEST:
    MorteGame__resolve_collision_PRIEST(self, c);
    break;
    /////////////////////////////////////////////////////////////////////////////
    // LOOT
  case GRENADE:
    MorteGame__resolve_collision_GRENADE(self, c);
    break;
  case HAT:
    MorteGame__resolve_collision_HAT(self, c);
    break;
  case CANNABIS:
    MorteGame__resolve_collision_CANNABIS(self, c);
    break;
  case SAW:
    MorteGame__resolve_collision_SAW(self, c);
    break;
  case MUSHROOM:
    MorteGame__resolve_collision_MUSHROOM(self, c);
    break;
  case WINE:
    MorteGame__resolve_collision_WINE(self, c);
    break;
  }
}

/* While keeping the view inside the level bounds, focus camera's center on
 * the `object` center.
 */
void MorteGame__focus_view_on(MorteGame *self, Rectangle object) {
  // Offset the target if moving too close to level edges.
  self->camera.target =
      // Floor()ing prevents jittering when moving the camera along.
      (Vector2){floor(Clamp(object.x + object.width / 2,
                            -LEVEL_WIDTH / 2 + self->view_size.x / 4,
                            LEVEL_WIDTH / 2 - self->view_size.x / 4)),
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

void MorteGame__spawn_entity(MorteGame *self, enum EntityType type) {
  switch (type) {
  case WALL:
    break;
  case WACKO:
    break;
  case HAND:
    break;
  case SNAKE:
    break;
  case GULL:
    Animation *gull_animation = Animation__from_path_template(
        "content/uggies/gull/lokki%04d.png", ANIMATION_FRAME_COUNT_GULL,
        ANIMATION_LENGTH_MILLIS_GULL);
    Entity *gull = Entity__new(
        GULL,
        PhysicsBody__new((Rectangle){self->player->body->aabb.x + 100,
                                     self->player->body->aabb.y - 100,
                                     gull_animation->frames[0].width,
                                     gull_animation->frames[0].height},
                         20),
        gull_animation);
    gull->animation->state = LOOPING;
    MorteGame__add_entity(self, gull);
    break;
  case PRIEST:
    if (self->player != NULL) {
      puts("\033[31mAttempted adding player twice\033[0m");
      exit(1);
    }

    // Loading content.
    Texture2D player_texture = LoadTexture("content/uggies/pappi.png");
    Texture2D eye_texture = LoadTexture("content/silma.png");

    // Initialization.
    self->player = Entity__new(
        PRIEST,
        PhysicsBody__new(

            (Rectangle){-LEVEL_WIDTH / 2, LEVEL_HEIGHT - player_texture.height,
                        player_texture.width, player_texture.height},
            100),
        Animation__from_frames(1, &player_texture, 0));

    // Specialization.
    self->player->eye_texture = eye_texture;

    // Adding to sim.
    MorteGame__add_entity(self, self->player);
    break;
  }
}
MorteGame MorteGame__initialize() {
  MorteGame game = {
      .is_paused = false,
      .constants = DEFAULT_MORTE_GAME_CONSTANTS,
      .view_size = (Vector2){WINDOW_WIDTH, WINDOW_HEIGHT},
      .gravity = 10.0f,
      .player = NULL,
      .animation_count = 0,
      .physics_body_count = 0,
      .entity_count = 0,
      .physics_bodies = NULL,
      .entities = NULL,
      .animations = NULL,
      .collision_pairs = NULL,
      .debug = NULL,
  };

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
  game.cursor.animation->state = LOOPING;
  MorteGame__add_animation(&game, game.cursor.animation);

  MorteGame__spawn_entity(&game, PRIEST);

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
  MorteGame__add_entity(
      &game,
      Entity__new(
          WALL,
          PhysicsBody__new(
              (Rectangle){-LEVEL_WIDTH / 2, LEVEL_HEIGHT, LEVEL_WIDTH, 50}, 0),
          NULL));

  // Left wall.
  MorteGame__add_entity(
      &game, Entity__new(WALL,
                         PhysicsBody__new((Rectangle){-LEVEL_WIDTH / 2 - 50, 0,
                                                      50, LEVEL_HEIGHT},
                                          0),
                         NULL));

  // Right wall.
  MorteGame__add_entity(
      &game,
      Entity__new(WALL,
                  PhysicsBody__new(
                      (Rectangle){LEVEL_WIDTH / 2, 0, 50, LEVEL_HEIGHT}, 0),
                  NULL));

  game.gravity = 10.0f;
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

void MorteGame__behave_entity(MorteGame *self, Entity *entity) {
  switch (entity->type) {
  case WALL:
    break;
  case WACKO:
    break;
  case HAND:
    break;
  case SNAKE:
    break;
  case GULL:
    if (entity->state == DRAGGING) {
      entity->body->impulse = (Vector2){
          .x = entity->body->velocity.x * Clamp(frand(), 0.8f, 1.0f),
          .y = -500.0 * Clamp(frand(), 0.8f, 1.0f),
      };
    }
    if (entity->body->aabb.y > 50.0f) {
      entity->state = NONE;
      float floating = fmin(30.0f, fabs(30.0f - entity->body->velocity.x));
      float homing = Rectangle__center(self->player->body->aabb).x >
                             Rectangle__center(entity->body->aabb).x
                         ? 1.0f
                         : -1.0f;
      entity->body->impulse = (Vector2){
          .x = floating * homing * Clamp(frand(), 0.8f, 1.0f),
          .y = -300.0 * Clamp(frand(), 0.8f, 1.0f),
      };
    }
    break;
  case PRIEST:
    // Player character input handling.

    // Movement control.
    Vector2 horizontal = (Vector2){0};
    // Horizontal.
    if (IsKeyDown(KEY_D)) {
      horizontal.x = self->constants.player_walk_speed;
    } else if (IsKeyDown(KEY_A)) {
      horizontal.x = -self->constants.player_walk_speed;
    } else {
      // Stop immediately.
      horizontal.x = 0;
    }

    if (float__eq(self->player->body->velocity.y, 0)) {
      // Jump from the ground into the air.
      if (IsKeyDown(KEY_SPACE)) {
        entity->body->velocity.y = -self->constants.player_jump_speed;
      }
    } else {
      // Make air-strafing a bit harder than ground movement.
      entity->body->impulse.x *= 0.95f;
    }

    // Apply straight to velocity in order to avoid having to wait speeding
    // up.
    entity->body->velocity.x = horizontal.x;

    break;
  }

  // Consider gravity.
  if (entity->category != PROP && !float__eq(entity->body->velocity.y, 0)) {
    // Only apply gravity on objects moving in the air.
    entity->body->impulse.y += self->gravity * entity->body->mass;
  }
}

void MorteGame__draw(MorteGame *self, float delta) {
  BeginDrawing();

  ClearBackground(BACKGROUND_COLOR);

  BeginMode2D(self->camera);

  for (size_t i = 0; i < 2; i++) {
    DrawTexture(self->backgrounds[i].texture, self->backgrounds[i].position.x,
                self->backgrounds[i].position.y, WHITE);
  }

  for (size_t i = 0; i < self->entity_count; i++) {
    Entity *entity = self->entities[i];
    Entity__draw(entity, self->camera, self->cursor);

    const char *state_text;
    switch (entity->state) {
    case NONE:
      state_text = "None";
      break;
    case DRAGGING:
      state_text = "Dragging";
      break;
    }

    if (self->debug) {
      DrawText(state_text, entity->body->aabb.x + entity->body->aabb.width + 10,
               entity->body->aabb.y, 10, WHITE);

      char position_text[3 + 8] = "x: -0000\0";

      sprintf(position_text, "x: %d", (int)entity->body->aabb.x);
      DrawText(position_text,
               entity->body->aabb.x + entity->body->aabb.width + 10,
               entity->body->aabb.y + 12, 10, WHITE);

      sprintf(position_text, "y: %d", (int)entity->body->aabb.y);
      DrawText(position_text,
               entity->body->aabb.x + entity->body->aabb.width + 10,
               entity->body->aabb.y + 24, 10, WHITE);
    }
  }

  DrawTexture(self->backgrounds[2].texture, self->backgrounds[2].position.x,
              self->backgrounds[2].position.y, WHITE);

  DrawTexture(
      self->cursor.animation->frames[self->cursor.animation->current_frame],
      self->cursor.position.x, self->cursor.position.y, WHITE);

  if (self->debug) {
    DEBUG__draw(self->debug);
  }

  EndMode2D();

  DrawTextureEx(self->hud.border, (Vector2){0}, 0, self->camera.zoom, WHITE);

  if (self->debug) {
    DrawText("DEBUG", 45, 35, 50, GREEN);

    char fps_text[4] = "NaN\0";
    int fps = 1.0f / delta;
    if (fps < 1000) {
      sprintf(fps_text, "%d", fps);
    }
    DrawText(fps_text, WINDOW_WIDTH - 100, 35, 50, GREEN);
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
  GAME_DO_RUN,
  GAME_DO_PAUSE,
  GAME_DO_RESET,
  GAME_DO_DEBUG,
};

/* User control updates.
 *
 * Returns true if the game loop should continue to the end of this frame and
 * false if not.
 */
enum GameStatus MorteGame__process_meta_input(MorteGame *self,
                                              DEBUG *debug_instance) {
  if (self->debug) {
    self->debug->pause_game_after_this_frame = false;
    self->constants.player_walk_speed = 500.0;

    self->camera.zoom += ((float)GetMouseWheelMove() * 0.05f);

    // Enemy spawn control.
    for (size_t i = WACKO + 1; i <= GULL; i++) {
      if (IsKeyPressed(KEY_ZERO + i - WACKO)) {
        MorteGame__spawn_entity(self, i);
      }
    }

    if (IsKeyPressed(KEY_N) || IsKeyPressedRepeat(KEY_N)) {
      self->is_paused = false;
      self->debug->pause_game_after_this_frame = true;
    }

  } else {
    self->constants = DEFAULT_MORTE_GAME_CONSTANTS;
  }

  if (IsKeyDown(KEY_LEFT_CONTROL)) {
    if (IsKeyPressed(KEY_R)) {
      return GAME_DO_RESET;
    }

    if (IsKeyPressed(KEY_D)) {
      return GAME_DO_DEBUG;
    }
  }

  if (IsKeyPressed(KEY_P)) {
    self->is_paused = !self->is_paused;
  }

  if (self->is_paused) {
    return GAME_DO_PAUSE;
  }

  return GAME_DO_RUN;
}

/* Perform game logic updates. */
void MorteGame__update(MorteGame *self, float delta) {
  for (size_t i = 0; i < self->entity_count; i++) {
    MorteGame__behave_entity(self, self->entities[i]);
  }

  // Integrate movement.
  for (size_t i = 0; i < self->physics_body_count; i++) {
    PhysicsBody *body = self->physics_bodies[i];

    // Semi-implicit Euler integration (velocity _before_ position).
    body->velocity.x += body->impulse.x * delta;
    body->velocity.y += body->impulse.y * delta;
    body->aabb.x += body->velocity.x * delta;
    body->aabb.y += body->velocity.y * delta;

    if (self->debug) {
      // Debug the physics body movement result.
      DEBUG__draw_rectangle(self->debug, body->aabb, MAGENTA);
    }

    // NOTE: Reset impulses for next frame.
    body->impulse = (Vector2){0};

    if (self->debug) {
      float vl = Vector2Length(body->velocity);
      if (vl > 0) {
        DEBUG__draw_direction(self->debug, body->aabb.x + body->aabb.width / 2,
                              body->aabb.y + body->aabb.height / 2,
                              body->velocity.x / vl, body->velocity.y / vl, vl,
                              GREEN);
      }
    }
  }

  // Check and resolve collisions in bulk to avoid movement between collisions
  // (i.e., in the same frame X collides with Y and immediately moves out of
  // the way, but then Z does not detect collision with the now moved X).
  size_t collision_count = MorteGame__collisions(self, delta);

  for (size_t i = 0; i < collision_count; i++) {
    CollisionPair original = self->collision_pairs[i];
    MorteGame__resolve_collision(self, original);

    // Because of how collision checking is implemented (< N^2), the pair needs
    // to be re-handled "flipped" so that both entities resolve while being the
    // actor once.
    CollisionPair flipped = {
        .actor = original.target,
        .target = original.actor,
        .collision = {
            .depth = original.collision.depth,
            .direction = Vector2Scale(original.collision.direction, -1.0f),
        }};
    MorteGame__resolve_collision(self, flipped);
  }

  self->cursor.position = GetScreenToWorld2D(GetMousePosition(), self->camera);

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

  for (int i = 0; i < self->animation_count; i++) {
    Animation__update(self->animations[i], delta);
  }
}

int main(void) {
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
             "Morte Mysteria dom Domine dem Daemonium");

  SetTargetFPS(60);
  DisableCursor();

  // DEBUG
  DEBUG debug_instance = {
      .draw_queue_length = 0,
      .draw_queue = NULL,
      .pause_game_after_this_frame = false,
  };

  MorteGame game = MorteGame__reset(NULL, &debug_instance);

  while (!WindowShouldClose()) {
    float delta = GetFrameTime();

    switch (MorteGame__process_meta_input(&game, &debug_instance)) {
    case GAME_DO_DEBUG:
      if (game.debug) {
        game.debug = NULL;
      } else {
        game.debug = &debug_instance;
      }
      // Fall to game state update.

    case GAME_DO_RUN:
      // Refresh debug drawing ready for this next frame frame.
      if (game.debug) {
        game.debug->draw_queue_length = 0;
      }

      MorteGame__update(&game, delta);
      // Fall to draw.

    case GAME_DO_PAUSE:
      // Skip game logic updates.
      MorteGame__draw(&game, delta);
      if (game.debug && game.debug->pause_game_after_this_frame) {
        game.is_paused = true;
      }
      break;

    case GAME_DO_RESET:
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
