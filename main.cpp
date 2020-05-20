#include <stdio.h>
#include <stdlib.h>

#include "node_editor.h"
#define UTILS_IMPL
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
void dump_scene() {
  TMP_STORAGE_SCOPE;
  string_ref dump = Scene::get_scene()->get_save_script();
  dump_file("scene.lsp", dump.ptr, dump.len);
}
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
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
  ImGui::Begin("Canvas");
  ImGui::PopStyleVar(4);
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

  //  static int  selected_fish = -1;
  //  const char *names[]       = {"Bream", "Haddock", "Mackerel", "Pollock", "Tilefish"};
  //  static bool toggles[]     = {true, false, false, false, false};

  if (ImGui::BeginPopupContextWindow()) {
    //    for (int i = 0; i < IM_ARRAYSIZE(names); i++) ImGui::MenuItem(names[i], "", &toggles[i]);
    //    if (ImGui::BeginMenu("Sub-menu")) {
    //      ImGui::MenuItem("Click me");
    //      ImGui::EndMenu();
    //    }
    if (ImGui::Button("Add node")) ImGui::OpenPopup("add_node_popup");
    {
      if (ImGui::BeginPopup("add_node_popup")) {
        char const **ptr   = NULL;
        u32          count = 0;
        Scene::get_scene()->get_node_type_list(&ptr, &count);
        ito(count) {
          if (ImGui::MenuItem(ptr[i])) {
            Scene::get_scene()->add_node("new node #", ptr[i],
                                         Scene::get_scene()->c2d.camera.mouse_world_x,
                                         Scene::get_scene()->c2d.camera.mouse_world_y, 1.0f, 1.0f);
          }
        }
        ImGui::EndPopup();
      }
    }
    if (ImGui::Button("Save scene")) {
      dump_scene();
    }
    if (ImGui::Button("Exit")) std::exit(0);
    // ImGui::Separator();
    //    ImGui::Text("Tooltip here");
    //    if (ImGui::IsItemHovered()) ImGui::SetTooltip("I am a tooltip over a popup");

    //    if (ImGui::Button("Stacked Popup")) ImGui::OpenPopup("another popup");
    //    if (ImGui::BeginPopup("another popup")) {
    //      for (int i = 0; i < IM_ARRAYSIZE(names); i++) ImGui::MenuItem(names[i], "",
    //      &toggles[i]); if (ImGui::BeginMenu("Sub-menu")) {
    //        ImGui::MenuItem("Click me");
    //        if (ImGui::Button("Stacked Popup")) ImGui::OpenPopup("another popup");
    //        if (ImGui::BeginPopup("another popup")) {
    //          ImGui::Text("I am the last one here.");
    //          ImGui::EndPopup();
    //        }
    //        ImGui::EndMenu();
    //      }
    //      ImGui::EndPopup();
    //    }
    ImGui::EndPopup();
  }
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
}
void Context2D::imcanvas_end() { ImGui::End(); }

// Usage:
//  static ExampleAppLog my_log;
//  my_log.AddLog("Hello %d world\n", 123);
//  my_log.Draw("title");
struct ExampleAppLog {
  ImGuiTextBuffer Buf;
  ImGuiTextFilter Filter;
  ImVector<int>   LineOffsets; // Index to lines offset. We maintain this with AddLog() calls.
  bool            AutoScroll;  // Keep scrolling if already at the bottom.

  ExampleAppLog() {
    AutoScroll = true;
    Clear();
  }

  void Clear() {
    Buf.clear();
    LineOffsets.clear();
    LineOffsets.push_back(0);
  }

  void AddLog(const char *fmt, ...) IM_FMTARGS(2) {
    int     old_size = Buf.size();
    va_list args;
    va_start(args, fmt);
    Buf.appendfv(fmt, args);
    va_end(args);
    for (int new_size = Buf.size(); old_size < new_size; old_size++)
      if (Buf[old_size] == '\n') LineOffsets.push_back(old_size + 1);
  }

  void AddLog(const char *fmt, va_list args) {
    int old_size = Buf.size();
    Buf.appendfv(fmt, args);
    for (int new_size = Buf.size(); old_size < new_size; old_size++)
      if (Buf[old_size] == '\n') LineOffsets.push_back(old_size + 1);
  }

