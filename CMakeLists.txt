cmake_minimum_required(VERSION 3.4.3)
project(gfxnode)
add_executable(gfxnode
main.cpp
io_parser.cpp
context.cpp
3rdparty/imgui/imgui.cpp
3rdparty/imgui/imgui_widgets.cpp
3rdparty/imgui/imgui_draw.cpp
3rdparty/imgui/imgui_demo.cpp
3rdparty/imgui/examples/imgui_impl_opengl3.cpp
3rdparty/imgui/examples/imgui_impl_sdl.cpp
3rdparty/ImGuiColorTextEdit/TextEditor.cpp
)
target_include_directories(gfxnode
  PRIVATE
  3rdparty
  3rdparty/imgui
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DIMGUI_IMPL_OPENGL_LOADER_CUSTOM -march=skylake -Werror -ferror-limit=1 -fno-exceptions -fno-rtti -fvisibility=hidden")
target_link_libraries(gfxnode SDL2 OpenGL pthread)