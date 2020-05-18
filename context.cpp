#include "node_editor.h"
#include "simplefont.h"

static Temporary_Storage<> ts = Temporary_Storage<>::create(1 * (1 << 20));

static u16 f32_to_u16(f32 x) { return (u16)(clamp(x, 0.0f, 1.0f) * ((1 << 16) - 1)); }

float3 parse_color_float3(char const *str) {
  ASSERT_ALWAYS(str[0] == '#');
  auto hex_to_decimal = [](char c) {
    if (c >= '0' && c <= '9') {
      return (u32)c - (u32)'0';
    } else if (c >= 'a' && c <= 'f') {
      return 10 + (u32)c - (u32)'a';
    } else if (c >= 'A' && c <= 'F') {
      return 10 + (u32)c - (u32)'A';
    }
    UNIMPLEMENTED;
  };
  u32 r = hex_to_decimal(str[1]) * 16 + hex_to_decimal(str[2]);
  u32 g = hex_to_decimal(str[3]) * 16 + hex_to_decimal(str[4]);
  u32 b = hex_to_decimal(str[5]) * 16 + hex_to_decimal(str[6]);
  return (float3){(f32)r / 255.0f, (f32)g / 255.0f, (f32)b / 255.0f};
}

u32 parse_color_u32(char const *str) {
  ASSERT_ALWAYS(str[0] == '#');
  auto hex_to_decimal = [](char c) {
    if (c >= '0' && c <= '9') {
      return (u32)c - (u32)'0';
    } else if (c >= 'a' && c <= 'f') {
      return 10 + (u32)c - (u32)'a';
    } else if (c >= 'A' && c <= 'F') {
      return 10 + (u32)c - (u32)'A';
    }
    UNIMPLEMENTED;
  };
  u32 r = hex_to_decimal(str[1]) * 16 + hex_to_decimal(str[2]);
  u32 g = hex_to_decimal(str[3]) * 16 + hex_to_decimal(str[4]);
  u32 b = hex_to_decimal(str[5]) * 16 + hex_to_decimal(str[6]);
  return r | (g << 8) | (b << 16);
}

void Oth_Camera::update(u32 viewport_x, u32 viewport_y, u32 viewport_width, u32 viewport_height) {
  this->viewport_x      = viewport_x;
  this->viewport_width  = viewport_width;
  this->viewport_y      = viewport_y;
  this->viewport_height = viewport_height;
  height_over_width     = ((float)viewport_height / viewport_width);
  width_over_heigth     = ((float)viewport_width / viewport_height);
  float e               = (float)2.4e-7f;
  fovy                  = 2.0f;
  fovx                  = 2.0f * height_over_width;
  // clang-format off
      float4 proj[4] = {
        {fovx/pos.z,   0.0f,          0.0f,      -fovx * pos.x/pos.z},
        {0.0f,         fovy/pos.z,    0.0f,      -fovy * pos.y/pos.z},
        {0.0f,         0.0f,          1.0f,      0.0f},
        {0.0f,         0.0f,          0.0f,      1.0f}
      };
  // clang-format on
  memcpy(&this->proj[0], &proj[0], sizeof(proj));
  float2 mwp    = screen_to_world((float2){mouse_screen_x, mouse_screen_y});
  mouse_world_x = mwp.x;
  mouse_world_y = mwp.y;
  world_min_x   = screen_to_world((float2){-1.0f, -1.0f}).x;
  world_min_y   = screen_to_world((float2){-1.0f, -1.0f}).y;
  world_max_x   = screen_to_world((float2){1.0f, 1.0f}).x;
  world_max_y   = screen_to_world((float2){1.0f, 1.0f}).y;
  glyphs_world_height =
      glyph_scale * (float)(simplefont_bitmap_glyphs_height) / viewport_height * pos.z;
  glyphs_world_width =
      glyph_scale * (float)(simplefont_bitmap_glyphs_width) / viewport_width * pos.z;
  pixel_screen_width  = 2.0f / viewport_width;
  pixel_screen_height = 2.0f / viewport_height;
  glyphs_screen_width =
      2.0f * glyph_scale * (float)(simplefont_bitmap_glyphs_width) / viewport_width;
  glyphs_screen_height =
      2.0f * glyph_scale * (float)(simplefont_bitmap_glyphs_height) / viewport_height;
}

void compile_shader(GLuint shader) {
  glCompileShader(shader);
  GLint isCompiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
  if (isCompiled == GL_FALSE) {
    GLint maxLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
    GLchar *errorLog = (GLchar *)malloc(maxLength);
    defer(free(errorLog));
    glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

    glDeleteShader(shader);
    fprintf(stderr, "[ERROR]: %s\n", &errorLog[0]);
    exit(1);
  }
}

