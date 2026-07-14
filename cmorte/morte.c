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
#define WINDOW_SCALE (WINDOW_HEIGHT / LEVEL_HEIGHT)
#define LEVEL_HEIGHT 400
#define LEVEL_WIDTH 2800
#define BACKGROUND_COLOR (Color){133, 31, 10, 255}

#define MAX_SPEED 666.0f

#define ENTITY_INVINCIBILITY_TIME_SECONDS 0.5

#define PLAYER_MAX_HEALTH 100
#define GULL_ATTACK_DAMAGE 10

#define ANIMATION_FRAME_COUNT_CURSOR 19
#define ANIMATION_LENGTH_MILLIS_CURSOR 750
#define ANIMATION_FRAME_COUNT_GULL 19
#define ANIMATION_LENGTH_MILLIS_GULL 1000

#define NEW_T_ARRAY(array_type, item_type)                                     \
  (array_type) {                                                               \
    .length = 0, .size = 256, .data = malloc(256 * sizeof(item_type))          \
  }

#define WITH_SIZE_T_ARRAY(array_type, item_type, size_)                        \
  (array_type) {                                                               \
    .length = 0, .size = size_, .data = malloc(size_ * sizeof(item_type))      \
  }

#define APPEND_T_ARRAY(array, type, item)                                      \
  {                                                                            \
    if (array.size <= (array.length + 1)) {                                    \
      array.data = realloc(array.data, (array.size * 2) * sizeof(type));       \
    }                                                                          \
    array.data[array.length] = item;                                           \
    array.length += 1;                                                         \
  }

#define LAST_T_ARRAY(array) &array.data[array.length - 1]

#define RESIZE_T_ARRAY(array, new_size, item_type)                             \
  {                                                                            \
    array.data = realloc(array.data, new_size * sizeof(item_type));            \
    array.size = new_size;                                                     \
  }

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
  // Amount of items in the array.
  size_t length;
  // Amount of space in the array (always greater or equal to length).
  size_t size;
  // Pointer to the contained data.
  DEBUG_visual *data;
} DEBUG_visualArray;

typedef struct {
  DEBUG_visualArray draw_queue;
  bool pause_game_after_this_frame;
} DEBUG;

