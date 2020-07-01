cmake_minimum_required(VERSION 3.4.3)
project(gfxnode)
add_executable(gfxnode
main.cpp
context.cpp
gl3w.c
3rdparty/imgui/imgui.cpp
3rdparty/imgui/imgui_widgets.cpp
3rdparty/imgui/imgui_draw.cpp
3rdparty/imgui/imgui_demo.cpp
3rdparty/imgui/examples/imgui_impl_opengl3.cpp
3rdparty/imgui/examples/imgui_impl_sdl.cpp
3rdparty/ImGuiColorTextEdit/TextEditor.cpp
)
IF (WIN32)
  set(SDL2_PATH ${CMAKE_SOURCE_DIR}/bin/SDL2)
  if(EXISTS "${SDL2_PATH}/lib/x64/SDL2.lib")
  else()
	message(FATAL_ERROR "Please provide path to SLD2! e.g. -D SDL2_PATH=C:/SDL2")
  endif()
  add_custom_command(TARGET gfxnode POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_PATH}/lib/x64/SDL2.dll"
        $<TARGET_FILE_DIR:gfxnode>)
  set (LIBS
    "${SDL2_PATH}/lib/x64/SDL2.lib"
    "${SDL2_PATH}/lib/x64/SDL2main.lib"
    )
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
  /D IMGUI_IMPL_OPENGL_LOADER_CUSTOM \
	")
  set (INCLUDES "${SDL2_PATH}/include")
ELSE()
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
  -DIMGUI_IMPL_OPENGL_LOADER_CUSTOM \
	-march=skylake \
	-pedantic-errors -Wall -Wextra -Werror -ferror-limit=1 \
	-Wno-c99-extensions -Wno-comment \
	-fno-exceptions -fno-rtti -fvisibility=hidden")
  set (LIBS SDL2 pthread ncurses z dl
    )
  set (INCLUDES "")
ENDIF()

add_executable(data_struct_test_0
tests/data_struct_test_0.cpp
)
target_include_directories(gfxnode
  PRIVATE
  3rdparty
  3rdparty/imgui
  ${INCLUDES}
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
)
target_link_libraries(gfxnode
${LIBS}
)