void link_program(GLuint program) {
  glLinkProgram(program);
  GLint isLinked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
  if (isLinked == GL_FALSE) {
    GLint maxLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
    GLchar *infoLog = (GLchar *)malloc(maxLength);
    defer(free(infoLog));
    glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
    glDeleteProgram(program);
    fprintf(stderr, "[ERROR]: %s\n", &infoLog[0]);
    exit(1);
  }
}

GLuint create_program(GLchar const *vsrc, GLchar const *fsrc) {
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vsrc, NULL);
  compile_shader(vertexShader);
  defer(glDeleteShader(vertexShader););
  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fsrc, NULL);
  compile_shader(fragmentShader);
  defer(glDeleteShader(fragmentShader););

  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  link_program(program);
  glDetachShader(program, vertexShader);
  glDetachShader(program, fragmentShader);

  return program;
}

void Oth_Camera::consume_event(SDL_Event event) {
  float       camera_speed  = 1.0f;
  static bool ldown         = false;
  static int  old_mp_x      = 0;
  static int  old_mp_y      = 0;
  static bool hyper_pressed = false;
  static bool skip_key      = false;
  static bool lctrl         = false;
  switch (event.type) {
  case SDL_QUIT: {
    break;
  }
  case SDL_KEYUP: {
    if (event.key.keysym.sym == SDLK_LCTRL) {
      lctrl = false;
    }
    break;
  }
  case SDL_TEXTINPUT: {
  } break;
  case SDL_KEYDOWN: {
    uint32_t c = event.key.keysym.sym;
    switch (event.key.keysym.sym) {

    case SDLK_ESCAPE: {
      break;
    }
    }
    break;
  }
  case SDL_MOUSEBUTTONDOWN: {
    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)&event;
    if (m->button == 3) {
    }
    if (m->button == 1) {

      ldown = true;
    }
    break;
  }
  case SDL_MOUSEBUTTONUP: {
    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)&event;
    if (m->button == 1) ldown = false;
    break;
  }
  case SDL_WINDOWEVENT_FOCUS_LOST: {
    skip_key      = false;
    hyper_pressed = false;
    ldown         = false;
    lctrl         = false;
    break;
  }
  case SDL_MOUSEMOTION: {
    SDL_MouseMotionEvent *m = (SDL_MouseMotionEvent *)&event;

    int dx = m->x - old_mp_x;
    int dy = m->y - old_mp_y;
    if (ldown) {
      pos.x -= pos.z * (float)dx / viewport_height;
      pos.y += pos.z * (float)dy / viewport_height;
    }
    mouse_screen_x = 2.0f * (float(m->x - viewport_x) + 0.5f) / viewport_width - 1.0f;
    mouse_screen_y = -2.0f * (float(m->y - viewport_y) - 0.5f) / viewport_height + 1.0f;

    old_mp_x = m->x;
    old_mp_y = m->y;
  } break;
  case SDL_MOUSEWHEEL: {
    float dz = pos.z * (float)(event.wheel.y > 0 ? 1 : -1) * 2.0e-1;
    //    fprintf(stdout, "dz: %i\n", event.wheel.y);
    pos.x += -0.5f * dz * (window_to_screen((int2){(int32_t)old_mp_x, 0}).x);
    pos.y += -0.5f * dz * (window_to_screen((int2){0, (int32_t)old_mp_y}).y);
    pos.z += dz;
    pos.z = clamp(pos.z, 0.1f, 512.0f);
  } break;
  }
}

void Context2D::consume_event(SDL_Event event) { camera.consume_event(event); }

