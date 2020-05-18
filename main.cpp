#include <stdio.h>
#include <stdlib.h>

#include "node_editor.h"
#include "utils.hpp"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <imgui.h>
#include <imgui/examples/imgui_impl_opengl3.h>
#include <imgui/examples/imgui_impl_sdl.h>

#ifndef __EMSCRIPTEN__
void MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                     const GLchar *message, const void *userParam) {
  fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
          (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
}
#endif

static int quit_loop = 0;

SDL_Window *               window = NULL;
SDL_GLContext              glc;
int                        SCREEN_WIDTH, SCREEN_HEIGHT;
static Temporary_Storage<> ts = Temporary_Storage<>::create(1 * (1 << 20));
#if __EMSCRIPTEN__
// int update_canvas_size_webgl(int eventType, const EmscriptenUiEvent *e, void *userData) {
//  double width, height;
//  emscripten_get_element_css_size("canvas", &width, &height);
//  emscripten_set_canvas_size(int(width), int(height));
//  SCREEN_WIDTH = int(width);
//  SCREEN_HEIGHT = int(height);
//  return 0;
//}
#endif

void Context2D::imcanvas_start() {
  ImGui::SetNextWindowBgAlpha(-1.0f);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  ImGui::Begin("Canvas");

  /*---------------------------------------*/
  /* Update the viewport for the rendering */
  /*---------------------------------------*/
  auto wpos         = ImGui::GetCursorScreenPos();
  viewport_x        = wpos.x;
  viewport_y        = wpos.y;
  auto wsize        = ImGui::GetWindowSize();
  viewport_width    = wsize.x;
  float height_diff = 24;
  if (wsize.y < height_diff + 2) {
    viewport_height = 2;
  } else {
    viewport_height = wsize.y - height_diff;
  }
  screen_height = SCREEN_HEIGHT;
  screen_width  = SCREEN_WIDTH;
  hovered       = ImGui::IsWindowHovered();
//  if (ImGui::IsWindowHovered()) {
//    float mx, my;
//    auto  mpos = ImGui::GetMousePos();
//    mx         = 2.0f * (float(mpos.x - viewport_x) + 0.5f) / viewport_width - 1.0f;
//    my         = -2.0f * (float(mpos.y - viewport_y) - 0.5f) / viewport_height + 1.0f;
//    auto dx    = mpos.x - old_mpos.x;
//    auto dy    = mpos.y - old_mpos.y;

//    if (ImGui::GetIO().MouseDown[0]) {
//      camera.pos.x -= camera.pos.z * (float)dx / viewport_height;
//      camera.pos.y += camera.pos.z * (float)dy / viewport_height;
//    }
//    camera.mouse_screen_x = mx;
//    camera.mouse_screen_y = my;

//    old_mpos      = int2(mpos.x, mpos.y);
//    auto scroll_y = ImGui::GetIO().MouseWheel;
//    if (scroll_y) {
//      float dz = camera.pos.z * (float)(scroll_y > 0 ? 1 : -1) * 2.0e-1;
//      //    fprintf(stdout, "dz: %i\n", event.wheel.y);
//      camera.pos.x += -0.5f * dz * (camera.window_to_screen((int2){(int32_t)old_mpos.x, 0}).x);
//      camera.pos.y += -0.5f * dz * (camera.window_to_screen((int2){0, (int32_t)old_mpos.y}).y);
//      camera.pos.z += dz;
//      camera.pos.z = clamp(camera.pos.z, 0.1f, 512.0f);
//    }
//  }
  camera.update(viewport_x, viewport_y, viewport_width, viewport_height);
  line_storage.enter_scope();
  quad_storage.enter_scope();
  string_storage.enter_scope();
  char_storage.enter_scope();
}
void Context2D::imcanvas_end() {
  ImGui::End();
  ImGui::PopStyleVar(3);
}

Scene scene;

#if __EMSCRIPTEN__
void main_tick() {
#else
int main_tick() {
#endif
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  SDL_Event event;
  i32       old_mp_x     = 0;
  i32       old_mp_y     = 0;
  auto      handle_event = [&]() {
    ImGui_ImplSDL2_ProcessEvent(&event);
    scene.consume_event(event);
  };
  static TextEditor editor;
  TextEditor::LanguageDefinition::CPlusPlus();
  editor.SetText(
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
  }
  )");

  auto poll_events = [&]() {
#if __EMSCRIPTEN__
    int fs;
    emscripten_get_canvas_size(&SCREEN_WIDTH, &SCREEN_HEIGHT, &fs);
    // EMSCRIPTEN_RESULT r = emscripten_get_canvas_element_size("#canvas", &w, &h);
    // SDL_GetWindowSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);
#else
    SDL_GetWindowSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);
#endif
    handle_event();
    while (SDL_PollEvent(&event)) {
      handle_event();
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window, SCREEN_WIDTH, SCREEN_HEIGHT);

    ImGui::NewFrame();

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport *  viewport     = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    window_flags |= ImGuiWindowFlags_NoBackground;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(-1.0f);
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
    scene.draw();
    ImGui::Begin("Text Editor");
    editor.Render("TextEditor");
    ImGui::End();
    bool show_demo_window = true;
    ImGui::ShowDemoWindow(&show_demo_window);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    scene.c2d.flush_rendering();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      SDL_Window *  backup_current_window  = SDL_GL_GetCurrentWindow();
      SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
    }

    SDL_UpdateWindowSurface(window);
    glFinish();
    SDL_GL_SwapWindow(window);
  };
#if __EMSCRIPTEN__
  poll_events();
#else

  while (SDL_WaitEvent(&event)) {
    poll_events();
  }

#endif

#if !__EMSCRIPTEN__
  return 0;
#endif
}

void main_loop() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
  //  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform
  //  Windows
  // io.ConfigViewportsNoAutoMerge = true;
  // io.ConfigViewportsNoTaskBarIcon = true;

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsClassic();

  // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look
  // identical to regular ones.
  ImGuiStyle &style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    style.WindowRounding              = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  // Setup Platform/Renderer bindings
  ImGui_ImplSDL2_InitForOpenGL(window, glc);
  const char *glsl_version = "#version 300 es";
  ImGui_ImplOpenGL3_Init(glsl_version);

#if __EMSCRIPTEN__
  emscripten_set_main_loop(main_tick, 0, true);
#else
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(MessageCallback, 0);
  main_tick();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
#endif
}

int main() {
  SDL_Init(SDL_INIT_VIDEO);

  window = SDL_CreateWindow("Gfx-Node", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 512, 512,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#if __EMSCRIPTEN__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetSwapInterval(1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  // emscripten_set_resize_callback(nullptr, nullptr, false, update_canvas_size_webgl);
#else
  // 3.2 is minimal requirement for renderdoc
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetSwapInterval(0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
#endif

  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  glc = SDL_GL_CreateContext(window);
  ASSERT_ALWAYS(glc);
  SDL_StartTextInput();
  main_loop();

  SDL_GL_DeleteContext(glc);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
// g++ ../test.cpp --std=c++14 -lSDL2 -lOpenGL && ./a.out
// emcc ../test.cpp -O2 -std=c++14 -s TOTAL_MEMORY=33554432 \
    -s USE_SDL_IMAGE=2 -s USE_SDL=2 -s USE_WEBGL2=1 \
    -s FULL_ES3=1 -o index.html && \
    python3 -m http.server
