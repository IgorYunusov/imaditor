#include "program.h"
#include <GL/glextl.h>
#include <nfd.h>
#include <imgui.h>
#include <imgui_internal.h>
#include "imgui_impl_glfw_gl3.h"

#include "image.h"
#include "font-icons.h"
#include "shader.h"
#include "tools.h"
#include "actions/baseaction.h"

#define IMGUI_TABS_IMPLEMENTATION
#include "imgui_tabs.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

static Tools tools;
static Images images;

static struct {
    bool show_toolbar = false;
    bool show_content = false;
    bool show_dockbar = false;
    float width = 200.0f;
    float height = 300.0f;
    int mousex = 0;
    int mousey = 0;
    int zoom = 100;
    int translatex = 0;
    int translatey = 0;
    bool shiftPressed = false;
    bool ctrlPressed = false;

} state;

Program::Program(GLFWwindow* window)
    : _window(window)
{
    glfwSetWindowUserPointer(this->_window, static_cast<void*>(this));
}

Program::~Program()
{
    glfwSetWindowUserPointer(this->_window, nullptr);
}

static std::string vertexGlsl = "#version 150\n\
        in vec3 vertex;\
in vec2 texcoord;\
\
uniform mat4 u_projection;\
uniform mat4 u_view;\
\
out vec2 f_texcoord;\
\
void main()\
{\
    gl_Position = u_projection * u_view * vec4(vertex.xyz, 1.0);\
    f_texcoord = texcoord;\
}";

static std::string fragmentGlsl = "#version 150\n\
        uniform sampler2D u_texture;\
\
in vec2 f_texcoord;\
\
out vec4 color;\
\
void main()\
{\
    color = texture(u_texture, f_texcoord);\
}";




static std::string vertexBlocksGlsl = "#version 150\n\
        in vec3 vertex;\
in vec2 texcoord;\
\
uniform mat4 u_projection;\
\
out vec2 f_texcoord;\
\
void main()\
{\
    gl_Position = u_projection * vec4(vertex.xyz, 1.0);\
    f_texcoord = texcoord;\
}";

static std::string fragmentBlocksGlsl = "#version 150\n\
        in vec2 f_texcoord;\
out vec4 color;\
\
void main()\
{\
    if (int(gl_FragCoord.x) % 32 < 16 && int(gl_FragCoord.y) % 32 > 16\
            || int(gl_FragCoord.x) % 32 > 16 && int(gl_FragCoord.y) % 32 < 16)\
        color = vec4(0.9f, 0.9f, 0.92f, 1.0f);\
    else\
        color = vec4(1.0f, 1.0f, 1.0f, 1.0f);\
}";

static float g_vertex_buffer_data[] = {
    0.5f,  0.5f,  0.0f,  1.0f, 1.0f,  0.0f,
    0.5f, -0.5f,  0.0f,  1.0f, 0.0f,  0.0f,
    -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,  0.0f,
    -0.5f, -0.5f,  0.0f,  0.0f, 0.0f,  0.0f,
};

static GLuint program;
static GLuint blocksProgram;
static GLuint u_projection;
static GLuint u_view;
static GLuint vertexbuffer;
static GLuint texture;

