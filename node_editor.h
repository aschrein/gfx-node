#ifndef NODE_EDITOR_H
#define NODE_EDITOR_H

#include "utils.hpp"

#if __EMSCRIPTEN__
#include <GLES3/gl3.h>
#include <SDL.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#else
#include <GLES3/gl32.h>
#include <SDL2/SDL.h>
#endif

#include <glm/glm.hpp>
using namespace glm;
using float2 = vec2;
using float3 = vec3;
using float4 = vec4;
using int2   = ivec2;
using int3   = ivec3;
using int4   = ivec4;
using uint2  = uvec2;
using uint3  = uvec3;
using uint4  = uvec4;

struct Oth_Camera {
  float3   pos;
  float4   proj[4];
  float    height_over_width;
  float    width_over_heigth;
  u32      viewport_x;
  u32      viewport_width;
  u32      viewport_y;
  u32      viewport_height;
  f32      world_min_x;
  f32      world_min_y;
  f32      world_max_x;
  f32      world_max_y;
  f32      mouse_world_x;
  f32      mouse_world_y;
  f32      mouse_screen_x;
  f32      mouse_screen_y;
  f32      glyphs_world_height;
  f32      glyphs_screen_height;
  f32      glyphs_world_width;
  f32      glyphs_screen_width;
  f32      pixel_screen_width;
  f32      pixel_screen_height;
  f32      fovy, fovx;
  uint32_t glyph_scale = 1;
  void     update(u32 viewport_x, u32 viewport_y, u32 viewport_width, u32 viewport_height);
  float2   world_to_screen(float2 p) {
    float  x0  = glm::dot(proj[0], (float4){p.x, p.y, 0.0f, 1.0f});
    float  x1  = glm::dot(proj[1], (float4){p.x, p.y, 0.0f, 1.0f});
//    float  x2  = glm::dot(proj[2], (float4){p.x, p.y, 0.0f, 1.0f});
    float  x3  = glm::dot(proj[3], (float4){p.x, p.y, 0.0f, 1.0f});
    float2 pos = (float2){x0, x1} / x3;
    return pos;
  }
  float2 screen_to_world(float2 p) {
    return (float2){
        //
        p.x * pos.z / fovx + pos.x,
        p.y * pos.z / fovy + pos.y,
    };
  }
  float2 window_to_world(int2 p) {
    return screen_to_world((float2){
        //
        2.0f * ((float)(p.x - viewport_x) + 0.5f) / viewport_width - 1.0f,
        -2.0f * ((float)(p.y - viewport_y) - 0.5f) / viewport_height + 1.0f,
    });
  }
  float2 window_to_screen(int2 p) {
    return (float2){
        //
        width_over_heigth * 2.0f * ((float)(p.x - viewport_x) / viewport_width - 0.5f),
        2.0f * (-(float)(p.y - viewport_y) / viewport_height + 0.5f),
    };
  }
  float window_width_to_world(uint32_t size) {
    float screen_size = 2.0f * (float)size / viewport_width;
    return screen_to_world((float2){//
                                    screen_size, 0.0f})
               .x -
           pos.x;
  }
  float window_height_to_world(uint32_t size) {
    float screen_size = 2.0f * (float)size / viewport_height;
    return screen_to_world((float2){//
                                    0.0f, screen_size})
               .y -
           pos.y;
  }
  bool inbounds(float2 world_pos) {
    return //
        world_pos.x > world_min_x && world_pos.x < world_max_x && world_pos.y > world_min_y &&
        world_pos.y < world_max_y;
  }
  bool intersects(float min_x, float min_y, float max_x, float max_y) {
    return //
        !(max_x < world_min_x || min_x > world_max_x || max_y < world_min_y || min_y > world_max_y);
  }
  void consume_event(SDL_Event event);
};

struct Color {
  float r, g, b;
};
struct Rect2D {
  float x, y, z, width, height;
  Color color;
  bool  world_space = true;
};
struct Line2D {
  float x0, y0, x1, y1, z;
  Color color;
  bool  world_space = true;
};
struct String2D {
  char const *c_str;
  float       x, y, z;
  Color       color;
  bool        world_space = true;
};

struct Context2D {
  Oth_Camera camera;

  void draw_rect(Rect2D p);
  void draw_line(Line2D l);
  void draw_string(String2D s);
  void imcanvas_start();
  void flush_rendering();
  void imcanvas_end();
  void render_stuff();

  // Gui State
  u32  viewport_x;
  u32  viewport_width;
  u32  viewport_y;
  u32  viewport_height;
  u32  screen_width;
  u32  screen_height;
  int2 old_mpos{};
  bool hovered;
  void consume_event(SDL_Event event);
};

struct Node;

struct Scene {
  Context2D c2d;
  u32  add_node(char const *name, char const *type, float x, float y, float size_x, float size_y);
  void draw();
  void consume_event(SDL_Event event);
  // called per frame
  void get_source_list(char const ***ptr, u32 *count);
  void get_node_type_list(char const ***ptr, u32 *count);
  // return long-lived reference
  char const *get_source(char const *name);
  // new_src: short-lived reference
  void          set_source(char const *name, char const *new_src);
  void          remove_source(char const *name);
  void          add_source(char const *name, char const *text);
  void          reset();
  void          run_script(char const *src_name);
  string_ref    get_save_script();
  void          push_warning(char const *fmt, ...);
  void          push_debug_message(char const *fmt, ...);
  void          push_error(char const *fmt, ...);
  static Scene *get_scene();
};

#define PUSH_WARNING(fmt, ...) Scene::get_scene()->push_warning(fmt, __VA_ARGS__)
#define PUSH_ERROR(fmt, ...) Scene::get_scene()->push_warning(fmt, __VA_ARGS__)
#define PUSH_DEBUG(fmt, ...) Scene::get_scene()->push_warning(fmt, __VA_ARGS__)
#define ASSERT_RETNULL(x)                                                                          \
  do {                                                                                             \
    if (!(x)) {                                                                                    \
      PUSH_WARNING("%s : returning 0", #x);                                                             \
      return 0;                                                                                    \
    }                                                                                              \
  } while (0)
#endif // NODE_EDITOR_H
