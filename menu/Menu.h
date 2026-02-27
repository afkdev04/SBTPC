#ifndef SBTPC_MENU_H
#define SBTPC_MENU_H

#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../core/Core.h"
#include "imgui.h"
class Product;
// Helper for system identity
inline std::string GetSystemUsername() {
#ifdef _WIN32
    const char* user = std::getenv("USERNAME");
#else
    const char* user = std::getenv("USER");
#endif
    return (user != nullptr) ? std::string(user) : "Unknown-Station";
}


namespace UI {
    // Widths
    inline float LabelWidth = 120.0f;
    inline float InputWidth = 250.0f;
    inline float ButtonWidth = 120.0f;
    inline float SquareWidth = 120.0f;

    // Heights
    inline float ChildHeight = 230.0f;
    inline float FrameHeight = 0.0f; // 0.0f uses ImGui's default calculated height
    inline float ButtonHeight = 40.f; // 0.0f uses ImGui's default calculated height

    // Layout
    inline float Gutter = 8.0f; // Space between 50/50 splits
    // The Helper Function
    // 'inline' is key here so you can include this file everywhere
    inline void LabeledItem(const char* label, float width = -FLT_MIN) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(UI::LabelWidth);
        if (width != 0.0f) {
            ImGui::SetNextItemWidth(width);
        }
    }
}
enum ViewState {
    VIEW_DASHBOARD = 0,
    VIEW_CATALOG,
    VIEW_SETTINGS
};
namespace Dashboard {
    void Render();
    void RenderFileTable(const std::vector<std::filesystem::path>& files, const char* table_id, bool is_completed);
    void BarcodePopup();
    void SearchBarPopup();
    void OrderCreatorPopup();
    void OrderEditorPopup();
    void DeleteConfirmPopup();
    void InvoicePopup(const std::string& path);
}
namespace Catalog {
    void Render();
    void RenderTable(ImGuiID brandID, ImGuiID productID);
    void BrandEditorPopup();
    void ProductEditorPopup();
    void ConfirmBackupPopup();
    void ConfirmRestorePopup();
    void MassPriceEditPopup();
    void BackupsPopup();
}
class Menu {
public:
    Menu();
    bool NeedsUIRefresh() const;
    void Render();
    void SetupStyle();
private:
    void RenderSettings();
    void RenderFileBrowserModal();
};

#endif //SBTPC_MENU_H