void DEBUG__enqueue(DEBUG *self, DEBUG_visual object) {
  APPEND_T_ARRAY(self->draw_queue, DEBUG_visual, object);
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
  for (size_t i = 0; i < self->draw_queue.length; i++) {
    DEBUG_visual object = self->draw_queue.data[i];

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

void DEBUG__free(DEBUG *self) { free(self->draw_queue.data); }
// -----------------------------------------------------------------------------

float frand() { return (float)rand() / (float)RAND_MAX; }

bool float__eq(float a, float b) { return b - E < a && a < b + E; }

bool float__is_positive(float a) { return !float__eq(a, 0.0f) && -E < a; }

float float__lerp(float a, float b, float t) { return (1.0f - t) * a + t * b; }

bool Vector2__eq(Vector2 a, Vector2 b) {
  return float__eq(a.x, b.x) && float__eq(a.y, b.y);
}

Vector2 Rectangle__relative_center(Rectangle self) {
  return (Vector2){self.width / 2.0f, self.height / 2.0f};
}

Vector2 Rectangle__absolute_center(Rectangle self) {
  return Vector2Add((Vector2){self.x, self.y},
                    Rectangle__relative_center(self));
}

typedef struct {
  float delta;
  double elapsed;
} Time;

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

PhysicsBody PhysicsBody__new(Rectangle aabb, float mass) {
  PhysicsBody self;
  self.aabb = aabb;
  self.mass = mass;
  self.inverse_mass = 1.0f / mass;
  self.velocity = (Vector2){0, 0};
  self.impulse = (Vector2){0, 0};
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

typedef struct {
  Texture2D *content;
  size_t frame_count;
  // Duration of the whole animation in milliseconds.
  unsigned length_ms;
} Frames;

/* Load animation frames from a string template filepath using zero-left-padded
 * indexes [0, `frame_count`).
 *
 * NOTE: This method assumes using the `template` will yield constant length
 * strings (as per the padding condition).
 *
 * NOTE: The maximum path length cannot exceed 500 ASCII-characters.
 */
Frames Frames__from_path_template(const char *path_template, size_t frame_count,
                                  unsigned length_ms) {
  Texture2D *content = malloc(frame_count * sizeof(Texture2D));
  // Limit path length to 500 characters.
  char filename[501];
  for (size_t i = 0; i < frame_count; i++) {
    sprintf(filename, path_template, (int)i + 1);
    content[i] = LoadTexture(filename);
  }
  return (Frames){content, frame_count, length_ms};
}

enum AnimationState { STOPPED, PLAYING_ONCE, LOOPING };

enum AnimationTiming { LINEAR, EASE_IN, EASE_OUT };

typedef struct {
  bool is_mirrored;
  enum AnimationState state;
  size_t current_frame;
  unsigned elapsed_ms;
  enum AnimationTiming timing;
  Color color;
  Frames frames;
} Animation;

/* Copy `frames` for animation.
 */
Animation Animation__from_frames(Frames frames) {
  Animation self;
  self.is_mirrored = false;
  self.state = STOPPED;
  self.frames = frames;
  self.current_frame = 0;
  self.elapsed_ms = 0;
  self.timing = LINEAR;
  self.color = WHITE;

  return self;
}

void Animation__update(Animation *self, Time time) {
  if (self->state == STOPPED) {
    return;
  }

  self->elapsed_ms += 1000 * time.delta;
  float t = fmin(1.0f, (float)self->elapsed_ms / (float)self->frames.length_ms);

  switch (self->timing) {
  case LINEAR:
    self->current_frame = t * self->frames.frame_count;
    break;
  case EASE_IN:
    self->current_frame =
        (1.0f - cosf(t * PI / 2.0f)) * self->frames.frame_count;
    break;
  case EASE_OUT:
    self->current_frame = sinf(t * PI / 2.0f) * self->frames.frame_count;
    break;
  }

  if (self->current_frame >= self->frames.frame_count) {
    self->current_frame = 0;
    self->elapsed_ms = 0;

    if (self->state == PLAYING_ONCE) {
      self->state = STOPPED;
    }
  }
}

void Frames__free(Frames self) {
  for (size_t i = 0; i < self.frame_count; i++) {
    UnloadTexture(self.content[i]);
  }

  free(self.content);
}

/* This enum works as a mask to match into the different categories of
 * EntityTypes (poor man's polymorphism).
 */
enum EntityCategory {
  PROP = 0x0000ff,
  UGGY = 0x00ff00,
  LOOT = 0xff0000,
};

// TODO: Set up these int-values so that they could actually be used as
// bitmasks for tag-based interactions e.g., GULL should hit all PROPS, weapon
// LOOTS and UGGIES except other GULLS => gull.collision_mask = PROP | (LOOT &
// ~(MUSHROOM | WINE)) | (UGGY & ~GULL);
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

char *const EntityType__to_string(enum EntityType self) {
  switch (self) {
  case WALL:
    return "Wall";
  case WACKO:
    return "Wacko";
  case HAND:
    return "Hand";
  case SNAKE:
    return "Snake";
  case GULL:
    return "Gull";
  case PRIEST:
    return "Priest";
  case GRENADE:
    return "Grenade";
  case HAT:
    return "Hat";
  case CANNABIS:
    return "Cannabis";
  case SAW:
    return "Saw";
  case MUSHROOM:
    return "Mushroom";
  case WINE:
    return "Wine";
  }
};

enum EntityState {
  NONE = 0,
  DRAGGING,
  DRAGGED,
};

/* Represents objects/characters in the game world.
 */
typedef struct Entity {
  enum EntityCategory category;
  enum EntityType type;
  enum EntityState state;
  PhysicsBody *body;
  Animation *animation;
  // For UGGY category.
  int health;
  // For UGGY category.
  double hurt_time;
  // For PRIEST type.
  Texture2D eye_texture;
  // For GULL type.
  struct Entity *drag_target;
  // For GRENADE type.
  float rotation;
} Entity;

Entity Entity__new(enum EntityType type) {
  Entity self;
  if (type < (int)LOOT) {
    if (type < (int)UGGY) {
      self.category = PROP;
    } else {
      self.category = UGGY;
    }
  } else {
    self.category = LOOT;
  }
  self.type = type;
  self.state = NONE;
  self.body = NULL;
  self.animation = NULL;
  self.health = 0;
  self.hurt_time = 0.0;
  self.drag_target = NULL;
  self.rotation = 0.0f;
  return self;
}

typedef struct {
  bool move_right;
  bool move_left;
  bool jump;
  bool throw_grenade;
  // In-world coordinate of where player is aiming at time of shoot.
  Vector2 cursor_position;
} UserInput;

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

enum CollisionEventType {
  NOT_COLLIDING,
  COLLISION_ENTER,
  COLLIDING,
  COLLISION_EXIT,
};

char *const CollisionEventType__to_string(enum CollisionEventType self) {
  switch (self) {
  case NOT_COLLIDING:
    return "Not Colliding";
  case COLLISION_ENTER:
    return "Collision Enter";
  case COLLIDING:
    return "Colliding";
  case COLLISION_EXIT:
    return "Collision Exit";
  }
}

/* Abstraction to help me think about how an actor acts when it collides to
 * a target instead of shuffling both entities' behavior in the same scope.
 */
typedef struct {
  enum CollisionEventType type;
  // Elapsed game time in seconds at the time of the event.
  double time_stamp;
  Collision collision;
  size_t actor_handle;
  size_t target_handle;
  Entity *actor;
  Entity *target;
} CollisionEvent;

typedef struct {
  Texture2D border;
  Texture2D cross[2];
} HUD;

typedef struct {
  float player_walk_speed;
  float player_jump_speed;
  Vector2 grenade_launch_velocity;
} MorteGameConstants;

const MorteGameConstants DEFAULT_MORTE_GAME_CONSTANTS = {
    .player_walk_speed = 100.0f,
    .player_jump_speed = 350.0f,
    .grenade_launch_velocity = (Vector2){500, -250},
};

typedef struct {
  Frames cursor_frames;
  Frames gull_frames;
  Frames priest_frames;
  Frames priest_eye_frames;
  Frames grenade_frames;
} MorteGameAssets;

// TODO: Simplify this into an enum that indexes to a continuous array of
// frames' starting points for straightforward cleanup?
void MorteGameAssets__free(MorteGameAssets self) {
  Frames__free(self.cursor_frames);
  Frames__free(self.gull_frames);
  Frames__free(self.priest_frames);
  Frames__free(self.priest_eye_frames);
  Frames__free(self.grenade_frames);
}

typedef struct {
  // Amount of items in the array.
  size_t length;
  // Amount of space in the array (always greater or equal to length).
  size_t size;
  // Pointer to the contained data.
  Animation *data;
} AnimationArray;

typedef struct {
  // Amount of items in the array.
  size_t length;
  // Amount of space in the array (always greater or equal to length).
  size_t size;
  // Pointer to the contained data.
  PhysicsBody *data;
} PhysicsBodyArray;

typedef struct {
  // Amount of items in the array.
  size_t length;
  // Amount of space in the array (always greater or equal to length).
  size_t size;
  // Pointer to the contained data.
  Entity *data;
} EntityArray;

typedef struct {
  // Amount of items in the array.
  size_t length;
  // Amount of space in the array (always greater or equal to length).
  size_t size;
  // Pointer to the contained data.
  CollisionEvent *data;
} CollisionEventArray;

typedef struct {
  bool is_paused;
  bool is_game_over;
  MorteGameConstants constants;
  Vector2 view_size;
  float gravity;
  UserInput user_input;
  MorteGameAssets assets;
  Animation *cursor_animation;
  Camera2D camera;
  // Convenience handle to the player Entity.
  Entity *player;
  PhysicsBodyArray physics_bodies;
  EntityArray entities;
  AnimationArray animations;
  CollisionEventArray collision_history;

  // TODO: Just forget this and put it in animations -collection.
  Background backgrounds[3];
  HUD hud;
  DEBUG *debug;
} MorteGame;

void MorteGame__free(MorteGame *self) {
  MorteGameAssets__free(self->assets);

  free(self->animations.data);

  free(self->physics_bodies.data);
  // NOTE: Need to destroy this before destroying player along with other
  // entities.
  UnloadTexture(self->player->eye_texture);

  free(self->entities.data);

  for (size_t i = 0; i < 3; i++) {
    UnloadTexture(self->backgrounds[i].texture);
  }

  UnloadTexture(self->hud.border);

  free(self->collision_history.data);
}

Animation *MorteGame__add_animation(MorteGame *self, Animation animation) {
  APPEND_T_ARRAY(self->animations, Animation, animation);

  Animation *ptr = LAST_T_ARRAY(self->animations);
  printf("Added Animation %p\n", ptr);
  return ptr;
}

PhysicsBody *MorteGame__add_physics_body(MorteGame *self, PhysicsBody body) {
  APPEND_T_ARRAY(self->physics_bodies, PhysicsBody, body);

  PhysicsBody *ptr = LAST_T_ARRAY(self->physics_bodies);
  printf("Added PhysicsBody %p\n", ptr);
  return ptr;
}

/* The animation parameter is optional: pass NULL if not desired. */
Entity *MorteGame__add_entity(MorteGame *self, Entity entity, PhysicsBody body,
                              Animation *animation) {
  entity.body = MorteGame__add_physics_body(self, body);

  if (animation) {
    entity.animation = MorteGame__add_animation(self, *animation);
  }

  APPEND_T_ARRAY(self->entities, Entity, entity);
  if (self->entities.size * self->entities.size >
      self->collision_history.size) {
    size_t old_size = self->collision_history.size;
    // The history must be made to fit all possible entity pairings.
    RESIZE_T_ARRAY(self->collision_history,
                   self->entities.size * self->entities.size, CollisionEvent);
    printf("Resized collision history %ld -> %ld\n", old_size,
           self->collision_history.size);
  }

  Entity *ptr = LAST_T_ARRAY(self->entities);
  printf("Added Entity %p\n", ptr);
  return ptr;
}

void MorteGame__remove_entity(MorteGame *self, size_t entity_idx) {
  // Write over the entity data in collections effectively removing it from sim.
  if (self->entities.length > 1) {
    self->entities.data[entity_idx] = *LAST_T_ARRAY(self->entities);
  }

  // Erase from collision history the events that _removed entity_ was part of
  // and update the events that _replacing entity_ is part of.
  // TODO

  // Finalize swapping the one item from tail.
  self->entities.length -= 1;

  printf("Removed Entity %p\n", &self->entities.data[entity_idx]);
}

void MorteGame__update_user_input(MorteGame *self) {
  self->user_input = (UserInput){
      .move_right = IsKeyDown(KEY_D),
      .move_left = IsKeyDown(KEY_A),
      .jump = IsKeyDown(KEY_W),
      .throw_grenade = IsKeyPressed(KEY_SPACE),
      .cursor_position = GetScreenToWorld2D(GetMousePosition(), self->camera),
  };
}

/* Check for and report collisions between physics bodies preventing.
 */
void MorteGame__collisions(MorteGame *self, Time time) {
  // Start collecting the collisions for this update.
  self->collision_history.length = 0;

  for (size_t i = 0; i < self->entities.length; i++) {
    Entity *a = &self->entities.data[i];

    for (size_t j = i + 1; j < self->entities.length; j++) {
      Entity *b = &self->entities.data[j];

      Collision collision = PhysicsBody__colliding(a->body, b->body);

      // NOTE: Not initializing .type field here...
      CollisionEvent event = (CollisionEvent){.actor_handle = i,
                                              .target_handle = j,
                                              .actor = a,
                                              .target = b,
                                              .collision = collision,
                                              .time_stamp = time.elapsed};

      enum CollisionEventType previous_event_type =
          self->collision_history.data[i + j].type;
      // ...but here.
      if (collision.happened) {
        if (previous_event_type == COLLIDING ||
            previous_event_type == COLLISION_ENTER) {
          event.type = COLLIDING;
        } else {
          event.type = COLLISION_ENTER;
        }
      } else if (previous_event_type == COLLIDING ||
                 previous_event_type == COLLISION_ENTER) {
        // FIXME: For some reason this is set even if no previous enter (Gull
        // <> Grenade)...
        event.type = COLLISION_EXIT;
      } else {
        event.type = NOT_COLLIDING;
      }

      // Append the new event to the list (a diagonal matrix). NOTE that this
      // keeps the events related to any entity idx re-discoverable: find "row"
      // of entity, and iterate from entity idx + 1 until the end.
      // E.g., find events related to entity 3:
      //   1 2 3 4 5        1. Entity 3 is associated with events B, E, H, I
      // 1[  A B C D]       2. Get to the column indexing matrix indexes 1 and 4
      // 2[    E F G]       3. Compute indexes (5-1)-3=1 and ((5-1)+(5-2))-3=4
      // 3[      H I]       4. Get to the row indexing matrix indexes 7 and 8
      // 4[        J]       5. Compute starting index (5-1)+(5-2)=4+3=7
      // 5[         ]       6. Robert is your parent's brother.
      APPEND_T_ARRAY(self->collision_history, CollisionEvent, event);

      if (self->debug) {
        if (collision.happened) {
          DEBUG__draw_rectangle(self->debug, a->body->aabb, YELLOW);
          DEBUG__draw_rectangle(self->debug, b->body->aabb, SKYBLUE);
        }
      }
    }
  }
}

void MorteGame__resolve_collision_WALL(MorteGame *self, CollisionEvent event) {}

void MorteGame__resolve_collision_WACKO(MorteGame *self, CollisionEvent event) {
}

void MorteGame__resolve_collision_HAND(MorteGame *self, CollisionEvent event) {}

void MorteGame__resolve_collision_SNAKE(MorteGame *self, CollisionEvent event) {
}

void MorteGame__resolve_collision_GULL(MorteGame *self, CollisionEvent event) {
  switch (event.target->type) {
  case PRIEST:
    switch (event.type) {
    case COLLISION_ENTER:
      if (event.actor->state == NONE && event.target->state == NONE) {
        // Pick up the priest with talons.
        event.actor->state = DRAGGING;
        event.target->state = DRAGGED;
        event.actor->drag_target = event.target;
      }
      break;
    case COLLISION_EXIT:
      // Set Priest finally free.
      event.target->state = NONE;
      break;
    }
    break;
  }
}

void MorteGame__resolve_collision_PRIEST(MorteGame *self,
                                         CollisionEvent event) {}

void MorteGame__resolve_collision_GRENADE(MorteGame *self,
                                          CollisionEvent event) {
  if (event.target->category == UGGY && event.target->type != PRIEST) {
    MorteGame__remove_entity(self, event.target_handle);
  }
}

void MorteGame__resolve_collision_HAT(MorteGame *self, CollisionEvent event) {}

void MorteGame__resolve_collision_CANNABIS(MorteGame *self,
                                           CollisionEvent event) {}

void MorteGame__resolve_collision_SAW(MorteGame *self, CollisionEvent event) {}

void MorteGame__resolve_collision_MUSHROOM(MorteGame *self,
                                           CollisionEvent event) {}

void MorteGame__resolve_collision_WINE(MorteGame *self, CollisionEvent event) {}

/* Select the matching method to handle collision for the c.actor. */
void MorteGame__resolve_collision(MorteGame *self, CollisionEvent event) {
  if (event.type == NOT_COLLIDING) {
    return;
  }

  printf("%s : %s (%p) <> %s (%p)\n", CollisionEventType__to_string(event.type),
         EntityType__to_string(event.actor->type), event.actor,
         EntityType__to_string(event.target->type), event.target);

  if (event.actor->category == UGGY) {
    switch (event.target->type) {
    case WALL:
      // Separate the collider from the wall.
      event.actor->body->aabb.x -=
          event.collision.direction.x * event.collision.depth;
      event.actor->body->aabb.y -=
          event.collision.direction.y * event.collision.depth;

      // Stop when dropping onto a platform.
      if (float__eq(event.collision.direction.y, DOWN.y)) {
        event.actor->body->velocity.y = 0;
        event.actor->body->impulse.y = 0;
      }
      break;
    }
  }

  switch (event.actor->type) {
    /////////////////////////////////////////////////////////////////////////////
    // PROPS
  case WALL:
    MorteGame__resolve_collision_WALL(self, event);
    break;
    /////////////////////////////////////////////////////////////////////////////
    // UGGIES
  case WACKO:
    MorteGame__resolve_collision_WACKO(self, event);
    break;
  case HAND:
    MorteGame__resolve_collision_HAND(self, event);
    break;
  case SNAKE:
    MorteGame__resolve_collision_SNAKE(self, event);
    break;
  case GULL:
    MorteGame__resolve_collision_GULL(self, event);
    break;
  case PRIEST:
    MorteGame__resolve_collision_PRIEST(self, event);
    break;
    /////////////////////////////////////////////////////////////////////////////
    // LOOT
  case GRENADE:
    MorteGame__resolve_collision_GRENADE(self, event);
    break;
  case HAT:
    MorteGame__resolve_collision_HAT(self, event);
    break;
  case CANNABIS:
    MorteGame__resolve_collision_CANNABIS(self, event);
    break;
  case SAW:
    MorteGame__resolve_collision_SAW(self, event);
    break;
  case MUSHROOM:
    MorteGame__resolve_collision_MUSHROOM(self, event);
    break;
  case WINE:
    MorteGame__resolve_collision_WINE(self, event);
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
  printf("Spawning entity '%s'\n", EntityType__to_string(type));
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
    Entity gull = Entity__new(GULL);
    Animation gull_animation = Animation__from_frames(self->assets.gull_frames);
    PhysicsBody gull_body =
        PhysicsBody__new((Rectangle){self->player->body->aabb.x + 100,
                                     self->player->body->aabb.y - 100,
                                     gull_animation.frames.content[0].width,
                                     gull_animation.frames.content[0].height},
                         20);

    gull_animation.state = LOOPING;

    MorteGame__add_entity(self, gull, gull_body, &gull_animation);
    break;
  case PRIEST:
    if (self->player != NULL) {
      puts("\033[31mAttempted adding player twice\033[0m");
      exit(1);
    }

    // Initialization.
    Animation player_animation =
        Animation__from_frames(self->assets.priest_frames);
    PhysicsBody player_body = PhysicsBody__new(
        (Rectangle){-LEVEL_WIDTH / 2,
                    LEVEL_HEIGHT - player_animation.frames.content[0].height,
                    player_animation.frames.content[0].width,
                    player_animation.frames.content[0].height},
        100);
    Entity player = Entity__new(PRIEST);

    // Specialization.
    player.health = PLAYER_MAX_HEALTH;
    // TODO: Make into an animation for blinking eyes?
    player.eye_texture = *self->assets.priest_eye_frames.content;

    // Adding to sim.
    self->player =
        MorteGame__add_entity(self, player, player_body, &player_animation);
    break;

  case GRENADE:
    Animation grenade_animation =
        Animation__from_frames(self->assets.grenade_frames);

    Vector2 offset = {
        self->player->body->aabb.x + self->player->body->aabb.width / 2 +
            (self->player->animation->is_mirrored ? -1.0f : 0.0f) *
                grenade_animation.frames.content[0].width * 2.0f,
        self->player->body->aabb.y + self->player->body->aabb.height / 2.0f -
            grenade_animation.frames.content[0].height};
    Entity grenade = Entity__new(GRENADE);
    PhysicsBody grenade_body = PhysicsBody__new(
        (Rectangle){offset.x, offset.y,
                    grenade_animation.frames.content[0].width,
                    grenade_animation.frames.content[0].height},
        50);

    // Direct from the side of player character's current orientation.
    float throw_direction = (self->player->animation->is_mirrored) ? -1 : 1;
    // Throw.
    grenade_body.velocity = Vector2Add(
        self->player->body->velocity,
        (Vector2){self->constants.grenade_launch_velocity.x * throw_direction,
                  self->constants.grenade_launch_velocity.y});

    MorteGame__add_entity(self, grenade, grenade_body, &grenade_animation);
    break;
  case HAT:
    break;
  case CANNABIS:
    break;
  case SAW:
    break;
  case MUSHROOM:
    break;
  case WINE:
    break;
  }
}

MorteGame MorteGame__initialize(DEBUG *debug_instance) {
  MorteGame game = {
      .is_paused = false,
      .is_game_over = false,
      .constants = DEFAULT_MORTE_GAME_CONSTANTS,
      .view_size = (Vector2){WINDOW_WIDTH, WINDOW_HEIGHT},
      .gravity = 10.0f,
      .player = NULL,
      .animations = NEW_T_ARRAY(AnimationArray, Animation),
      .physics_bodies = NEW_T_ARRAY(PhysicsBodyArray, PhysicsBody),
      .entities = NEW_T_ARRAY(EntityArray, Entity),
      .collision_history =
          WITH_SIZE_T_ARRAY(CollisionEventArray, CollisionEvent, 256 * 256),
      .debug = debug_instance,
  };

  game.camera = (Camera2D){0};
  game.camera.zoom = WINDOW_SCALE;
  game.view_size = (Vector2){WINDOW_WIDTH, WINDOW_HEIGHT};

  // ---------------------------------------------------------------------------
  // Load content.
  game.assets.gull_frames = Frames__from_path_template(
      "content/uggies/gull/lokki%04d.png", ANIMATION_FRAME_COUNT_GULL,
      ANIMATION_LENGTH_MILLIS_GULL);
  game.assets.cursor_frames = Frames__from_path_template(
      "content/kursori/kursori00%02d.png", ANIMATION_FRAME_COUNT_CURSOR,
      ANIMATION_LENGTH_MILLIS_CURSOR);
  game.assets.priest_frames =
      Frames__from_path_template("content/uggies/pappi.png", 1, 0);
  game.assets.priest_eye_frames =
      Frames__from_path_template("content/silma.png", 1, 0);
  game.assets.grenade_frames =
      Frames__from_path_template("content/loot/grenade.png", 1, 0);

  // Initialize entities.
  Animation cursor_animation =
      Animation__from_frames(game.assets.cursor_frames);
  cursor_animation.state = LOOPING;
  game.cursor_animation = MorteGame__add_animation(&game, cursor_animation);

  MorteGame__spawn_entity(&game, PRIEST);

  if (game.debug) {
    MorteGame__spawn_entity(&game, GULL);
  }

  game.hud = (HUD){.border = LoadTexture("content/border.png"),
                   .cross = {LoadTexture("content/cross/vertical.png"),
                             LoadTexture("content/cross/horizontal.png")}};

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
      &game, Entity__new(WALL),
      PhysicsBody__new(
          (Rectangle){-LEVEL_WIDTH / 2, LEVEL_HEIGHT, LEVEL_WIDTH, 50}, 0),
      NULL);

  // Left wall.
  MorteGame__add_entity(
      &game, Entity__new(WALL),
      PhysicsBody__new((Rectangle){-LEVEL_WIDTH / 2 - 50, 0, 50, LEVEL_HEIGHT},
                       0),
      NULL);

  // Right wall.
  MorteGame__add_entity(
      &game, Entity__new(WALL),
      PhysicsBody__new((Rectangle){LEVEL_WIDTH / 2, 0, 50, LEVEL_HEIGHT}, 0),
      NULL);

  game.gravity = 10.0f;
  // ---------------------------------------------------------------------------

  return game;
}

/* (What a mess this function's idea is...) */
MorteGame MorteGame__reset(MorteGame *self, DEBUG *debug_instance) {
  if (self != NULL) {
    MorteGame__free(self);
    *self = MorteGame__initialize(debug_instance);
    return *self;
  } else {
    MorteGame game = MorteGame__initialize(debug_instance);
    return game;
  }
}

void MorteGame__behave_entity(MorteGame *self, Entity *entity, Time time) {
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
      if (entity->body->aabb.y < 30.0f) {
        // Drop the target to ground.
        entity->state = NONE;
        entity->drag_target->body->velocity =
            Vector2Scale(entity->body->velocity, 0.5f);
        entity->drag_target = NULL;
      } else {
        // Keep pulling the target higher into the sky.
        entity->body->impulse = (Vector2){
            .x = entity->body->velocity.x * Clamp(frand(), 0.8f, 1.0f),
            .y = -500.0 * Clamp(frand(), 0.8f, 1.0f),
        };

        // Position the drag target with the grabbing talons.
        Vector2 actor_center = Rectangle__absolute_center(entity->body->aabb);
        entity->drag_target->body->aabb.x =
            actor_center.x - entity->drag_target->body->aabb.width / 2.1f;
        entity->drag_target->body->aabb.y =
            actor_center.y + entity->body->aabb.y / 4.0f;

        // Prevent accumulating gravity on drag target while airborne.
        entity->drag_target->body->velocity = (Vector2){0};
      }
    }
    if (entity->body->aabb.y > 30.0f) {
      float floating = fmin(30.0f, fabs(30.0f - entity->body->velocity.x));
      float homing = Rectangle__absolute_center(self->player->body->aabb).x >
                             Rectangle__absolute_center(entity->body->aabb).x
                         ? 1.0f
                         : -1.0f;
      entity->body->impulse = (Vector2){
          .x = floating * homing * Clamp(frand(), 0.8f, 1.0f),
          .y = -300.0 * Clamp(frand(), 0.8f, 1.0f),
      };
    }
    break;
  case PRIEST:
    // Game over.
    if (entity->health <= 0) {
      self->is_game_over = true;
    }
    // Movement control.
    Vector2 horizontal = (Vector2){0};
    // Horizontal.
    if (self->user_input.move_right) {
      horizontal.x = self->constants.player_walk_speed;
      entity->animation->is_mirrored = false;
    } else if (self->user_input.move_left) {
      horizontal.x = -self->constants.player_walk_speed;
      entity->animation->is_mirrored = true;
    } else {
      // Stop immediately.
      horizontal.x = 0;
    }

    if (float__eq(self->player->body->velocity.y, 0)) {
      // Jump from the ground into the air.
      if (self->user_input.jump) {
        entity->body->velocity.y = -self->constants.player_jump_speed;
      }
    } else {
      // Make air-strafing a bit harder than ground movement.
      entity->body->impulse.x *= 0.95f;
    }

    // Apply straight to velocity in order to avoid having to wait speeding
    // up.
    entity->body->velocity.x = horizontal.x;

    if (time.elapsed < entity->hurt_time + ENTITY_INVINCIBILITY_TIME_SECONDS) {
      double hurt_t = (time.elapsed - entity->hurt_time) /
                      ENTITY_INVINCIBILITY_TIME_SECONDS;
      entity->animation->color = ColorLerp(RED, WHITE, hurt_t);
    }

    switch (entity->state) {
    case DRAGGED:
      if (entity->hurt_time + ENTITY_INVINCIBILITY_TIME_SECONDS <
          time.elapsed) {
        entity->health -= GULL_ATTACK_DAMAGE;
        entity->hurt_time = time.elapsed;
      }
      break;
    case NONE:
      if (self->user_input.throw_grenade) {
        MorteGame__spawn_entity(self, GRENADE);
      }
    }

    break;
  case GRENADE:
    float spin_multiplier =
        Vector2Length(entity->body->velocity) /
        Vector2Length(self->constants.grenade_launch_velocity);
    entity->rotation += 10.0f * spin_multiplier;
    break;
  }

  // Consider gravity.
  if (entity->category != PROP && !float__eq(entity->body->velocity.y, 0)) {
    // Only apply gravity on objects moving in the air.
    entity->body->impulse.y += self->gravity * entity->body->mass;
  }
}