void Scene::consume_event(SDL_Event event) {
  float       camera_speed  = 1.0f;
  static bool ldown         = false;
  static int  old_mp_x      = 0;
  static int  old_mp_y      = 0;
  static bool hyper_pressed = false;
  static bool skip_key      = false;
  static bool lctrl         = false;
  switch (event.type) {
  case SDL_QUIT: {
    break;
  }
  case SDL_KEYUP: {
    if (event.key.keysym.sym == SDLK_LCTRL) {
      lctrl = false;
    }
    break;
  }
  case SDL_TEXTINPUT: {
  } break;
  case SDL_KEYDOWN: {
    uint32_t c = event.key.keysym.sym;
    switch (event.key.keysym.sym) {

    case SDLK_ESCAPE: {
      break;
    }
    }
    break;
  }
  case SDL_MOUSEBUTTONDOWN: {
    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)&event;
    if (m->button == 3) {
    }
    if (m->button == 1) {

      ldown = true;
    }
    if (c2d.hovered) c2d.consume_event(event);
    break;
  }
  case SDL_MOUSEBUTTONUP: {
    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)&event;
    if (m->button == 1) ldown = false;
    if (c2d.hovered) c2d.consume_event(event);
    break;
  }
  case SDL_WINDOWEVENT_FOCUS_LOST: {
    skip_key      = false;
    hyper_pressed = false;
    ldown         = false;
    lctrl         = false;
    if (c2d.hovered) c2d.consume_event(event);
    break;
  }
  case SDL_MOUSEMOTION: {
    SDL_MouseMotionEvent *m = (SDL_MouseMotionEvent *)&event;

    int dx = m->x - old_mp_x;
    int dy = m->y - old_mp_y;

    old_mp_x = m->x;
    old_mp_y = m->y;
    if (c2d.hovered) c2d.consume_event(event);
  } break;
  case SDL_MOUSEWHEEL: {
    if (c2d.hovered) c2d.consume_event(event);
  } break;
  }
}

