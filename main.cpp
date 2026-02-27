#include <filesystem>
#include <iostream>
#include <iostream>
#include <stdio.h>
#include <GL/glew.h>    // Include GLEW before glfw
#include <GLFW/glfw3.h>
#include "utils/imgui/imgui.h"
#include "utils/imgui/imgui_impl_glfw.h"
#include "utils/imgui/imgui_impl_opengl3.h"
#include "menu/Menu.h"
#include "core/Core.h"
#include "utils/Utils.h"
#include "utils/logger/Logger.h"
#include "core/servermanager/ServerManager.h"

static void glfw_error_callback(int error, const char* description) {
    // Pipe GLFW internal errors directly into our System Log
    Utils::logger().Log(LogLevel::ERR, "GLFW Error " + std::to_string(error) + ": " + description);
}
int main() {
    // 1. Setup GLFW (Keep your existing hints/init)
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    // ... window creation and context setup ...
    GLFWwindow* window = glfwCreateWindow(1280, 720, "SBT Warehouse Manager", NULL, NULL);
    if (window == NULL) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // --- ADD THIS NOW ---
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        std::cerr << "GLEW Error: " << glewGetErrorString(err) << std::endl;
        return -1;
    }
    printf("[DEBUG] GLEW Initialized\n");

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        printf("Failed to initialize GLEW\n");
        return -1;
    }
    // 2. Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // 3. CORE DATA INITIALIZATION
    // This must happen AFTER the logger is ready but BEFORE the server starts.
    // This loads your catalog.json, routes.cfg, and sets the IP.
    printf("[DEBUG] 1: Initializing Core...\n");
    Core::Initialize();

    printf("[DEBUG] 2: Starting Server...\n");
    Core::server_manager().Start(8080);

    printf("[DEBUG] 3: Creating Menu Object...\n");
    Menu menu;
    menu.SetupStyle();
    printf("[DEBUG] 4: Entering Main Loop...\n");
    glfwShowWindow(window);
    glfwFocusWindow(window);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // 1. Start the Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. Clear the background (Keep this so the ghost doesn't return)
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f); // Professional dark grey
        glClear(GL_COLOR_BUFFER_BIT);

        // 3. YOUR UI CODE
        menu.Render();

        // 4. THE FINISH (Crucial sequence)
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    // 5. Cleanup
    Utils::logger().Log(LogLevel::INFO, "Shutting down application...");

    Core::server_manager().Stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}