void MorteGame__draw_priest_eye(MorteGame *self, Entity priest,
                                bool left_side) {
  // Eye in own coordinates.
  Vector2 independent_eye_pos = {priest.eye_texture.width / 2,
                                 priest.eye_texture.height / 2};
  // Eye in Priest coordinates.
  Vector2 relative_eye_pos = {priest.animation->frames.content[0].width / 2,
                              priest.animation->frames.content[0].height *
                                  0.06};

  // Eye in world coordinates.
  Vector2 absolute_eye_pos =
      Vector2Add(Vector2Add(independent_eye_pos, relative_eye_pos),
                 (Vector2){priest.body->aabb.x, priest.body->aabb.y});

  float priest_direction = priest.animation->is_mirrored ? 0 : 2;
  // Translate based on eye's side.
  if (left_side) {
    // NOTE: For some reason not translating by whole number makes the eye
    // shaky...
    absolute_eye_pos.x += -12.0f + priest_direction;
  } else {
    absolute_eye_pos.x += 1.0f + priest_direction;
  }

  // Rotate the eyes to look at the cursor.
  DrawTexturePro(
      priest.eye_texture,
      (Rectangle){0, 0, priest.eye_texture.width, priest.eye_texture.height},
      // Floor()ing prevents jittering when moving the character along.
      (Rectangle){floor(absolute_eye_pos.x), absolute_eye_pos.y,
                  priest.eye_texture.width, priest.eye_texture.height},
      independent_eye_pos,
      degrees_between(absolute_eye_pos, self->user_input.cursor_position),
      WHITE);
}

