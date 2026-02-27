//
// Created by archdev on 2/23/26.
//
#include "imgui_internal.h"
#include "../Menu.h"
#include "../../core/catalogmanager/CatalogManager.h"
#include "../../utils/Utils.h"
#include "../../utils/logger/Logger.h"
// --- REGISTRATION BUFFERS ---
static char buf_brand_reg[128] = "";
static char buf_item_reg[128] = "";
static double buf_price_reg = 0.00;
static double registration_success_time = 0.0;

// --- SEARCH & FILTERING ---
static char buf_search[128] = "";
static std::string search_query = "";
static std::string current_brand_filter = "";
static bool request_jump_to_top = false;

// --- DELETION STATE ---
static std::string brand_to_delete = "";
static std::string deferred_delete_name = "";          // For Products
static std::string deferred_brand_delete_name = "";    // For Brands

// --- REORDERING & MOVING ---
static std::string move_target_brand = "";
static int move_to_idx = -1;
static std::string deferred_brand_move_name = "";
static int deferred_brand_move_target = -1;
static bool show_drag_confirm = false;

// --- MASS EDITING ---
static bool request_mass_edit = false;
static std::string target_mass_price_key = "";
static double mass_price_input = 0.00;

// --- CONFIG ---
static const std::string backupFilename = "inventory_backup.json";
void CellText(const std::string& str) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(str.c_str());
}
void Catalog::Render() {
    const auto& brands = Core::catalog_manager().GetMasterBrands();
    ImGuiID brandID = ImGui::GetID("Brand Editor");
    ImGuiID productID = ImGui::GetID("Product Editor");

    if (Core::needs_ui_refresh) {
        Core::needs_ui_refresh = false;
        // Don't return! Just let it draw one frame or use a flag.
    }
    // Logic Setup
    search_query = buf_search;
    std::transform(search_query.begin(), search_query.end(), search_query.begin(), ::tolower);
    if (Core::catalog_manager().GetSelectedBrandIdx() >= (int)Core::catalog_manager().GetBrandNames().size()) Core::catalog_manager().SetSelectedBrandIdx(0);
    auto brandNames = Core::catalog_manager().GetBrandNames();
    int selectedIdx = Core::catalog_manager().GetSelectedBrandIdx();

    if (brandNames.empty()) {
        current_brand_filter = "ALL BRANDS";
    } else {
        if (selectedIdx >= brandNames.size()) selectedIdx = 0;
        current_brand_filter = brandNames[selectedIdx];
    }
    // --- SECTION A: REGISTRATION ---
    ImGui::Text("MASTER REGISTRY");
    ImGui::Separator();
    ImGui::Spacing();

    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float half_width = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

    ImGui::BeginChild("BrandRegSection", ImVec2(half_width, UI::ChildHeight), true);
    ImGui::Text("New Brand");
    ImGui::Spacing();

    UI::LabeledItem("Name:");
    ImGui::InputText("##BrandName", buf_brand_reg, IM_ARRAYSIZE(buf_brand_reg));

    float btn_h = UI::FrameHeight;
    ImGui::SetCursorPosY(UI::ChildHeight - ImGui::GetFrameHeightWithSpacing() - 10.0f);
    if (ImGui::Button("Add Brand", ImVec2(-FLT_MIN, btn_h))) {
        if (strlen(buf_brand_reg) > 0) {
            Core::catalog_manager().AddBrand(buf_brand_reg);

            memset(buf_brand_reg, 0, sizeof(buf_brand_reg));
        }
    }

    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("ProductRegSection", ImVec2(half_width, UI::ChildHeight), true);
    ImGui::Text("New Product");
    ImGui::Spacing();

    UI::LabeledItem("Brand:");
    // 1. Get a local reference to the brands via the public getter

    // 2. We need a local index for the "New Product" brand dropdown
    // (Since we deleted brand_selection_idx from the manager)
    static int local_brand_reg_idx = 0;

    // DEFENSIVE CHECK
    if (brands.empty()) {
        ImGui::TextDisabled("No Brands Available");
        local_brand_reg_idx = 0;
    } else {
        // Wrap index to safety against the local variable
        if (local_brand_reg_idx < 0 || local_brand_reg_idx >= (int)brands.size()) {
            local_brand_reg_idx = 0;
        }

        const char* preview = brands[local_brand_reg_idx].c_str();

        if (ImGui::BeginCombo("##SelectBrand", preview)) {
            for (int i = 0; i < (int)brands.size(); i++) {
                bool is_selected = (local_brand_reg_idx == i);
                if (ImGui::Selectable(brands[i].c_str(), is_selected)) {
                    local_brand_reg_idx = i;
                }
            }
            ImGui::EndCombo();
        }
    }
    UI::LabeledItem("Item:");
    ImGui::InputText("##ItemName", buf_item_reg, sizeof(buf_item_reg));

    UI::LabeledItem("Price:");
    ImGui::InputDouble("##BasePrice", &buf_price_reg, 0.01, 1.0, "%.2f");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (buf_price_reg < 0) buf_price_reg = 0; // Prevent negative prices
    }

    ImGui::SetCursorPosY(UI::ChildHeight - ImGui::GetFrameHeightWithSpacing() - 10.0f);
    if (ImGui::Button("Add Product", ImVec2(-FLT_MIN, UI::FrameHeight))) {
        if (!brands.empty() && strlen(buf_item_reg) > 0) {

            // One clean call replaces ~30 lines of logic
            Core::catalog_manager().AddProduct(
                buf_item_reg,
                brands[Core::catalog_manager().GetSelectedBrandIdx()],
                buf_price_reg
            );

            registration_success_time = ImGui::GetTime();

            // Reset buffers
            memset(buf_item_reg, 0, sizeof(buf_item_reg));
            buf_price_reg = 0.0;
        }
    }
    float square_size = ImGui::GetFrameHeight();
    double time_since_success = ImGui::GetTime() - registration_success_time;
    if (time_since_success < 2.0) {
        float alpha = 1.0f;
        if (time_since_success > 1.0) {
            alpha = 1.0f - (float)(time_since_success - 1.0);
        }

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, alpha), "Product Registered!");
    }
    ImGui::EndChild();

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    {
        UI::LabeledItem("Filter:", UI::InputWidth);
        if (ImGui::BeginCombo("##brandFilter", current_brand_filter.c_str())) {
            for (int i = 0; i < (int)Core::catalog_manager().GetBrandNames().size(); i++) {
                if (ImGui::Selectable(Core::catalog_manager().GetBrandNames()[i].c_str(), Core::catalog_manager().GetSelectedBrandIdx() == i)) Core::catalog_manager().SetSelectedBrandIdx(i);
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        if (ImGui::Button("Edit Brands", ImVec2(UI::ButtonWidth, UI::FrameHeight))) {
            ImGui::OpenPopup("Brand Editor");
        }
    }

    {
        UI::LabeledItem("Search:", UI::InputWidth);

        float x_btn_width = square_size;
        float search_input_width = UI::InputWidth - x_btn_width - ImGui::GetStyle().ItemSpacing.x;

        ImGui::SetNextItemWidth(search_input_width);
        ImGui::InputText("##prodsearch", buf_search, sizeof(buf_search));

        ImGui::SameLine();
        if (ImGui::Button("X##clearMain", ImVec2(x_btn_width, UI::FrameHeight))) {
            memset(buf_search, 0, sizeof(buf_search));
        }
        ImGui::SameLine();
        if (ImGui::Button("Edit Products", ImVec2(UI::ButtonWidth, UI::FrameHeight))) {
            ImGui::OpenPopup("Product Editor");
        }

        ImGui::SameLine();
        if (ImGui::Button("Top ^", ImVec2(UI::ButtonWidth, UI::FrameHeight))) {
            request_jump_to_top = true;
        }
    }

    {
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - (UI::ButtonWidth * 2) - 30.0f);

        const std::string backupFilename = "inventory_backup.json";

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.4f, 0.1f, 1.0f));
        if (ImGui::Button("Backup", ImVec2(UI::ButtonWidth, 0))) {
            ImGui::OpenPopup("Restore Catalog from Backup");
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Restore", ImVec2(UI::ButtonWidth, 0))) {
            ImGui::OpenPopup("Confirm Restore?");
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();
    }

    {
        std::lock_guard<std::mutex> lock(Core::catalog_manager().GetMutex());
        RenderTable(brandID, productID);
    }

    if (request_mass_edit) {
        ImGui::OpenPopup("Mass Price Edit");
        request_mass_edit = false;
    }
    if (show_drag_confirm && !ImGui::IsPopupOpen("Confirm Reorder")) {
        ImGui::OpenPopup("Confirm Reorder");
    }
    BackupsPopup();
    BrandEditorPopup();
    ProductEditorPopup();
    ConfirmRestorePopup();
    ConfirmBackupPopup();
    MassPriceEditPopup();
}