void Context2D::render_stuff() {
  glViewport((float)viewport_x, (float)(screen_height - viewport_y - viewport_height),
             (float)viewport_width, (float)viewport_height);
  glScissor(viewport_x, screen_height - viewport_y - viewport_height, viewport_width,
            viewport_height);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_SCISSOR_TEST);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_GEQUAL);
  glDisable(GL_CULL_FACE);
  float3 bg_color = parse_color_float3(dark_mode::g_background_color);
  glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
  glClearDepthf(0.0f);
  glLineWidth(1.0f);
  {
    const GLchar *line_vs =
        R"(#version 300 es
        precision highp float;
  layout (location = 0) in vec4 vertex_position;
  layout (location = 1) in vec3 vertex_color;
  uniform mat4 projection;
  out vec3 color;
  void main() {
      color = vertex_color;
      if (vertex_position.w > 0.0)
        gl_Position = vertex_position * projection;
      else
        gl_Position = vec4(vertex_position.xyz, 1.0);
  })";
    const GLchar *line_ps =
        R"(#version 300 es
        precision highp float;
  layout(location = 0) out vec4 SV_TARGET0;
  in vec3 color;
  void main() {
    SV_TARGET0 = vec4(color, 1.0);
  })";
    const GLchar *quad_vs =
        R"(#version 300 es
        precision highp float;
  layout (location = 0) in vec2 vertex_position;
  layout (location = 1) in vec4 instance_offset;
  layout (location = 2) in vec3 instance_color;
  layout (location = 3) in vec2 instance_size;

  out vec3 color;
  uniform mat4 projection;
  void main() {
      color = instance_color;
      if (instance_offset.w > 0.0)
        gl_Position =  vec4(vertex_position * instance_size + instance_offset.xy, instance_offset.z, 1.0) * projection;
      else
        gl_Position =  vec4(vertex_position * instance_size + instance_offset.xy, instance_offset.z, 1.0);
  })";
    const GLchar *quad_ps =
        R"(#version 300 es
        precision highp float;
  layout(location = 0) out vec4 SV_TARGET0;
  in vec3 color;
  void main() {
    SV_TARGET0 = vec4(color, 1.0);
  })";
    static GLuint line_program = create_program(line_vs, line_ps);
    static GLuint quad_program = create_program(quad_vs, quad_ps);
    static GLuint line_vao;
    static GLuint line_vbo;
    static GLuint quad_vao;
    static GLuint quad_vbo;
    static GLuint quad_instance_vbo;
    static int    init_va0 = [&] {
      glGenVertexArrays(1, &line_vao);
      glBindVertexArray(line_vao);
      glGenBuffers(1, &line_vbo);
      glBindBuffer(GL_ARRAY_BUFFER, line_vbo);

      glGenVertexArrays(1, &quad_vao);
      glBindVertexArray(quad_vao);
      float pos[] = {
          0.0f, 0.0f, //
          1.0f, 0.0f, //
          1.0f, 1.0f, //
          0.0f, 0.0f, //
          1.0f, 1.0f, //
          0.0f, 1.0f, //
      };
      glGenBuffers(1, &quad_vbo);
      glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_DYNAMIC_DRAW);

      glGenBuffers(1, &quad_instance_vbo);
      glBindBuffer(GL_ARRAY_BUFFER, quad_instance_vbo);

      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);

      return 0;
    }();
    // Draw quads
    if (quad_storage.cursor != 0) {
      glUseProgram(quad_program);
      glUniformMatrix4fv(glGetUniformLocation(quad_program, "projection"), 1, GL_FALSE,
                         (float *)&camera.proj[0]);
      glBindVertexArray(quad_vao);
      glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
      glEnableVertexAttribArray(0);
      // glVertexAttribBinding(0, 0);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

      glBindBuffer(GL_ARRAY_BUFFER, quad_instance_vbo);
      struct Rect_Instance_GL {
        float x, y, z, w;
        float r, g, b;
        float width, height;
      };
      static_assert(sizeof(Rect_Instance_GL) == 36, "");
      //      uint32_t max_num_quads = width * height;
      uint32_t max_num_quads = quad_storage.cursor;
      uint32_t num_quads     = 0;
      ts.enter_scope();
      defer(ts.exit_scope());
      Rect_Instance_GL *qinstances =
          (Rect_Instance_GL *)ts.alloc(sizeof(Rect_Instance_GL) * max_num_quads);
      ito(max_num_quads) {
        Rect2D quad2d = *quad_storage.at(i);
        if (quad2d.world_space && !camera.intersects(quad2d.x, quad2d.y, quad2d.x + quad2d.width,
                                                     quad2d.y + quad2d.height))
          continue;
        Rect_Instance_GL quadgl;
        if (quad2d.world_space) {
          quadgl.x      = quad2d.x;
          quadgl.y      = quad2d.y;
          quadgl.z      = quad2d.z;
          quadgl.w      = 1.0f;
          quadgl.width  = quad2d.width;
          quadgl.height = quad2d.height;
        } else {
          quadgl.x      = 2.0f * quad2d.x / viewport_width - 1.0f;
          quadgl.y      = -2.0f * quad2d.y / viewport_height + 1.0f;
          quadgl.z      = quad2d.z;
          quadgl.w      = 0.0f;
          quadgl.width  = 2.0f * quad2d.width / viewport_width;
          quadgl.height = -2.0f * quad2d.height / viewport_height;
        }

        quadgl.r                = quad2d.color.r;
        quadgl.g                = quad2d.color.g;
        quadgl.b                = quad2d.color.b;
        qinstances[num_quads++] = quadgl;
      }
      if (num_quads == 0) goto skip_quads;
      glBufferData(GL_ARRAY_BUFFER, sizeof(Rect_Instance_GL) * num_quads, qinstances,
                   GL_DYNAMIC_DRAW);

      glEnableVertexAttribArray(1);
      // glVertexAttribBinding(1, 0);
      glVertexAttribDivisor(1, 1);
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Rect_Instance_GL), 0);
      glEnableVertexAttribArray(2);
      // glVertexAttribBinding(2, 0);
      glVertexAttribDivisor(2, 1);
      glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Rect_Instance_GL), (void *)16);
      glEnableVertexAttribArray(3);
      // glVertexAttribBinding(3, 0);
      glVertexAttribDivisor(3, 1);
      glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Rect_Instance_GL), (void *)28);

      glDrawArraysInstanced(GL_TRIANGLES, 0, 6, num_quads);

      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
      glDisableVertexAttribArray(2);
      glVertexAttribDivisor(1, 0);
      glVertexAttribDivisor(2, 0);
    }
  skip_quads:
    // Draw lines
    if (line_storage.cursor != 0) {
      uint32_t num_lines = line_storage.cursor;
      struct Line_GL {
        float x0, y0, z0, w0;
        float r0, g0, b0;
        float x1, y1, z1, w1;
        float r1, g1, b1;
      };
      static_assert(sizeof(Line_GL) == 56, "");
      ts.enter_scope();
      defer(ts.exit_scope());
      Line_GL *lines = (Line_GL *)ts.alloc(sizeof(Line_GL) * num_lines);
      ito(num_lines) {
        Line2D  l = *line_storage.at(i);
        Line_GL lgl;
        if (l.world_space) {
          lgl.x0 = l.x0;
          lgl.y0 = l.y0;
          lgl.z0 = l.z;
          lgl.w0 = 1.0f;
          lgl.r0 = l.color.r;
          lgl.g0 = l.color.g;
          lgl.b0 = l.color.b;
          lgl.x1 = l.x1;
          lgl.y1 = l.y1;
          lgl.z1 = l.z;
          lgl.w1 = 1.0f;
        } else {
          lgl.x0 = 2.0f * l.x0 / viewport_width - 1.0f;
          lgl.y0 = -2.0f * l.y0 / viewport_height + 1.0f;
          lgl.z0 = l.z;
          lgl.w0 = 0.0f;
          lgl.r0 = l.color.r;
          lgl.g0 = l.color.g;
          lgl.b0 = l.color.b;
          lgl.x1 = 2.0f * l.x1 / viewport_width - 1.0f;
          lgl.y1 = -2.0f * l.y1 / viewport_height + 1.0f;
          lgl.z1 = l.z;
          lgl.w1 = 0.0f;
        }

        lgl.r1   = l.color.r;
        lgl.g1   = l.color.g;
        lgl.b1   = l.color.b;
        lines[i] = lgl;
      }
      glBindVertexArray(line_vao);
      glBindBuffer(GL_ARRAY_BUFFER, line_vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(Line_GL) * num_lines, lines, GL_DYNAMIC_DRAW);
      glUseProgram(line_program);
      glUniformMatrix4fv(glGetUniformLocation(line_program, "projection"), 1, GL_FALSE,
                         (float *)&camera.proj[0]);

      glEnableVertexAttribArray(0);
      // glVertexAttribBinding(0, 0);
      glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 28, 0);
      glEnableVertexAttribArray(1);
      // glVertexAttribBinding(1, 0);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 28, (void *)16);
      glDrawArrays(GL_LINES, 0, 2 * num_lines);

      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
    }
    // Draw strings
    if (string_storage.cursor != 0) {
      const GLchar *vsrc =
          R"(#version 300 es
          precision highp float;
  layout (location=0) in vec2 vertex_position;
  layout (location=1) in vec3 instance_offset;
  layout (location=2) in vec2 instance_uv_offset;
  layout (location=3) in vec3 instance_color;

  out vec2 uv;
  out vec3 color;

  uniform vec2 glyph_uv_size;
  uniform vec2 glyph_size;
  uniform vec2 viewport_size;
  void main() {
      color = instance_color;
      uv = instance_uv_offset + (vertex_position * vec2(1.0, -1.0) + vec2(0.0, 1.0)) * glyph_uv_size;
      vec4 sspos =  vec4(vertex_position * glyph_size + instance_offset.xy, 0.0, 1.0);
      int pixel_x = int(viewport_size.x * (sspos.x * 0.5 + 0.5) + 0.5);
      int pixel_y = int(viewport_size.y * (sspos.y * 0.5 + 0.5) + 0.5);
      sspos.x = 2.0 * float(pixel_x) / viewport_size.x - 1.0;
      sspos.y = 2.0 * float(pixel_y) / viewport_size.y - 1.0;
      sspos.z = instance_offset.z;
      gl_Position = sspos;

  })";
      const GLchar *fsrc =
          R"(#version 300 es
          precision highp float;
  layout(location = 0) out vec4 SV_TARGET0;

  in vec2 uv;
  in vec3 color;

  uniform sampler2D image;
  void main() {
    if (texture(image, uv).x > 0.0)
      SV_TARGET0 = vec4(color, 1.0);
    else
      discard;
  })";
      static GLuint program      = create_program(vsrc, fsrc);
      static GLuint font_texture = [&] {
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        uint8_t *r8_data = (uint8_t *)malloc(simplefont_bitmap_height * simplefont_bitmap_width);
        defer(free(r8_data));
        ito(simplefont_bitmap_height) {
          jto(simplefont_bitmap_width) {
            char c                                   = simplefont_bitmap[i][j];
            r8_data[(i)*simplefont_bitmap_width + j] = c == ' ' ? 0 : 0xff;
          }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, simplefont_bitmap_width, simplefont_bitmap_height, 0,
                     GL_RED, GL_UNSIGNED_BYTE, r8_data);
        return tex;
      }();
      static GLuint vao;
      static GLuint vbo;
      static GLuint instance_vbo;
      static int    glyph_vao = [&] {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        float pos[] = {
            0.0f, 0.0f, //
            1.0f, 0.0f, //
            1.0f, 1.0f, //
            0.0f, 0.0f, //
            1.0f, 1.0f, //
            0.0f, 1.0f, //
        };
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_DYNAMIC_DRAW);
        glGenBuffers(1, &instance_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, instance_vbo);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return 0;
      }();
      struct Glyph_Instance_GL {
        float x, y, z;
        float u, v;
        float r, g, b;
      };
      static_assert(sizeof(Glyph_Instance_GL) == 32, "");
      uint32_t glyph_scale     = 2;
      float    glyph_uv_width  = (float)simplefont_bitmap_glyphs_width / simplefont_bitmap_width;
      float    glyph_uv_height = (float)simplefont_bitmap_glyphs_height / simplefont_bitmap_height;

      float      glyph_pad_ss   = 2.0f / viewport_width;
      uint32_t   max_num_glyphs = 0;
      uint32_t   num_strings    = string_storage.cursor;
      _String2D *strings        = string_storage.at(0);
      kto(num_strings) { max_num_glyphs += (uint32_t)strings[k].len; }
      ts.enter_scope();
      defer(ts.exit_scope());
      Glyph_Instance_GL *glyphs_gl =
          (Glyph_Instance_GL *)ts.alloc(sizeof(Glyph_Instance_GL) * max_num_glyphs);
      uint32_t num_glyphs = 0;

      kto(num_strings) {
        _String2D string = strings[k];
        if (string.len == 0) continue;
        float2 ss = (float2){string.x, string.y};
        if (string.world_space) {
          ss = camera.world_to_screen(ss);
        } else {
          ss = camera.window_to_screen((int2){(int32_t)ss.x, (int32_t)ss.y});
        }
        float min_ss_x = ss.x;
        float min_ss_y = ss.y;
        float max_ss_x = ss.x + (camera.glyphs_screen_width + glyph_pad_ss) * string.len;
        float max_ss_y = ss.y + camera.glyphs_screen_height;
        if (min_ss_x > 1.0f || min_ss_y > 1.0f || max_ss_x < -1.0f || max_ss_y < -1.0f) continue;

        ito(string.len) {
          uint32_t c = (uint32_t)string.c_str[i];

          // Printable characters only
          c            = clamp(c, 0x20u, 0x7eu);
          uint32_t row = (c - 0x20) / simplefont_bitmap_glyphs_per_row;
          uint32_t col = (c - 0x20) % simplefont_bitmap_glyphs_per_row;
          float    v0 =
              ((float)row * (simplefont_bitmap_glyphs_height + simplefont_bitmap_glyphs_pad_y * 2) +
               simplefont_bitmap_glyphs_pad_y) /
              simplefont_bitmap_height;
          float u0 =
              ((float)col * (simplefont_bitmap_glyphs_width + simplefont_bitmap_glyphs_pad_x * 2) +
               simplefont_bitmap_glyphs_pad_x) /
              simplefont_bitmap_width;
          Glyph_Instance_GL glyph;
          glyph.u                 = u0;
          glyph.v                 = v0;
          glyph.x                 = ss.x + (camera.glyphs_screen_width + glyph_pad_ss) * i;
          glyph.y                 = ss.y;
          glyph.z                 = string.z;
          glyph.r                 = string.color.r;
          glyph.g                 = string.color.g;
          glyph.b                 = string.color.b;
          glyphs_gl[num_glyphs++] = glyph;
        }
      }
      if (num_glyphs == 0) goto skip_strings;
      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glEnableVertexAttribArray(0);
      // glVertexAttribBinding(0, 0);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
      glBindBuffer(GL_ARRAY_BUFFER, instance_vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(Glyph_Instance_GL) * num_glyphs, glyphs_gl,
                   GL_DYNAMIC_DRAW);
      glEnableVertexAttribArray(1);
      // glVertexAttribBinding(1, 0);
      glVertexAttribDivisor(1, 1);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void *)0);
      glEnableVertexAttribArray(2);
      // glVertexAttribBinding(2, 0);
      glVertexAttribDivisor(2, 1);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void *)12);
      glEnableVertexAttribArray(3);
      // glVertexAttribBinding(3, 0);
      glVertexAttribDivisor(3, 1);
      glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 32, (void *)20);
      // Draw

      glUseProgram(program);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, font_texture);
      glUniform1i(glGetUniformLocation(program, "image"), 0);
      //      glUniformMatrix4fv(glGetUniformLocation(line_program,
      //      "projection"), 1,
      //                         GL_FALSE, (float *)&camera.proj[0]);
      glUniform2f(glGetUniformLocation(program, "viewport_size"), (float)viewport_width,
                  (float)viewport_height);
      glUniform2f(glGetUniformLocation(program, "glyph_size"), camera.glyphs_screen_width,
                  camera.glyphs_screen_height);
      glUniform2f(glGetUniformLocation(program, "glyph_uv_size"), glyph_uv_width, glyph_uv_height);

      glDrawArraysInstanced(GL_TRIANGLES, 0, 6, num_glyphs);
      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
    }
  skip_strings:
    (void)0;
  }
}