  void Draw(const char *title, bool *p_open = NULL) {
    if (!ImGui::Begin(title, p_open)) {
      ImGui::End();
      return;
    }

    // Options menu
    if (ImGui::BeginPopup("Options")) {
      ImGui::Checkbox("Auto-scroll", &AutoScroll);
      ImGui::EndPopup();
    }

    // Main window
    if (ImGui::Button("Options")) ImGui::OpenPopup("Options");
    ImGui::SameLine();
    bool clear = ImGui::Button("Clear");
    ImGui::SameLine();
    bool copy = ImGui::Button("Copy");
    ImGui::SameLine();
    Filter.Draw("Filter", -100.0f);

    ImGui::Separator();
    ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (clear) Clear();
    if (copy) ImGui::LogToClipboard();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char *buf     = Buf.begin();
    const char *buf_end = Buf.end();
    if (Filter.IsActive()) {
      // In this example we don't use the clipper when Filter is enabled.
      // This is because we don't have a random access on the result on our filter.
      // A real application processing logs with ten of thousands of entries may want to store the
      // result of search/filter.. especially if the filtering function is not trivial (e.g.
      // reg-exp).
      for (int line_no = 0; line_no < LineOffsets.Size; line_no++) {
        const char *line_start = buf + LineOffsets[line_no];
        const char *line_end =
            (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
        if (Filter.PassFilter(line_start, line_end)) ImGui::TextUnformatted(line_start, line_end);
      }
    } else {
      // The simplest and easy way to display the entire buffer:
      //   ImGui::TextUnformatted(buf_begin, buf_end);
      // And it'll just work. TextUnformatted() has specialization for large blob of text and will
      // fast-forward to skip non-visible lines. Here we instead demonstrate using the clipper to
      // only process lines that are within the visible area. If you have tens of thousands of items
      // and their processing cost is non-negligible, coarse clipping them on your side is
      // recommended. Using ImGuiListClipper requires
      // - A) random access into your data
      // - B) items all being the  same height,
      // both of which we can handle since we an array pointing to the beginning of each line of
      // text. When using the filter (in the block of code above) we don't have random access into
      // the data to display anymore, which is why we don't use the clipper. Storing or skimming
      // through the search result would make it possible (and would be recommended if you want to
      // search through tens of thousands of entries).
      ImGuiListClipper clipper;
      clipper.Begin(LineOffsets.Size);
      while (clipper.Step()) {
        for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++) {
          const char *line_start = buf + LineOffsets[line_no];
          const char *line_end =
              (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
          ImGui::TextUnformatted(line_start, line_end);
        }
      }
      clipper.End();
    }
    ImGui::PopStyleVar();

    if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
  }
};
ExampleAppLog debug_log;
void          Scene::push_warning(char const *fmt, ...) {
  debug_log.AddLog("[WARNING] ");
  va_list args;
  va_start(args, fmt);
  debug_log.AddLog(fmt, args);
  va_end(args);
  debug_log.AddLog("\n");
}
void Scene::push_debug_message(char const *fmt, ...) {
  debug_log.AddLog("[DEBUG] ");
  va_list args;
  va_start(args, fmt);
  debug_log.AddLog(fmt, args);
  va_end(args);
  debug_log.AddLog("\n");
}
void Scene::push_error(char const *fmt, ...) {
  debug_log.AddLog("[ERROR] ");
  va_list args;
  va_start(args, fmt);
  debug_log.AddLog(fmt, args);
  va_end(args);
  debug_log.AddLog("\n");
}
TextEditor editor;
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
    Scene::get_scene()->consume_event(event);
  };

  auto poll_events = [&]() {
#if __EMSCRIPTEN__
    int fs;
    emscripten_get_canvas_size(&SCREEN_WIDTH, &SCREEN_HEIGHT, &fs);
    // EMSCRIPTEN_RESULT r = emscripten_get_canvas_element_size("#canvas", &w, &h);
    // SDL_GetWindowSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);
#else
    SDL_GetWindowSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);
    handle_event();
