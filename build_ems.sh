emcc -O2 \
../main.cpp \
../io_parser.cpp \
../context.cpp \
../3rdparty/imgui/imgui_draw.cpp \
../3rdparty/imgui/imgui.cpp \
../3rdparty/imgui/imgui_widgets.cpp \
../3rdparty/imgui/imgui_demo.cpp \
../3rdparty/imgui/examples/imgui_impl_opengl3.cpp \
../3rdparty/imgui/examples/imgui_impl_sdl.cpp \
../3rdparty/ImGuiColorTextEdit/TextEditor.cpp \
-I ../3rdparty/imgui \
-std=c++14 -s TOTAL_MEMORY=268435456 \
    -s USE_SDL_IMAGE=2 -s USE_SDL=2 -s USE_WEBGL2=1 -I ../3rdparty \
    -s FULL_ES3=1 -o index.html && \
    cp ../default_index.html index.html && \
    python3 -m http.server