// void compile_shader(GLuint shader) {
//  glCompileShader(shader);
//  GLint isCompiled = 0;
//  glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
//  if (isCompiled == GL_FALSE) {
//    GLint maxLength = 0;
//    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

//    // The maxLength includes the NULL character
//    std::vector<GLchar> errorLog(maxLength);
//    glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

//    glDeleteShader(shader);
//    fprintf(stderr, "[ERROR]: %s\n", &errorLog[0]);
//    exit(1);
//  }
//}

// void link_program(GLuint program) {
//  glLinkProgram(program);
//  GLint isLinked = 0;
//  glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
//  if (isLinked == GL_FALSE) {
//    GLint maxLength = 0;
//    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

//    // The maxLength includes the NULL character
//    std::vector<GLchar> infoLog(maxLength);
//    glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

//    // The program is useless now. So delete it.
//    glDeleteProgram(program);
//    fprintf(stderr, "[ERROR]: %s\n", &infoLog[0]);
//    exit(1);
//  }
//}

GLuint initShader() {
  const GLchar *vertexSource =
      R"(#version 300 es
  precision highp float;
  layout (location=0) in vec3 position;
  out vec3 color;
  void main() {
      color = position;
      gl_Position = vec4(position, 1.0);

  })";

  // Fragment/pixel shader
  const GLchar *fragmentSource =
      R"(#version 300 es
  precision highp float;
  in vec3 color;
  layout(location = 0) out vec4 SV_TARGET0;

  void main() {
    SV_TARGET0 = vec4(color, 1.0);
  })";
  // Create and compile vertex shader
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexSource, NULL);
  compile_shader(vertexShader);

  // Create and compile fragment shader
  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
  compile_shader(fragmentShader);

  // Link vertex and fragment shader into shader program and use it
  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  link_program(program);
  glDetachShader(program, vertexShader);
  glDetachShader(program, fragmentShader);
  glUseProgram(program);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  return program;
}