void MorteGame__draw_entity(MorteGame *self, Entity entity) {
  if (entity.animation) {
    Texture2D frame =
        entity.animation->frames.content[entity.animation->current_frame];
    float frame_direction = entity.animation->is_mirrored ? 1 : -1;
    Vector2 relative_center = Rectangle__relative_center(entity.body->aabb);
    DrawTexturePro(
        frame, (Rectangle){0, 0, frame.width * frame_direction, frame.height},
        (Rectangle){entity.body->aabb.x + relative_center.x,
                    entity.body->aabb.y + relative_center.y, frame.width,
                    frame.height},
        relative_center, entity.rotation, entity.animation->color);
  }

  switch (entity.type) {
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
    MorteGame__draw_priest_eye(self, entity, true);
    MorteGame__draw_priest_eye(self, entity, false);
    break;
  }
}

void MorteGame__draw(MorteGame *self, Time time) {
  BeginDrawing();

  ClearBackground(BACKGROUND_COLOR);

  BeginMode2D(self->camera);

  for (size_t i = 0; i < 2; i++) {
    DrawTexture(self->backgrounds[i].texture, self->backgrounds[i].position.x,
                self->backgrounds[i].position.y, WHITE);
  }

  for (size_t i = 0; i < self->entities.length; i++) {
    Entity entity = self->entities.data[i];
    MorteGame__draw_entity(self, entity);

    if (self->debug) {
      const char *state_text;
      switch (entity.state) {
      case NONE:
        state_text = "None";
        break;
      case DRAGGING:
        state_text = "Dragging";
        break;
      case DRAGGED:
        state_text = "Dragged";
        break;
      default:
        state_text = "UNDEFINED";
        break;
      }
      DrawText(state_text, entity.body->aabb.x + entity.body->aabb.width + 10,
               entity.body->aabb.y, 10, WHITE);

      char health_text[0 + 8] = "-0000\0";
      sprintf(health_text, "%d", (int)entity.health);
      DrawText(health_text, entity.body->aabb.x + entity.body->aabb.width + 10,
               entity.body->aabb.y + 12, 10, GREEN);

      char position_text[3 + 8] = "x: -0000\0";
      sprintf(position_text, "x: %d", (int)entity.body->aabb.x);
      DrawText(position_text,
               entity.body->aabb.x + entity.body->aabb.width + 10,
               entity.body->aabb.y + 24, 10, WHITE);
      sprintf(position_text, "y: %d", (int)entity.body->aabb.y);
      DrawText(position_text,
               entity.body->aabb.x + entity.body->aabb.width + 10,
               entity.body->aabb.y + 36, 10, WHITE);
    }
  }

  DrawTexture(self->backgrounds[2].texture, self->backgrounds[2].position.x,
              self->backgrounds[2].position.y, WHITE);

  // TODO: Move this to generic (layered/ordered!) animations -collection
  // and animate everything (including entities) in one common for-loop.
  // NOTE that should then separate game/physics bodies from skin/animation
  // for getting the draw-positions (might not always want to perfectly
  // overlap visual with collision-body).
  DrawTexture(self->cursor_animation->frames
                  .content[self->cursor_animation->current_frame],
              self->user_input.cursor_position.x,
              self->user_input.cursor_position.y, WHITE);

  if (self->debug) {
    DEBUG__draw(self->debug);
  }

  EndMode2D();

  DrawTextureEx(self->hud.border, (Vector2){0}, 0, WINDOW_SCALE, WHITE);
  const float BORDER_THICKNESS = 13;
  const float HUD_MARGIN = 5;

  // Visualize decreasing health with a decline in both purity and
  // christianity.
  float t_health = (float)self->player->health / (float)PLAYER_MAX_HEALTH;
  Color cross_color = ColorLerp(GetColor(0x221111FF), RED, t_health);
  Vector2 cross_v_pos =
      (Vector2){HUD_MARGIN + BORDER_THICKNESS + self->hud.cross[1].width / 2 -
                    self->hud.cross[0].width / 2,
                HUD_MARGIN + BORDER_THICKNESS};
  float decline = float__lerp(self->hud.cross[0].height * 0.7,
                              self->hud.cross[0].height * 0.3, t_health);
  Vector2 cross_h_pos =
      (Vector2){cross_v_pos.x - self->hud.cross[1].width / 2 +
                    self->hud.cross[0].width / 2,
                cross_v_pos.y - self->hud.cross[1].height / 2 + decline};
  // The positions must be scaled for window size.
  cross_v_pos = Vector2Scale(cross_v_pos, WINDOW_SCALE);
  cross_h_pos = Vector2Scale(cross_h_pos, WINDOW_SCALE);
  DrawTextureEx(self->hud.cross[0], cross_v_pos, 0, WINDOW_SCALE, cross_color);
  DrawTextureEx(self->hud.cross[1], cross_h_pos, 0, WINDOW_SCALE, cross_color);

  if (self->debug) {
    DrawText("DEBUG", WINDOW_WIDTH / 2 - 84, 35, 50, GREEN);

    char fps_text[4] = "NaN\0";
    int fps = 1.0f / time.delta;
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

  if (self->is_game_over) {
    char *text = "Game Over";
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
  GAME_DO_END,
  GAME_DO_RESET,
  GAME_DO_DEBUG,
};

/* User control updates related to game state (i.e., not player character
 * controls).
 *
 * Returns true if the game loop should continue to the end of this frame
 * and false if not.
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
    if (IsKeyPressed(KEY_MINUS)) { // PLUS.
      self->camera.zoom += 0.5;
    }
    if (IsKeyPressed(KEY_SLASH)) { // MINUS.
      self->camera.zoom -= 0.5;
    }

  } else {
    self->constants = DEFAULT_MORTE_GAME_CONSTANTS;
  }

  if (IsKeyPressed(KEY_R)) {
    return GAME_DO_RESET;
  }

  if (IsKeyDown(KEY_LEFT_CONTROL)) {
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

  if (self->is_game_over) {
    return GAME_DO_END;
  }

  return GAME_DO_RUN;
}

/* Perform game logic updates. */
void MorteGame__update(MorteGame *self, Time time) {
  MorteGame__update_user_input(self);

  for (size_t i = 0; i < self->entities.length; i++) {
    MorteGame__behave_entity(self, &self->entities.data[i], time);
  }

  // Integrate movement.
  for (size_t i = 0; i < self->physics_bodies.length; i++) {
    PhysicsBody *body = &self->physics_bodies.data[i];

    // Semi-implicit Euler integration (velocity _before_ position).
    body->velocity.x += body->impulse.x * time.delta;
    body->velocity.y += body->impulse.y * time.delta;
    body->aabb.x += body->velocity.x * time.delta;
    body->aabb.y += body->velocity.y * time.delta;

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

  // Check and resolve collisions in bulk to avoid movement between
  // collisions (i.e., in the same frame X collides with Y and immediately
  // moves out of the way, but then Z does not detect collision with the now
  // moved X).
  MorteGame__collisions(self, time);

  for (size_t i = 0; i < self->collision_history.length; i++) {
    CollisionEvent original = self->collision_history.data[i];
    MorteGame__resolve_collision(self, original);

    // Because of how collision checking is implemented ((N^2 - N) / 2), the
    // pair needs to be re-handled "flipped" so that both entities resolve
    // while being the actor once.
    CollisionEvent flipped = original; // Copy fields for editing.
    flipped.actor = original.target;
    flipped.target = original.actor;
    flipped.collision.direction =
        Vector2Scale(original.collision.direction, -1.0f);
    MorteGame__resolve_collision(self, flipped);
  }

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

  for (int i = 0; i < self->animations.length; i++) {
    Animation__update(&self->animations.data[i], time);
  }
}

int main(void) {
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
             "Morte Mysteria dom Domine dem Daemonium");

  SetTargetFPS(60);
  DisableCursor();

  // DEBUG is a singleton and thus needs to be separated from game
  // (dependency injection).
  DEBUG debug_instance = {
      .draw_queue = NEW_T_ARRAY(DEBUG_visualArray, DEBUG_visual),
      .pause_game_after_this_frame = false,
  };

  MorteGame game = MorteGame__reset(NULL, &debug_instance);

  while (!WindowShouldClose()) {
    Time time = {.delta = GetFrameTime(), .elapsed = GetTime()};

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
        game.debug->draw_queue.length = 0;
      }

      MorteGame__update(&game, time);
      // Fall to draw.

    case GAME_DO_END:
      // TODO: Add some game over -animation?
    case GAME_DO_PAUSE:
      // Skip game logic updates.
      MorteGame__draw(&game, time);
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
  // De-initialize DEBUG separately just like it was created separately.
  DEBUG__free(&debug_instance);

  CloseWindow();
  // ---------------------------------------------------------------------------

  return 0;
}