void Catalog::RenderTable(ImGuiID brandID, ImGuiID productID) {
    const auto& catalog = Core::catalog_manager().GetCatalog();
    std::vector<int> visible_indices;

    for (int i = 0; i < (int)catalog.size(); i++) {
        if (Core::needs_ui_refresh) break;

        const auto& prod = catalog[i];

        bool brand_match = (Core::catalog_manager().GetSelectedBrandIdx() == 0 ||
                            prod.brand == current_brand_filter);

        // Search Check:
        bool search_match = search_query.empty() ||
                            Core::split_fuzzy_match(search_query, prod.brand_lower, prod.name_lower);

        if (brand_match && search_match) {
            visible_indices.push_back(i);
        }
    }
    float table_height = ImGui::GetContentRegionAvail().y;
    if (table_height < 100.0f) table_height = -1.0f;
    if (ImGui::BeginTable("ProductTable", 8,
ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY,
ImVec2(0.0f, table_height))) {
        // --- CUSTOM MODERN CONTEXT MENU ---
        // Triggered when right-clicking the table headers specifically
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("CatalogColumnsPopup");
        }

        if (ImGui::BeginPopup("CatalogColumnsPopup")) {
            ImGui::TextDisabled("DISPLAY COLUMNS");
            ImGui::Separator();

            for (int i = 0; i < ImGui::TableGetColumnCount(); i++) {
                const char* name = ImGui::TableGetColumnName(i);

                // Skip the "Name" column so user can't accidentally hide the primary info
                if (i == 0) continue;

                // Get current visibility state from ImGui's internal settings
                ImGuiTableColumnFlags col_flags = ImGui::TableGetColumnFlags(i);
                bool is_visible = (col_flags & ImGuiTableColumnFlags_IsVisible);

                if (ImGui::Checkbox(name, &is_visible)) {
                    ImGui::TableSetColumnEnabled(i, is_visible);
                }
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Reset to Default Widths")) {
                for (int i = 0; i < ImGui::TableGetColumnCount(); i++) {
                    ImGui::TableSetColumnWidth(i, -1.0f); // -1 resets to auto-layout
                }
            }
            ImGui::EndPopup();
        }
        ImGui::TableSetupColumn("Brand");
        ImGui::TableSetupColumn("Item");
        ImGui::TableSetupColumn("Std Price");
        ImGui::TableSetupColumn("Std Barcode");
        ImGui::TableSetupColumn("P1 Price");
        ImGui::TableSetupColumn("P1 Barcode");
        ImGui::TableSetupColumn("P2 Price");
        ImGui::TableSetupColumn("P2 Barcode");

        ImGui::TableNextRow(ImGuiTableRowFlags_Headers, Core::global_row_height);

        for (int column = 0; column < 8; column++) {
            ImGui::TableSetColumnIndex(column);
            const char* column_name = ImGui::TableGetColumnName(column);

            bool is_ctrl_held = ImGui::GetIO().KeyCtrl;
            bool is_editor_col = (column == 0 || column == 1);
            bool is_price_col = (column == 2 || column == 4 || column == 6);
            bool can_action = (is_editor_col || is_price_col);

            // 1. Color formatting
            if (is_ctrl_held && can_action) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            }

            // 2. Render Header (Registers the item in ImGui)
            ImGui::TableHeader(column_name);

            if (is_ctrl_held && can_action) {
                ImGui::PopStyleColor();
            }

            // 3. IMMEDIATE Click Check (Do this right after TableHeader)
            if (can_action && is_ctrl_held && ImGui::IsItemClicked()) {
                if (column == 0) ImGui::OpenPopup(brandID);
                if (column == 1) ImGui::OpenPopup(productID);

                if (is_price_col) {
                    if (column == 2) target_mass_price_key = "standard";
                    if (column == 4) target_mass_price_key = "promo1";
                    if (column == 6) target_mass_price_key = "promo2";
                    mass_price_input = 0.00;
                    request_mass_edit = true;
                }
            }
            // 4. Tooltip (Only if hovered)
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                if (is_ctrl_held) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                    if (is_editor_col) {
                        ImGui::Text("Click to open %s", (column == 0) ? "Brand Editor" : "Product Editor");
                    } else if (is_price_col) {
                        ImGui::Text("Click to Mass Edit %s prices", column_name);
                    }
                    ImGui::PopStyleColor();
                } else {
                    if (is_editor_col || is_price_col) ImGui::Text("Hold CTRL + Click for actions");
                    else ImGui::Text("Column: %s", column_name);
                }
                ImGui::EndTooltip();
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin((int)visible_indices.size());
        while (clipper.Step()) {
            if (Core::needs_ui_refresh) break;
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                if (Core::needs_ui_refresh) break; // Added safety
                int real_idx = visible_indices[i];
                auto& prod = catalog[real_idx];

                ImGui::PushID(real_idx);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0); CellText(prod.brand);
                ImGui::TableSetColumnIndex(1); CellText(prod.name.c_str());

                auto RenderOfferCells = [&](int startCol, const std::string& key) {
                    ImGui::PushID(key.c_str());
                    ImGui::TableSetColumnIndex(startCol);

                    // Use the prod reference for DISPLAY only
                    double ui_val = prod.offers.at(key).price / 100.0;
                    ImGui::SetNextItemWidth(-FLT_MIN);

                    // We use a temporary double so we don't modify 'prod' directly
                    if (ImGui::InputDouble("##p", &ui_val, 0, 0, "%.2f", ImGuiInputTextFlags_CharsDecimal)) {
                        // Tell the manager to update the actual data
                        Core::catalog_manager().UpdatePrice(real_idx, key, ui_val);
                    }

                    ImGui::TableSetColumnIndex(startCol + 1);
                    ImGui::TextUnformatted(prod.offers.at(key).barcode.empty() ? "[NONE]" : prod.offers.at(key).barcode.c_str());
                    ImGui::PopID();
                };

                RenderOfferCells(2, "standard");
                RenderOfferCells(4, "promo1");
                RenderOfferCells(6, "promo2");

                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
}