GLuint initGeometry(GLuint program) {
  // Create vertex buffer object and copy vertex data into it
  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  GLfloat vertices[] = {0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f};
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  // Specify the layout of the shader vertex data (positions only, 3 floats)
  GLint posAttrib = glGetAttribLocation(program, "position");
  glEnableVertexAttribArray(posAttrib);
  glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glBindVertexArray(0);
  return vao;
}

void redraw() {
  GLuint program = initShader();
  GLuint vao     = initGeometry(program);
  glUseProgram(program);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glDeleteProgram(program);
  glDeleteVertexArrays(1, &vao);
}

void Scene::draw() {
  ts.enter_scope();
  c2d.imcanvas_start();
  defer({
    c2d.imcanvas_end();
    ts.exit_scope();
  });
  u32    W          = 256;
  u32    H          = 256;
  float  dx         = 1.0f;
  float  dy         = 1.0f;
  float  size_x     = 256.0f;
  float  size_y     = 256.0f;
  float  QUAD_LAYER = 1.0f / 256.0f;
  float  GRID_LAYER = 2.0f / 256.0f;
  float  TEXT_LAYER = 3.0f / 256.0f;
  float3 grid_color = parse_color_float3(dark_mode::g_grid_color);
  c2d.draw_string({//
                   .c_str = "hello world!",
                   .x     = c2d.camera.mouse_world_x,
                   .y     = c2d.camera.mouse_world_y,
                   .z     = GRID_LAYER,
                   .color = {.r = 1.0f, .g = 1.0f, .b = 1.0f}});
  c2d.draw_rect({//
                 .x      = c2d.camera.mouse_world_x,
                 .y      = c2d.camera.mouse_world_y,
                 .z      = GRID_LAYER,
                 .width  = 1.0f,
                 .height = 1.0f,

                 .color = {.r = grid_color.x, .g = grid_color.y, .b = grid_color.z}});
  ito(W + 1) {
    c2d.draw_line({//
                   .x0    = dx * (float)(i),
                   .y0    = 0.0f,
                   .x1    = dx * (float)(i),
                   .y1    = size_y,
                   .z     = GRID_LAYER,
                   .color = {.r = grid_color.x, .g = grid_color.y, .b = grid_color.z}});
  }
  ito(H + 1) {
    c2d.draw_line({//
                   .x0    = 0.0f,
                   .y0    = dy * (float)(i),
                   .x1    = size_x,
                   .y1    = dy * (float)(i),
                   .z     = GRID_LAYER,
                   .color = {.r = grid_color.x, .g = grid_color.y, .b = grid_color.z}});
  }
}