#endif
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
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
    ImGui::SetNextWindowBgAlpha(-1.0f);
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(4);
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
    Scene::get_scene()->draw();
    ImGui::Begin("Scene");
    if (ImGui::Button("Save scene")) {
      dump_scene();
    }
    ImGui::End();

    ImGui::Begin("Log");
    debug_log.Draw("Log");
    ImGui::End();

    ImGui::Begin("Text Editor");
    {
      TMP_STORAGE_SCOPE;
      char const **ptr     = NULL;
      u32          count   = 0;
      static i32   current = -1;
      static char  current_name[0x20];

      Scene::get_scene()->get_source_list(&ptr, &count);
      if (ImGui::Combo("Source", &current, ptr, (i32)count)) {
        editor.SetText(Scene::get_scene()->get_source(ptr[current]));
        memcpy(current_name, ptr[current], sizeof(current_name));
        current_name[sizeof(current_name) - 1] = '\0';
      }
      if (current >= 0) {
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
          Scene::get_scene()->set_source(current_name, editor.GetText().c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Run")) {
          Scene::get_scene()->set_source(current_name, editor.GetText().c_str());
          Scene::get_scene()->run_script(current_name);
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("New")) {
        ImGui::OpenPopup("new_source_popup");
      }
      if (ImGui::BeginPopup("new_source_popup")) {
        static char buf[0x20] = {};
        ImGui::InputText("Name", buf, IM_ARRAYSIZE(buf));
        if (ImGui::MenuItem("Create")) {
          if (strnlen(buf, sizeof(buf)) > 0) {
            Scene::get_scene()->add_source(buf, "");
          }
        }
        if (ImGui::MenuItem("Cancel")) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }
    editor.Render("TextEditor");
    ImGui::End();
    bool show_demo_window = true;
    ImGui::ShowDemoWindow(&show_demo_window);

    ImGui::Render();
    Scene::get_scene()->c2d.flush_rendering();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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
  ImGui::GetStyle().FrameRounding = 4.0f;
  ImGui::GetStyle().GrabRounding  = 4.0f;

  ImVec4 *colors                         = ImGui::GetStyle().Colors;
  colors[ImGuiCol_Text]                  = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
  colors[ImGuiCol_TextDisabled]          = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
  colors[ImGuiCol_WindowBg]              = ImVec4(0.11f, 0.15f, 0.17f, 0.00f);
  colors[ImGuiCol_ChildBg]               = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
  colors[ImGuiCol_Border]                = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
  colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
  colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
  colors[ImGuiCol_FrameBgActive]         = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
  colors[ImGuiCol_TitleBg]               = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
  colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
  colors[ImGuiCol_MenuBarBg]             = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
  colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
  colors[ImGuiCol_CheckMark]             = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrab]            = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
  colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
  colors[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
  colors[ImGuiCol_ButtonActive]          = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
  colors[ImGuiCol_Header]                = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
  colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
  colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
  colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
  colors[ImGuiCol_SeparatorActive]       = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
  colors[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
  colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
  colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
  colors[ImGuiCol_Tab]                   = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
  colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
  colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
  colors[ImGuiCol_TabUnfocused]          = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
  colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
  colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
  colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
  colors[ImGuiCol_DragDropTarget]        = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
  colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
  colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
  ImGuiIO &io                            = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
  //  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform
  //  Windows
  // io.ConfigViewportsNoAutoMerge = true;
  // io.ConfigViewportsNoTaskBarIcon = true;

  // Setup Dear ImGui style
  //  ImGui::StyleColorsDark();
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
#if __EMSCRIPTEN__
  {
    char const *source = R"(
    (main
      (add_node "new node #" "Gfx/DrawCall" -6.409513 27.993538 1.000000 1.000000)
      (add_node "new node 2" "Gfx/DrawCall" -12.458076 14.947708 1.000000 1.000000)
      (move_camera -1.915402 9.350576 48.028961)
    )
    )";
    Scene::get_scene()->add_source("init", source);
    Scene::get_scene()->run_script("init");
  }
#else
  {
    tl_alloc_tmp_enter();
    defer(tl_alloc_tmp_exit());
    Scene::get_scene()->add_source("init", read_file_tmp("scene.lsp"));
    Scene::get_scene()->run_script("init");
  }
#endif
  TextEditor::LanguageDefinition::CPlusPlus();
  editor.SetPalette(TextEditor::GetRetroBluePalette());
  //  Scene::get_scene()->add_source("test",
  //                                 R"(#version 300 es
  //  precision highp float;
  //  layout (location = 0) in vec2 vertex_position;
  //  layout (location = 1) in vec4 instance_offset;
  //  layout (location = 2) in vec3 instance_color;
  //  layout (location = 3) in vec2 instance_size;

  //  out vec3 color;
  //  uniform mat4 projection;
  //  void main() {
  //      color = instance_color;
  //      if (instance_offset.w > 0.0)
  //        gl_Position =  vec4(vertex_position * instance_size + instance_offset.xy,
  //        instance_offset.z, 1.0) * projection;
  //      else
  //        gl_Position =  vec4(vertex_position * instance_size + instance_offset.xy,
  //        instance_offset.z, 1.0);
  //  }
  //  )");
  SDL_Init(SDL_INIT_VIDEO);

  window = SDL_CreateWindow("Gfx-Node", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024,
                            1024, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
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