bool Program::SetUp()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("../imaditor/imgui/extra_fonts/Roboto-Medium.ttf", 16.0f);

    ImFontConfig config;
    config.MergeMode = true;

    static const ImWchar icons_ranges_fontawesome[] = { 0xf000, 0xf3ff, 0 };
    io.Fonts->AddFontFromFileTTF("../imaditor/fontawesome-webfont.ttf", 18.0f, &config, icons_ranges_fontawesome);

    static const ImWchar icons_ranges_googleicon[] = { 0xe000, 0xeb4c, 0 };
    io.Fonts->AddFontFromFileTTF("../imaditor/MaterialIcons-Regular.ttf", 18.0f, &config, icons_ranges_googleicon);

    blocksProgram = LoadShaderProgram(vertexBlocksGlsl.c_str(), fragmentBlocksGlsl.c_str());
    program = LoadShaderProgram(vertexGlsl.c_str(), fragmentGlsl.c_str());
    u_projection = glGetUniformLocation(program, "u_projection");
    u_view = glGetUniformLocation(program, "u_view");

    glGenBuffers(1, &vertexbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

static float foreColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static float backColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

static int selectedTab = 0;
static const char** tabNames = nullptr;
static int tabNameAllocCount = 0;

void updateTabNames()
{
    if (tabNames != nullptr) delete []tabNames;
    if (images._images.size() >= tabNameAllocCount)
    {
        delete []tabNames;
        tabNameAllocCount += 32;
        tabNames = new const char*[tabNameAllocCount];
    }

    for (int i = 0; i < images._images.size(); ++i)
    {
        auto tmp = new char[images._images[i]->_name.size()];
        strcpy(tmp, images._images[i]->_name.c_str());
        tabNames[i] = tmp;
    }
}

void addImage(Image* img)
{
    selectedTab = images._images.size();
    images._images.push_back(img);
    updateTabNames();
    images.select(selectedTab);
}

void newImage()
{
    auto img = new Image();
    img->_name = "New";
    img->_fullPath = "New.png";
    img->addLayer();
    addImage(img);
}

void openImage()
{
    nfdchar_t *outPath = NULL;
    nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath);

    if (result == NFD_OKAY)
    {
        auto img = new Image();
        img->_fullPath = outPath;
        std::replace(img->_fullPath.begin(), img->_fullPath.end(), '\\', '/');
        img->_name = img->_fullPath.substr(img->_fullPath.find_last_of('/') + 1);
        img->fromFile(outPath);
        addImage(img);
    }
}

void addLayer()
{
    if (images.selected() != nullptr)
    {
        images.selected()->addLayer();
    }
}

void removeCurrentLayer()
{
    if (images.selected() != nullptr)
    {
        images.selected()->removeCurrentLayer();
    }
}

void moveCurrentLayerUp()
{
    if (images.selected() != nullptr)
    {
        images.selected()->moveCurrentLayerUp();
    }
}

void moveCurrentLayerDown()
{
    if (images.selected() != nullptr)
    {
        images.selected()->moveCurrentLayerDown();
    }
}

