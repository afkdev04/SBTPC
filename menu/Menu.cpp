#include "Menu.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <nlohmann/json.hpp>
#include <zint.h>
#include <vector>
#include <vector>

#include "../core/Core.h"
#include "../core/catalogmanager/CatalogManager.h"
#include "../core/routemanager/RouteManager.h"
#include "../core/servermanager/ServerManager.h"
#include "../../utils/imgui/imgui.h"
static std::string station_name = "";
static bool is_save_mode = false;
static bool show_file_browser = false;
static char buf_filename[128] = "catalog_backup.json";
static std::vector<std::filesystem::path> local_files;
static void* active_edit_ptr = nullptr;
static ViewState current_view = VIEW_DASHBOARD;
static ViewState last_view = VIEW_DASHBOARD;
struct ItemInfo { std::string brand; std::string item; };
static std::map<std::string, ItemInfo> master_barcodes;
Menu::Menu() {
    // Keep the UI-specific setup
    Core::current_station_ip = Core::server_manager().GetLocalIP();
    station_name = GetSystemUsername();

    // Load local UI-helper files
   // LoadBarcodeMap();

    // Instead, just ensure the UI is ready to draw what Core loaded
    Core::needs_ui_refresh = true;
}
bool Menu::NeedsUIRefresh() const {
    return Core::needs_ui_refresh;
}
void Menu::RenderFileBrowserModal() {
    // Set popup size
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal("FileBrowserModal", &show_file_browser, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(is_save_mode ? "Save Database As:" : "Select File to Load:");
        ImGui::Separator();

        if (is_save_mode) {
            // SAVE AS UI
            ImGui::InputText("Filename", buf_filename, IM_ARRAYSIZE(buf_filename));
            ImGui::Spacing();
            if (ImGui::Button("Commit Save", ImVec2(120, 0))) {
                std::string path = buf_filename;
                if (path.find(".json") == std::string::npos) path += ".json";
                Core::catalog_manager().SaveCatalog(path);
                show_file_browser = false;
                ImGui::CloseCurrentPopup();
            }
        } else {
            // OPEN FILE UI
            if (ImGui::BeginChild("FileSelect", ImVec2(0, 150), true)) {
                for (const auto& file : local_files) {
                    if (ImGui::Selectable(file.string().c_str())) {
                        strncpy(buf_filename, file.string().c_str(), sizeof(buf_filename));
                    }
                }
            }
            ImGui::EndChild();

            ImGui::Text("Selected: %s", buf_filename);
            if (ImGui::Button("Load Selected", ImVec2(120, 0))) {
                Core::catalog_manager().LoadCatalog(buf_filename);
                show_file_browser = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            show_file_browser = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
void Menu::Render() {
    ImGuiIO& io = ImGui::GetIO();

    // Ensure we don't try to render if the window is minimized/collapsed
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) return;

    // 1. TOP BAR
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::MenuItem("Dashboard", "F1")) current_view = VIEW_DASHBOARD;
        if (ImGui::MenuItem("Catalog", "F2"))   current_view = VIEW_CATALOG;
        if (ImGui::MenuItem("Settings", "F3"))  current_view = VIEW_SETTINGS;
        ImGui::EndMainMenuBar();
    }

    // 2. CONTENT AREA
    float menu_height = ImGui::GetFrameHeight();
    ImGui::SetNextWindowPos(ImVec2(0, menu_height));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - menu_height));

    ImGuiWindowFlags root_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("PrimaryView", nullptr, root_flags)) {
        switch (current_view) {
            case VIEW_DASHBOARD: Dashboard::Render(); break;
            case VIEW_CATALOG:   Catalog::Render();   break;
            case VIEW_SETTINGS:  this->RenderSettings(); break;
            default: ImGui::Text("Error: Unknown View State"); break;
        }
    }
    ImGui::End(); // Matches "PrimaryView"
}
void Menu::RenderSettings() {
    ImGui::Text("System Settings");
    ImGui::Separator();
    ImGui::Text("Station Name: %s", station_name.c_str());
}

void Menu::SetupStyle() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    // --- Font Setup ---
    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 1;
    config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("/usr/share/fonts/noto/NotoSans-Regular.ttf", 18.0f, &config);

    ImVec4* colors = style.Colors;

    // --- 1. Rounding & Spacing ---
    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowPadding = ImVec2(15, 15);
    style.ItemSpacing = ImVec2(8, 8);
    style.WindowBorderSize = 0.0f; // Modern apps avoid borders

    // --- 2. The "Deep Charcoal" Palette ---
    colors[ImGuiCol_WindowBg]         = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_Header]           = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]    = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
    colors[ImGuiCol_HeaderActive]     = ImVec4(0.30f, 0.30f, 0.33f, 1.00f);

    // Buttons (Vibrant Accent)
    colors[ImGuiCol_Button]           = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered]    = ImVec4(0.29f, 0.29f, 0.32f, 1.00f);
    colors[ImGuiCol_ButtonActive]     = ImVec4(0.21f, 0.21f, 0.23f, 1.00f);

    // Frame (Inputs, Checkboxes)
    colors[ImGuiCol_FrameBg]          = ImVec4(0.05f, 0.05f, 0.06f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]   = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive]    = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
    // Tabs
    colors[ImGuiCol_Tab]              = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TabHovered]       = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
    colors[ImGuiCol_TabActive]        = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_TabUnfocused]     = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    // Popups
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
    // ResizeGrip
    // The dot/triangle in the corner
    colors[ImGuiCol_ResizeGrip]        = ImVec4(0.30f, 0.30f, 0.33f, 0.50f); // Subtle grey
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.40f, 0.43f, 0.75f); // Brighter on hover
    colors[ImGuiCol_ResizeGripActive]  = ImVec4(0.50f, 0.50f, 0.53f, 1.00f); // Fully opaque when clicking
    // Titles
    colors[ImGuiCol_TitleBg]          = ImVec4(0.07f, 0.07f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive]    = ImVec4(0.07f, 0.07f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.07f, 0.07f, 0.08f, 0.75f);

    style.FramePadding = ImVec2(6.0f, 6.0f);
    style.CellPadding = ImVec2(6.0f, 4.0f);

    Core::global_row_height = ImGui::GetTextLineHeight() + (style.CellPadding.y * 2.0f);
}