void Catalog::BrandEditorPopup() {
    if (ImGui::BeginPopupModal("Brand Editor")) {
        static char brand_search[64] = "";
        static char rename_buf[128] = "";
        static std::string target_brand = "";
        static std::string editing_index_brand = "";
        static int manual_index_buf = 0;
        static float scroll_lock_pos = -1.0f;


        ImGui::AlignTextToFramePadding();
        ImGui::Text("Manage Brands");
        float x_btn_width = ImGui::GetFrameHeight();
        float search_width = ImGui::GetContentRegionAvail().x - x_btn_width - ImGui::GetStyle().ItemSpacing.x;

        ImGui::SetNextItemWidth(search_width);
        ImGui::InputTextWithHint("##brSearch", "Search brands...", brand_search, sizeof(brand_search));
        ImGui::SameLine();
        if (ImGui::Button("X##clr", ImVec2(x_btn_width, 0))) memset(brand_search, 0, sizeof(brand_search));

        ImGui::Separator();
        ImGui::TextDisabled(" ID"); ImGui::SameLine(50.0f);
        ImGui::TextDisabled("SORT"); ImGui::SameLine(120.0f);
        ImGui::TextDisabled("BRAND NAME");
        ImGui::Spacing();

        float footer_h = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        if (ImGui::BeginChild("BrandList", ImVec2(0, -footer_h), true)) {
            std::string search_q = brand_search;
            std::transform(search_q.begin(), search_q.end(), search_q.begin(), ::tolower);

            std::vector<CatalogManager::BrandInfo> unique_brands = Core::catalog_manager().GetUniqueBrandsSorted();

            for (auto& b : unique_brands) {
                if (!search_q.empty() && b.name_lower.find(search_q) == std::string::npos)
                    continue;

                if (Core::needs_ui_refresh) continue;

                ImGui::PushID(b.index);
                bool is_ctrl_held = ImGui::GetIO().KeyCtrl;

                if (editing_index_brand == b.name) {
                    ImGui::SetNextItemWidth(40.0f);
                    ImGui::SetKeyboardFocusHere();

                    ImGui::InputInt("##manualIdx", &manual_index_buf, 0, 0);

                    bool enter_pressed = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
                    bool value_changed = ImGui::IsItemDeactivatedAfterEdit();
                    bool lost_focus = ImGui::IsItemDeactivated();

                    if (enter_pressed || value_changed) {
                        deferred_brand_move_name = b.name;
                        deferred_brand_move_target = manual_index_buf;
                        editing_index_brand = "";
                    }

                    else if (lost_focus || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                        editing_index_brand = "";
                    }
                } else {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextDisabled("[%03d]", b.index);

                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        if (is_ctrl_held) {
                            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Click to manually change ID");
                        } else {
                            ImGui::Text("Index: %d", b.index);
                            ImGui::TextDisabled("Hold CTRL + Click to edit");
                        }
                        ImGui::EndTooltip();

                        if (is_ctrl_held && ImGui::IsMouseReleased(0)) {
                            editing_index_brand = b.name;
                            manual_index_buf = b.index;
                        }
                    }
                }

                ImGui::SameLine(50.0f);
                if (ImGui::Button("^", ImVec2(x_btn_width, 0))) { deferred_brand_move_name = b.name; deferred_brand_move_target = b.index - 1; }
                ImGui::SameLine();
                if (ImGui::Button("v", ImVec2(x_btn_width, 0))) { deferred_brand_move_name = b.name; deferred_brand_move_target = b.index + 1; }

                float btn_width = 65.0f;
                float action_buttons_x = ImGui::GetWindowContentRegionMax().x - (btn_width * 2) - ImGui::GetStyle().ItemSpacing.x - 5.0f;
                float restricted_name_width = action_buttons_x - 120.0f - ImGui::GetStyle().ItemSpacing.x;

                ImGui::SameLine(120.0f);
                ImGui::BeginGroup();
                if (target_brand == b.name) {
                    ImGui::SetNextItemWidth(restricted_name_width);
                    ImGui::SetKeyboardFocusHere();

                    bool submitted = ImGui::InputText("##ren", rename_buf, sizeof(rename_buf), ImGuiInputTextFlags_EnterReturnsTrue);
                    bool deactivated = ImGui::IsItemDeactivated();

                    if (submitted) {
                        scroll_lock_pos = ImGui::GetScrollY();
                        Core::catalog_manager().RenameBrand(target_brand, rename_buf);
                        target_brand = "";
                    }
                    else if (deactivated || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                        target_brand = "";
                    }

                } else {
                    ImGui::Selectable(b.name.c_str(), false, ImGuiSelectableFlags_AllowOverlap, ImVec2(restricted_name_width, 0));

                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Brand: %s", b.name.c_str());
                        ImGui::TextDisabled("Click and drag to reorder list");
                        ImGui::EndTooltip();
                    }

                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                        ImGui::SetDragDropPayload("BR_REORDER", &b.index, sizeof(int));
                        ImGui::Text("Moving %s...", b.name.c_str());
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BR_REORDER")) {
                            int src_idx = *(const int*)payload->Data;
                            for(auto& search_b : unique_brands) {
                                if(search_b.index == src_idx) {
                                    deferred_brand_move_name = search_b.name;
                                    deferred_brand_move_target = b.index;
                                    break;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
                ImGui::EndGroup();
                ImGui::SameLine(action_buttons_x);
                ImGui::BeginGroup();
                {
                    if (ImGui::Button("Rename", ImVec2(btn_width, 0))) {
                        target_brand = b.name;
                        strncpy(rename_buf, b.name.c_str(), sizeof(rename_buf));
                    }

                    ImGui::SameLine();

                    if (brand_to_delete == b.name) {
                        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || (ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered()))
                            brand_to_delete = "";

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                        if (ImGui::Button("SURE?", ImVec2(btn_width, 0))) {
                            deferred_brand_delete_name = b.name; // Use the deferred variable
                            brand_to_delete = "";
                        }
                        ImGui::PopStyleColor();
                    } else {
                        if (ImGui::Button("Delete", ImVec2(btn_width, 0))) {
                            brand_to_delete = b.name;
                        }
                    }
                }
                ImGui::EndGroup();
                ImGui::PopID();
            }
            if (scroll_lock_pos >= 0.0f) {
                ImGui::SetScrollY(scroll_lock_pos);

                static int frames_to_hold = 0;
                if (frames_to_hold++ > 1) {
                    scroll_lock_pos = -1.0f;
                    frames_to_hold = 0;
                }
            }
            ImGui::EndChild();
        }

        if (!deferred_brand_move_name.empty()) {
            Core::catalog_manager().ReorderAndNormalize(deferred_brand_move_name, deferred_brand_move_target);
            deferred_brand_move_name = "";
            deferred_brand_move_target = -1;
        }

        if (!deferred_brand_delete_name.empty()) {
            Core::catalog_manager().DeleteBrand(deferred_brand_delete_name);
            deferred_brand_delete_name = "";
        }

        if (Core::needs_ui_refresh) {
            ImGui::TextColored(ImVec4(1,0,0,1), "Refreshing data...");
        }
        ImGui::Separator();

        if (!move_target_brand.empty()) {
            Core::catalog_manager().ReorderAndNormalize(move_target_brand, move_to_idx);
            move_target_brand = "";
            move_to_idx = -1;
            Core::needs_ui_refresh = true;
        }

        if (ImGui::Button("Close", ImVec2(-1, 0))) {
            memset(brand_search, 0, sizeof(brand_search));
            target_brand = "";
            brand_to_delete = "";
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
void Catalog::ProductEditorPopup() {
    if (ImGui::BeginPopupModal("Product Editor")) {
        static char pd_search[64] = "";
        static char pd_rename_buf[128] = "";
        static int pd_editing_idx = -1;
        static int pd_manual_idx_buf = 0;
        static int pd_local_brand_filter_idx = 0;
        static std::string pd_to_delete = "";
        static bool pd_is_renaming = false;

        static std::string deferred_pd_move_name = "";
        static int deferred_pd_move_target = -1;

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Manage Products");

        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::BeginCombo("##pdBrandFilter", Core::catalog_manager().GetBrandNames()[pd_local_brand_filter_idx].c_str())) {
            for (int i = 0; i < (int)Core::catalog_manager().GetBrandNames().size(); i++) {
                if (ImGui::Selectable(Core::catalog_manager().GetBrandNames()[i].c_str(), pd_local_brand_filter_idx == i))
                    pd_local_brand_filter_idx = i;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();

        float x_btn_width = ImGui::GetFrameHeight();
        float search_width = ImGui::GetContentRegionAvail().x - x_btn_width - ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(search_width);
        if (ImGui::InputTextWithHint("##pdSrch", "Search products...", pd_search, sizeof(pd_search))) {
            pd_editing_idx = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button("X##pdClr", ImVec2(x_btn_width, 0))) memset(pd_search, 0, sizeof(pd_search));

        ImGui::Separator();
        ImGui::TextDisabled(" ID"); ImGui::SameLine(50.0f);
        ImGui::TextDisabled("SORT"); ImGui::SameLine(120.0f);
        ImGui::TextDisabled("PRODUCT NAME");
        ImGui::Spacing();

        float footer_h = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;

        if (ImGui::BeginChild("ProdList", ImVec2(0, -footer_h), true)) {
            // 1. Setup local copies for filtering
            std::string search_q = pd_search;
            std::transform(search_q.begin(), search_q.end(), search_q.begin(), ::tolower);

            // Safety check for brand filter
            const auto& brand_list = Core::catalog_manager().GetBrandNames();
            std::string filter_brand = (pd_local_brand_filter_idx < brand_list.size()) ? brand_list[pd_local_brand_filter_idx] : "ALL BRANDS";

            // 2. Lock the mutex while we iterate
            std::unique_lock<std::mutex> lock(Core::catalog_manager().GetMutex());
            const auto& catalog = Core::catalog_manager().GetCatalog(); // Get the const reference

            for (int i = 0; i < (int)catalog.size(); i++) {
                if (Core::needs_ui_refresh) break; // Check for background updates
                auto& prod = Core::catalog_manager().GetCatalog()[i];

                if (pd_local_brand_filter_idx != 0 && prod.brand != filter_brand) continue;
                if (!search_q.empty() && prod.name_lower.find(search_q) == std::string::npos) continue;

                ImGui::PushID(i);
                bool is_ctrl_held = ImGui::GetIO().KeyCtrl;

                if (pd_editing_idx == i && !pd_is_renaming) {
                    ImGui::SetNextItemWidth(40.0f);
                    ImGui::SetKeyboardFocusHere();

                    ImGui::InputInt("##pdIdxIn", &pd_manual_idx_buf, 0, 0);

                    bool enter_pressed = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
                    bool value_changed = ImGui::IsItemDeactivatedAfterEdit();

                    if (enter_pressed || value_changed) {
                        deferred_pd_move_name = prod.name;
                        deferred_pd_move_target = pd_manual_idx_buf;
                        pd_editing_idx = -1;
                    }

                    if (ImGui::IsItemDeactivated() && !value_changed && !enter_pressed) {
                        pd_editing_idx = -1;
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                        pd_editing_idx = -1;
                    }
                } else {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextDisabled("[%03d]", i);

                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        if (is_ctrl_held) {
                            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Click to manually change ID");
                        } else {
                            ImGui::Text("Index: %d", i);
                            ImGui::TextDisabled("Hold CTRL + Click to edit");
                        }
                        ImGui::EndTooltip();

                        if (is_ctrl_held && ImGui::IsMouseReleased(0)) {
                            pd_editing_idx = i;
                            pd_manual_idx_buf = i;
                            pd_is_renaming = false;
                        }
                    }
                }

                ImGui::SameLine(50.0f);
                if (ImGui::Button("^##pd", ImVec2(x_btn_width, 0))) { deferred_pd_move_name = prod.name; deferred_pd_move_target = i - 1; }
                ImGui::SameLine();
                if (ImGui::Button("v##pd", ImVec2(x_btn_width, 0))) { deferred_pd_move_name = prod.name; deferred_pd_move_target = i + 1; }

                float btn_width = 65.0f;
                float action_buttons_x = ImGui::GetWindowContentRegionMax().x - (btn_width * 2) - ImGui::GetStyle().ItemSpacing.x - 5.0f;
                float restricted_name_width = action_buttons_x - 120.0f - ImGui::GetStyle().ItemSpacing.x;

                ImGui::SameLine(120.0f);
                ImGui::BeginGroup();
                // RENAME LOGIC (Updated to use Manager)
                if (pd_is_renaming && pd_editing_idx == i) {
                    ImGui::SetNextItemWidth(restricted_name_width);
                    ImGui::SetKeyboardFocusHere();

                    if (ImGui::InputText("##pdRen", pd_rename_buf, sizeof(pd_rename_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        // CALL THE MANAGER INSTEAD OF DIRECT EDIT
                        Core::catalog_manager().RenameProduct(i, pd_rename_buf);
                        pd_is_renaming = false;
                        pd_editing_idx = -1;
                    }
                } else {
                    ImGui::Selectable(prod.name.c_str(), false, ImGuiSelectableFlags_AllowOverlap, ImVec2(restricted_name_width, 0));

                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Product: %s", prod.name.c_str());
                        ImGui::TextDisabled("Click and drag to reorder list");
                        ImGui::EndTooltip();
                    }

                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("PD_REORDER", &i, sizeof(int));
                        ImGui::Text("Moving %s...", prod.name.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PD_REORDER")) {
                            int src_idx = *(const int*)payload->Data;
                            // FIX: Use getter to fetch the name for the deferred move
                            deferred_pd_move_name = catalog[src_idx].name;
                            deferred_pd_move_target = i;
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
                ImGui::EndGroup();

                ImGui::SameLine(action_buttons_x);
                if (ImGui::Button("Rename", ImVec2(btn_width, 0))) {
                    pd_is_renaming = true;
                    pd_editing_idx = i;
                    strncpy(pd_rename_buf, prod.name.c_str(), sizeof(pd_rename_buf));
                }
                ImGui::SameLine();
                if (pd_to_delete == prod.name) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
                    if (ImGui::Button("SURE?", ImVec2(btn_width, 0))) {
                        deferred_delete_name = pd_to_delete;
                        pd_to_delete = "";
                    }
                    ImGui::PopStyleColor();
                    if (ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered()) pd_to_delete = "";
                } else {
                    if (ImGui::Button("Delete", ImVec2(btn_width, 0))) pd_to_delete = prod.name;
                }

                ImGui::PopID();
            }
            lock.unlock();
            ImGui::EndChild();
        }

        if (!deferred_pd_move_name.empty()) {
            Core::catalog_manager().MoveProduct(deferred_pd_move_name, deferred_pd_move_target);
            deferred_pd_move_name = "";
            deferred_pd_move_target = -1;
        }

        if (!deferred_delete_name.empty()) {
            Core::catalog_manager().DeleteProductByName(deferred_delete_name);
            deferred_delete_name = "";
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(-FLT_MIN, 0))) {
            pd_is_renaming = false;
            pd_editing_idx = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
void Catalog::ConfirmBackupPopup() {
    if (ImGui::BeginPopupModal("Confirm Backup?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string fullPath = std::filesystem::absolute(backupFilename).string();

        ImGui::Text("Save database to the following location?");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f)); // Gold/Yellow
        ImGui::TextWrapped("%s", fullPath.c_str());
        ImGui::PopStyleColor();

        ImGui::Separator();

        if (ImGui::Button("SAVE NOW", ImVec2(120, 0))) {
            Core::catalog_manager().SaveCatalog(backupFilename);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("CANCEL", ImVec2(UI::ButtonWidth, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Catalog::ConfirmRestorePopup() {
    if (ImGui::BeginPopupModal("Confirm Restore?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string fullPath = std::filesystem::absolute(backupFilename).string();
        bool fileExists = std::filesystem::exists(backupFilename);

        if (fileExists) {
            ImGui::Text("Replace current data with the file at:");
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            ImGui::TextWrapped("%s", fullPath.c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();

            if (ImGui::Button("YES, OVERWRITE", ImVec2(UI::ButtonWidth, 0))) {
                Core::catalog_manager().LoadCatalog(backupFilename);

                Core::catalog_manager().SaveCatalog();

                Core::catalog_manager().RebuildBrandList();
                Core::catalog_manager().SortCatalog();
                Core::needs_ui_refresh = true;

                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "ERROR: File Not Found!");
            ImGui::TextWrapped("The system could not find:\n%s", fullPath.c_str());
        }

        ImGui::SameLine();
        if (ImGui::Button("BACK", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
void Catalog::MassPriceEditPopup() {
    if (ImGui::BeginPopupModal("Mass Price Edit", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Determine if we are filtering or doing a global update
        bool isGlobal = (Core::catalog_manager().GetSelectedBrandIdx() == 0);
        std::string targetBrand = isGlobal ? "" : current_brand_filter;

        ImGui::Text("Mass Edit Target: "); ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", isGlobal ? "EVERYTHING" : targetBrand.c_str());
        ImGui::Separator();

        ImGui::SetNextItemWidth(120);
        ImGui::InputDouble("New Price##val", &mass_price_input, 0.01, 1.0, "%.2f");

        if (ImGui::Button("Apply to All Matching", ImVec2(250, 40))) {
            // Pass the string name instead of the index
            Core::catalog_manager().MassUpdatePrice(
                targetBrand,
                target_mass_price_key,
                mass_price_input
            );

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Cancel", ImVec2(250, 40))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
void Catalog::BackupsPopup() {
    if (ImGui::BeginPopupModal("Restore Catalog from Backup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextDisabled("Select a snapshot to revert the catalog to that state.");
        ImGui::Separator();

        if (ImGui::BeginTable("BackupTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, ImVec2(500, 300))) {
            ImGui::TableSetupColumn("Date & Time", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableHeadersRow();

            if (std::filesystem::exists("backups")) {
                for (const auto& entry : std::filesystem::directory_iterator("backups")) {
                    if (entry.path().extension() == ".json") {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(entry.path().filename().string().c_str());

                        ImGui::TableSetColumnIndex(1);
                        if (ImGui::Button(("Restore##" + entry.path().string()).c_str())) {
                            // Logic to Restore
                            copy_file(entry.path(), "catalog.json", std::filesystem::copy_options::overwrite_existing);
                            Core::catalog_manager().LoadCatalog("catalog.json");
                            Utils::logger().Log(LogLevel::SUCCESS, "Catalog Restored to: " + entry.path().filename().string());
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
            }
            ImGui::EndTable();
        }

        if (ImGui::Button("Close", ImVec2(-1, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}