void Program::Render()
{
    glViewport(0, 0, state.width, state.height);
    glClearColor(114/255.0f, 144/255.0f, 154/255.0f, 255/255.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (images.selected() != nullptr)
    {
        auto img = images.selected();
        if (img->isDirty()) images.uploadSelectedImage();

        auto zoom = glm::scale(glm::mat4(), glm::vec3(state.zoom / 100.0f));
        auto translate = glm::translate(zoom, glm::vec3(state.translatex, state.translatey, 0.0f));
        auto projection = glm::ortho(-(state.width/2.0f), (state.width/2.0f), (state.height/2.0f), -(state.height/2.0f));

        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        auto full = glm::scale(glm::mat4(), glm::vec3(img->_size[0], img->_size[1], 1.0f));
        glUseProgram(blocksProgram);
        glUniformMatrix4fv(u_projection, 1, GL_FALSE, glm::value_ptr(projection * translate * full));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glUseProgram(program);
        glUniformMatrix4fv(u_projection, 1, GL_FALSE, glm::value_ptr(projection * translate));

        glBindTexture(GL_TEXTURE_2D, img->_glindex);
        auto view = glm::scale(glm::mat4(), glm::vec3(img->_size[0], img->_size[1], 1.0f));
        glUniformMatrix4fv(u_view, 1, GL_FALSE, glm::value_ptr(view));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindTexture(GL_TEXTURE_2D, 0);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }

    ImGui_ImplGlfwGL3_NewFrame();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 1.0f);
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4());
        {
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("New", "CTRL+N")) newImage();
                    if (ImGui::MenuItem("Open", "CTRL+O")) openImage();
                    if (ImGui::MenuItem("Save", "CTRL+S")) { }
                    if (ImGui::MenuItem("Save As..", "CTRL+SHIFT+Z")) { }
                    if (ImGui::MenuItem("Close")) { }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Quit")) { }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Edit"))
                {
                    if (ImGui::MenuItem("Undo", "CTRL+Z")) { }
                    if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) { }  // Disabled item
                    ImGui::Separator();
                    if (ImGui::MenuItem("Cut", "CTRL+X")) { }
                    if (ImGui::MenuItem("Copy", "CTRL+C")) { }
                    if (ImGui::MenuItem("Paste", "CTRL+V")) { }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Help"))
                {
                    if (ImGui::MenuItem("About IMaditor")) { }
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }
        }
        ImGui::PopStyleColor();

        const int dockbarWidth = 250;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.20f, 0.20f, 0.47f, 0.60f));
        {
            ImGui::Begin("toolbar", &state.show_toolbar, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |  ImGuiWindowFlags_NoTitleBar);
            {
                ImGui::SetWindowPos(ImVec2(0, 22));
                ImGui::SetWindowSize(ImVec2(45, state.height - 57));

                for (int i = 0; i < tools.toolCount(); i++)
                {
                    ImGui::PushID(i);
                    ImGui::PushStyleColor(ImGuiCol_Button, tools.isSelected(i) ? ImVec4() : ImGui::GetStyle().Colors[ImGuiCol_Button]);
                    if (ImGui::Button(tools[i]._icon, ImVec2(30, 30))) tools.selectTool(i);
                    ImGui::PopStyleColor(1);
                    ImGui::PopID();
                }
            }
            ImGui::End();

            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4());
            {
                ImGui::Begin("content", &(state.show_content), ImGuiWindowFlags_NoResize |ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
                {
                    ImGui::SetWindowPos(ImVec2(45, 22));
                    ImGui::SetWindowSize(ImVec2(state.width - 45 - dockbarWidth, state.height - 57));

                    if (images.hasImages())
                    {
                        if (ImGui::TabLabels(tabNames, images._images.size(), selectedTab))
                        {
                            images.select(selectedTab);
                        }
                    }
                }
                ImGui::End();
            }
            ImGui::PopStyleColor();

            ImGui::Begin("dockbar", &(state.show_dockbar), ImGuiWindowFlags_NoResize |ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
            {
                ImGui::SetWindowPos(ImVec2(state.width - dockbarWidth, 22));
                ImGui::SetWindowSize(ImVec2(dockbarWidth, state.height - 57));

                if (ImGui::CollapsingHeader("Color options", "colors", true, true))
                {
                    ImGui::ColorEdit4("Fore", foreColor);
                    ImGui::ColorEdit4("Back", backColor);
                }

                if (images.hasImages())
                {
                    if (ImGui::CollapsingHeader("Layer options", "layers", true, true))
                    {
                        if (ImGui::Button(FontAwesomeIcons::FA_PLUS, ImVec2(30.0f, 30.0f))) addLayer();
                        ImGui::SameLine();
                        if (ImGui::Button(FontAwesomeIcons::FA_MINUS, ImVec2(30.0f, 30.0f))) removeCurrentLayer();
                        ImGui::SameLine();
                        if (ImGui::Button(FontAwesomeIcons::FA_ARROW_UP, ImVec2(30.0f, 30.0f))) moveCurrentLayerUp();
                        ImGui::SameLine();
                        if (ImGui::Button(FontAwesomeIcons::FA_ARROW_DOWN, ImVec2(30.0f, 30.0f))) moveCurrentLayerDown();

                        ImGui::Separator();

                        for (int i = 0; i < images.selected()->_layers.size(); i++)
                        {
                            auto layer = images.selected()->_layers[i];
                            ImGui::PushID(i);
                            auto title = layer->_name;
                            if (images.selected()->_selectedLayer == i) title += " (selected)";
                            if (ImGui::TreeNode("layer_node", title.c_str()))
                            {
                                if (ImGui::Button(layer->isVisible() ? FontAwesomeIcons::FA_EYE : FontAwesomeIcons::FA_EYE_SLASH, ImVec2(30, 30)))
                                {
                                    layer->toggleVisibility();
                                }
                                ImGui::SameLine();
                                ImGui::PushStyleColor(ImGuiCol_Button, i == images.selected()->_selectedLayer ? ImGui::GetStyle().Colors[ImGuiCol_ButtonActive] : ImVec4(0.20f, 0.40f, 0.47f, 0.0f));
                                if (ImGui::Button(layer->_name.c_str(), ImVec2(-1, 30))) images.selected()->selectLayer(i);
                                ImGui::PopStyleColor(1);

                                ImGui::SliderFloat("Alpha", &(layer->_alpha), 0.0f, 1.0f);
                                ImGui::Combo("Mode", &(layer->_alphaMode), "Normal\0Darken\0Lighten\0Hue\0Saturation\0Color\0Lumminance\0Multiply\0Screen\0Dissolve\0Overlay\0Hard Light\0Soft Light\0Difference\0Dodge\0Burn\0Exclusion\0\0");

                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }

                        ImGui::Separator();
                    }
                }

                if (ImGui::CollapsingHeader("Tool options", "tools", true, true))
                {
                    ImGui::TextWrapped("This window is being created by the ShowTestWindow() function. Please refer to the code for programming reference.\n\nUser Guide:");
                }
            }
            ImGui::End();

            ImGui::Begin("statusbar", &(state.show_content), ImGuiWindowFlags_NoResize |ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
            {
                ImGui::SetWindowPos(ImVec2(0, state.height - 35));
                ImGui::SetWindowSize(ImVec2(state.width, 35));

                ImGui::Columns(3);
                ImGui::Text("status bar");
                ImGui::NextColumn();
                ImGui::SliderInt("zoom", &(state.zoom), 10, 400);
                ImGui::NextColumn();
                ImGui::Text("mouse: %d %d", state.mousex, state.mousey);
                ImGui::Columns(1);
            }
            ImGui::End();
        }
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    ImGui::Render();
}

void Program::onKeyAction(int key, int scancode, int action, int mods)
{
    state.shiftPressed = (mods & GLFW_MOD_SHIFT);
    state.ctrlPressed = (mods & GLFW_MOD_CONTROL);
}

void Program::onMouseMove(int x, int y)
{
    state.mousex = x;
    state.mousey = y;
}

void Program::onMouseButton(int button, int action, int mods)
{
    if (images.selected() == nullptr) return;

    if (tools.selectedTool()._actionFactory == nullptr) return;

    auto fac = tools.selectedTool()._actionFactory;
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        fac->PrimaryMouseButtonDown(images.selected(),
                                    mods & GLFW_MOD_SHIFT,
                                    mods & GLFW_MOD_CONTROL,
                                    mods & GLFW_MOD_ALT,
                                    mods & GLFW_MOD_SUPER);
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        fac->PrimaryMouseButtonUp(images.selected(),
                                  mods & GLFW_MOD_SHIFT,
                                  mods & GLFW_MOD_CONTROL,
                                  mods & GLFW_MOD_ALT,
                                  mods & GLFW_MOD_SUPER);
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        fac->PrimaryMouseButtonUp(images.selected(),
                                  mods & GLFW_MOD_SHIFT,
                                  mods & GLFW_MOD_CONTROL,
                                  mods & GLFW_MOD_ALT,
                                  mods & GLFW_MOD_SUPER);
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        fac->PrimaryMouseButtonUp(images.selected(),
                                  mods & GLFW_MOD_SHIFT,
                                  mods & GLFW_MOD_CONTROL,
                                  mods & GLFW_MOD_ALT,
                                  mods & GLFW_MOD_SUPER);
    }
}

void Program::onScroll(int x, int y)
{
    if (state.shiftPressed)
    {
        state.translatex += (y * 5);
    }
    else if (state.ctrlPressed)
    {
        state.translatey += (y * 5);
    }
    else
    {
        state.zoom += (y * 5);
        if (state.zoom < 10) state.zoom = 10;
    }
}

void Program::onResize(int width, int height)
{
    state.width = width;
    state.height = height;

    glViewport(0, 0, width, height);
}

void Program::CleanUp()
{ }

void Program::KeyActionCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto app = static_cast<Program*>(glfwGetWindowUserPointer(window));

    if (app != nullptr) app->onKeyAction(key, scancode, action, mods);
}

void Program::CursorPosCallback(GLFWwindow* window, double x, double y)
{
    auto app = static_cast<Program*>(glfwGetWindowUserPointer(window));

    if (app != nullptr) app->onMouseMove(int(x), int(y));
}

void Program::ScrollCallback(GLFWwindow* window, double x, double y)
{
    auto app = static_cast<Program*>(glfwGetWindowUserPointer(window));

    if (app != nullptr) app->onScroll(int(x), int(y));
}

void Program::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto app = static_cast<Program*>(glfwGetWindowUserPointer(window));

    if (app != nullptr) app->onMouseButton(button, action, mods);
}

void Program::ResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = static_cast<Program*>(glfwGetWindowUserPointer(window));

    if (app != nullptr) app->onResize(width, height);
}