// struct Console {
//    char     buffer[0x100][0x100];
//    uint32_t column    = 0;
//    uint32_t scroll_id = 0;
//    Console() { memset(this, 0, sizeof(*this)); }
//    void unscroll() {
//      if (scroll_id != 0) {
//        memcpy(&buffer[0][0], &buffer[scroll_id][0], 0x100);
//        scroll_id = 0;
//      }
//    }
//    void backspace() {
//      unscroll();
//      if (column > 0) {
//        ito(0x100 - column) { buffer[0][column + i - 1] = buffer[0][column + i]; }
//        column--;
//      }
//    }
//    void newline() {
//      unscroll();
//      ito(0x100 - 1) { memcpy(&buffer[0x100 - 1 - i][0], &buffer[0x100 - 2 - i][0], 0x100); }
//      memset(&buffer[0][0], 0, 0x100);
//      column = 0;
//    }
//    void cursor_right() {
//      unscroll();
//      if (buffer[0][column] != '\0') column++;
//    }
//    void cursor_left() {
//      unscroll();
//      if (column > 0) column--;
//    }
//    void put_line(char const *str) {
//      unscroll();
//      while (str[0] != '\0') {
//        put_char(str[0]);
//        str++;
//      }
//      newline();
//    }
//    void put_fmt(char const *fmt, ...) {
//      char    buffer[0x100];
//      va_list args;
//      va_start(args, fmt);
//      vsnprintf(buffer, sizeof(buffer), fmt, args);
//      va_end(args);
//      put_line(buffer);
//    }
//    void put_char(char c) {
//      unscroll();
//      if (c >= 0x20 && c <= 0x7e && column < 0x100 - 1) {
//        ito(0x100 - column - 1) { buffer[0][0x100 - i - 1] = buffer[0][0x100 - i - 2]; }
//        buffer[0][column++] = c;
//      }
//    }
//    void scroll_up() {
//      if (scroll_id < 0x100) {
//        scroll_id++;
//        column = strlen(buffer[scroll_id]);
//      }
//    }
//    void scroll_down() {
//      if (scroll_id > 0) {
//        scroll_id--;
//        column = strlen(buffer[scroll_id]);
//      }
//    }
//  } console;
//  bool console_mode = false;
//  void draw_console() {
//    float    CONSOLE_TEXT_LAYER       = 102.0f / 256.0f;
//    float    CONSOLE_CURSOR_LAYER     = 101.0f / 256.0f;
//    float    CONSOLE_BACKGROUND_LAYER = 100.0f / 256.0f;
//    float    GLYPH_HEIGHT             = camera.glyph_scale * simplefont_bitmap_glyphs_height;
//    float    GLYPH_WIDTH              = camera.glyph_scale * simplefont_bitmap_glyphs_width;
//    uint32_t console_lines            = 8;
//    float    console_bottom           = (GLYPH_HEIGHT + 1) * console_lines;
//    ito(console_lines - 1) {
//      draw_string({.c_str       = console.buffer[console_lines - 1 - i],
//                   .x           = 0,
//                   .y           = (GLYPH_HEIGHT + 1) * (i + 1),
//                   .z           = CONSOLE_TEXT_LAYER,
//                   .color       = {.r = 0.0f, .g = 0.0f, .b = 0.0f},
//                   .world_space = false});
//    }
//    draw_string({.c_str       = console.buffer[console.scroll_id],
//                 .x           = 0.0f,
//                 .y           = console_bottom,
//                 .z           = CONSOLE_TEXT_LAYER,
//                 .color       = {.r = 0.0f, .g = 0.0f, .b = 0.0f},
//                 .world_space = false});
//    draw_rect({//
//               .x           = 0.0f,
//               .y           = 0.0f,
//               .z           = CONSOLE_BACKGROUND_LAYER,
//               .width       = (float)camera.viewport_width,
//               .height      = console_bottom + 1.0f,
//               .color       = {.r = 0.8f, .g = 0.8f, .b = 0.8f},
//               .world_space = false});
//    draw_line({//
//               .x0          = 0.0f,
//               .y0          = console_bottom + 2.0f,
//               .x1          = (float)camera.viewport_width,
//               .y1          = console_bottom + 2.0f,
//               .z           = CONSOLE_CURSOR_LAYER,
//               .color       = {.r = 0.0f, .g = 0.0f, .b = 0.0f},
//               .world_space = false});
//    draw_rect({//
//               .x           = console.column * (GLYPH_WIDTH + 1.0f),
//               .y           = console_bottom,
//               .z           = CONSOLE_CURSOR_LAYER,
//               .width       = GLYPH_WIDTH,
//               .height      = -GLYPH_HEIGHT,
//               .color       = {.r = 1.0f, .g = 1.0f, .b = 1.0f},
//               .world_space = false});
//  }
