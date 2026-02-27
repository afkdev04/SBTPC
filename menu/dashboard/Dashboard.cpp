//
// Created by archdev on 2/23/26.
//

#include <GL/gl.h>

#include "imgui_internal.h"
#include "../Menu.h"
#include "../../core/servermanager/ServerManager.h"
#include "../../core/routemanager/RouteManager.h"
#include "../../core/ordermanager/OrderManager.h"
#include "../../core/invoicemanager/InvoiceManager.h"

#include "../../utils/Utils.h"
#include "../../utils/logger/Logger.h"
#include "../../utils/textureloader/TextureLoader.h"
#include "../../utils/Structs.h"
static bool show_server_logs = false;
static char buf_new_route[64] = "";
static unsigned int ipTextureID = 0;
static char search_buffer[256] = "";
static nlohmann::json editing_data;
static std::string current_editing_path;
static std::string path_to_delete;
// open_popup boolean is only used when we have to, eg: inside pushid/popid loops
static bool open_editor = false;
static bool open_invoicing = false;
static bool show_delete_confirm = false;
static bool show_order_creator = false;

static bool show_search = false;
static char search_buf[256] = "";
static int confirm_idx = -1;
static double duplicate_error_time = 0.0;
static float shake_start_time = 0.0f;
void Dashboard::Render() {
    RouteData current_data = Core::route_manager().GetSelectedData();
    // --- TOP BAR: SERVER STATUS & CONNECTION ---
    bool server_active = Core::server_manager().isRunning();
    ImVec4 status_color = server_active ? *(ImVec4*)Colors::green : *(ImVec4*)Colors::red;

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(status_color, "●");
    ImGui::SameLine();
    ImGui::Text("Server: http://%s:8080", Core::current_station_ip.c_str());

    // DYNAMIC RIGHT ALIGNMENT (Now for 3 buttons)
    float top_bar_btns_width = (UI::ButtonWidth * 3) + (ImGui::GetStyle().ItemSpacing.x * 2);
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - top_bar_btns_width);

    // --- NEW SERVER LOG BUTTON ---
    if (ImGui::Button(show_server_logs ? "Hide Logs" : "Server Log", ImVec2(UI::ButtonWidth, 0))) {
        show_server_logs = !show_server_logs;
    }

    ImGui::SameLine();

    if (ImGui::Button("Refresh IP", ImVec2(UI::ButtonWidth, 0))) {
        Core::current_station_ip = Core::server_manager().GetLocalIP();
    }

    ImGui::SameLine();

    ImGui::SameLine();
    if (ImGui::Button("Setup", ImVec2(UI::ButtonWidth, 0))) {
        if (ipTextureID != 0) glDeleteTextures(1, &ipTextureID);
        Core::server_manager().GenerateIPBarcode(Core::current_station_ip);
        ipTextureID = Utils::texture_loader().LoadTextureFromFile("ip_barcode.png");
        ImGui::OpenPopup("Connect Device");
    }

    ImGui::Separator();
    // --- ROW 1: ROUTE SELECTION ---
    UI::LabeledItem("Active Route:", UI::InputWidth);
    if (ImGui::BeginCombo("##ActiveRoute", Core::route_manager().GetCurrentRouteName().c_str())) {
        for (int i = 0; i < (int)Core::route_manager().routes.size(); i++) {
            if (ImGui::Selectable(Core::route_manager().routes[i].c_str(), Core::route_manager().selected_idx == i)) {
                Core::route_manager().selected_idx = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh", ImVec2(UI::ButtonWidth, 0))) {
        Core::route_manager().RefreshAllLists();
    }



    // --- ROW 2: ADD NEW ROUTE ---
    UI::LabeledItem("New Route:", UI::InputWidth);
    ImGui::InputTextWithHint("##newrt", "Enter route name...", buf_new_route, 64);

    ImGui::SameLine();
    if (ImGui::Button("Add", ImVec2(UI::ButtonWidth, 0))) {
        if (strlen(buf_new_route) > 0) {
            Core::route_manager().AddRoute(buf_new_route);
            memset(buf_new_route, 0, 64);
        }
    }

    ImGui::SameLine(0.0f, 20.0f);

    // STABLE ADD ORDER AREA
    if (Core::route_manager().selected_idx != 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, *(ImVec4*)Colors::green);
        if (ImGui::Button("+ Add Order", ImVec2(UI::ButtonWidth, 0))) {
            Core::order_manager().ClearOrder();
            ImGui::OpenPopup("Order Creator");
        }
        ImGui::PopStyleColor();
    } else {
        // If no route, show a disabled button to keep the row height identical
        ImGui::PushStyleColor(ImGuiCol_Button, *(ImVec4*)Colors::green);
        ImGui::BeginDisabled();
        ImGui::Button("+ Add Order##disabled", ImVec2(UI::ButtonWidth, 0));
        ImGui::EndDisabled();
        ImGui::PopStyleColor();
    }
    // --- REPLICATED RIGHT ALIGNMENT LOGIC ---
    // Move cursor to the far right, minus the button width and a small margin (e.g., 10-30px)
    ImGui::SameLine(ImGui::GetWindowWidth() - UI::ButtonWidth - 15.0f);

    // STABLE DELETE AREA
    if (Core::route_manager().selected_idx != 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, *(ImVec4*)Colors::red);
        if (ImGui::Button("Delete Route", ImVec2(UI::ButtonWidth, 0))) {
            Core::route_manager().RemoveRoute(Core::route_manager().selected_idx);
            Core::route_manager().selected_idx = 0;
            Core::route_manager().RefreshAllLists();
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, *(ImVec4*)Colors::red);
        ImGui::BeginDisabled();
        ImGui::Button("Delete Route##disabled", ImVec2(UI::ButtonWidth, 0));
        ImGui::EndDisabled();
        ImGui::PopStyleColor();
    }

    ImGui::Separator();

    // --- 4. Data Display ---
    {
        // Scope the lock as tightly as possible
        std::lock_guard<std::mutex> lock(Core::route_manager().route_mutex);
        //auto& current_data = Core::route_manager().GetSelectedData();

        if (ImGui::BeginTable("InboxLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings)) {
            ImGui::TableNextRow();

            // Left Column: Pending
            ImGui::TableSetColumnIndex(0);
            if (ImGui::BeginChild("OutBox", ImVec2(0, 450), true)) {
                ImGui::TextColored(*(ImVec4*)Colors::blue, "PENDING: %s", Core::route_manager().GetCurrentRouteName().c_str());
                ImGui::Separator();
                RenderFileTable(current_data.outgoing, "OutTable", false);
                ImGui::EndChild();
            }

            // Right Column: Completed
            ImGui::TableSetColumnIndex(1);
            if (ImGui::BeginChild("InBox", ImVec2(0, 450), true)) {
                ImGui::TextColored(*(ImVec4*)Colors::green, "COMPLETED: %s", Core::route_manager().GetCurrentRouteName().c_str());
                ImGui::Separator();
                RenderFileTable(current_data.incoming, "InTable", true);
                ImGui::EndChild();
            }
            ImGui::EndTable();
        }
        // --- LIVE SERVER LOG PANEL ---
        if (show_server_logs) {
            ImGui::Spacing();
            ImGui::Separator();

            ImGui::TextColored(*(ImVec4*)Colors::yellow, "SYSTEM ACTIVITY LOG");
            ImGui::SameLine();

            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - UI::ButtonWidth);
            if (ImGui::Button("Clear Logs", ImVec2(UI::ButtonWidth, 0))) {
                Utils::logger().Clear(); // Use the new utility to clear
            }

            ImGui::PushStyleColor(ImGuiCol_ChildBg, *(ImVec4*)Colors::darkbg);
            // Inside your Log Panel block
            if (ImGui::BeginChild("ServerConsole", ImVec2(0, 150), true)) {
                auto logs = Utils::logger().GetLogs(); // Use the new Logger

                if (logs.empty()) {
                    ImGui::TextDisabled("No activity recorded yet...");
                } else {
                    for (const auto& log : logs) {
                        // Simple color coding based on tags
                        if (log.find("[ERROR]") != std::string::npos)
                            ImGui::TextColored(*(ImVec4*)Colors::red, "%s", log.c_str());
                        else if (log.find("[OK]") != std::string::npos)
                            ImGui::TextColored(*(ImVec4*)Colors::green, "%s", log.c_str());
                        else
                            ImGui::TextUnformatted(log.c_str());
                    }
                }
                // Auto-scroll logic remains the same
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
    }


    if (Core::pending_refresh) {
        Core::route_manager().RefreshAllLists();
        Core::pending_refresh = false;
    }

    if (open_editor) {
        ImGui::OpenPopup("Order Editor");
        open_editor = false;
    }
    if (show_delete_confirm) {
        ImGui::OpenPopup("Delete Confirmation");
        show_delete_confirm = false;
    }
    if (open_invoicing) {
        ImGui::OpenPopup("InvoicePreview");
        open_invoicing = false;
    }

    if (show_order_creator) {
        ImGui::OpenPopup("Order Creator");
        show_order_creator = false;
    }
    if (Core::pending_refresh) {
        Core::route_manager().RefreshAllLists();
        Core::pending_refresh = false;
    }
    BarcodePopup();
    OrderCreatorPopup();
    OrderEditorPopup();
    DeleteConfirmPopup();
    InvoicePopup(Core::selected_invoice_path);
}
void Dashboard::SearchBarPopup() {
    auto& catalog_manager = Core::catalog_manager();
    auto& order_manager = Core::order_manager();
    ImGuiIO& io = ImGui::GetIO();

    // 1. Hotkey Trigger (Ctrl + N)
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
        show_search = true;
        search_buf[0] = '\0';
        // Reset times so the UI doesn't "shake" or show error on fresh open
        duplicate_error_time = -100.0;
        shake_start_time = -100.0;
        ImGui::OpenPopup("Add Item to Order");
    }

    // --- WINDOW SHAKE LOGIC ---
    float shake_offset_x = 0.0f;
    double shake_elapsed = ImGui::GetTime() - shake_start_time;
    if (shake_elapsed >= 0.0 && shake_elapsed < 0.4f) {
        float decay = 1.0f - (float)(shake_elapsed / 0.4f);
        shake_offset_x = sinf((float)shake_elapsed * 60.0f) * 8.0f * decay;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(ImVec2(center.x + shake_offset_x, center.y), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize;

    if (ImGui::BeginPopupModal("Add Item to Order", nullptr, flags)) {

        // 2. SEARCH INPUT
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##search", "Type to search catalog...", search_buf, sizeof(search_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            // Optional: Enter could trigger auto_select_idx = 0 logic
        }

        // --- KEYBOARD SHORTCUTS ---
        int auto_select_idx = -1;
        if (ImGui::IsItemActive()) {
            if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
                auto_select_idx = 0;
                ImGui::SetKeyboardFocusHere(-1);
            }
            if (io.KeyCtrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_1)) auto_select_idx = 0;
                if (ImGui::IsKeyPressed(ImGuiKey_2)) auto_select_idx = 1;
                if (ImGui::IsKeyPressed(ImGuiKey_3)) auto_select_idx = 2;
            }
        }

        ImGui::Separator();

        // Footer height: Error Message Space + Button Space + Padding
        float footer_h = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y;

        if (ImGui::BeginChild("ResultList", ImVec2(500.0f, 300.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            std::lock_guard<std::mutex> lock(catalog_manager.GetMutex());
            const auto& inventory = catalog_manager.GetCatalog();

            if (ImGui::BeginTable("SearchTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 80.0f);

                std::string query = search_buf;
                std::transform(query.begin(), query.end(), query.begin(), ::tolower);

                int match_count = 0;
                for (int i = 0; i < (int)inventory.size(); ++i) {
                    const auto& prod = inventory[i];

                    if (Core::split_fuzzy_match(query, prod.brand_lower, prod.name_lower)) {
                        if (prod.offers.contains("standard")) {
                            auto& stdOff = prod.offers.at("standard");
                            std::string displayName = "[" + prod.brand + "] " + prod.name;

                            // --- IMPROVED DUPLICATE CHECK ---
                            bool is_duplicate = false;
                            // Critical: Only check duplicates if the barcode isn't empty!
                            if (!stdOff.barcode.empty()) {
                                for (const auto& existing_item : order_manager.current_items) {
                                    if (existing_item.barcode == stdOff.barcode) {
                                        is_duplicate = true;
                                        break;
                                    }
                                }
                            }

                            bool shortcut_triggered = (auto_select_idx == match_count);

                            ImGui::TableNextRow(ImGuiTableRowFlags_None, Core::global_row_height);
                            ImGui::TableNextColumn();
                            ImGui::PushID(i);

                            ImGui::AlignTextToFramePadding();
                            bool selected = ImGui::Selectable(displayName.c_str(), false,
                                            ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);

                            if (match_count < 3) {
                                ImGui::SameLine();
                                ImGui::TextDisabled("(%d)", match_count + 1);
                            }

                            // --- ACTION LOGIC ---
                            if (selected || shortcut_triggered) {
                                if (is_duplicate) {
                                    duplicate_error_time = ImGui::GetTime();
                                    shake_start_time = ImGui::GetTime();
                                } else {
                                    order_manager.AddItem(stdOff.barcode, displayName, stdOff.price, prod.brand_index, prod.item_index);
                                    show_search = false;
                                    ImGui::CloseCurrentPopup();
                                }
                            }

                            ImGui::TableNextColumn();
                            ImGui::AlignTextToFramePadding();
                            ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "$%lld.%02lld",
                                               stdOff.price / 100, stdOff.price % 100);

                            ImGui::PopID();
                            match_count++;
                            if (match_count >= 50) break;
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }

        // --- 3. FOOTER SECTION ---
        ImGui::Separator();

        double time_since_error = ImGui::GetTime() - duplicate_error_time;
        if (time_since_error >= 0.0 && time_since_error < 2.0) {
            float alpha = 1.0f;
            if (time_since_error > 1.2) alpha = 1.0f - (float)((time_since_error - 1.2) / 0.8);

            float text_width = ImGui::CalcTextSize("Item already in order!").x;
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - text_width) * 0.5f);
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, alpha), "Item already in order!");
        } else {
            ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
        }

        if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
            show_search = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
void Dashboard::OrderCreatorPopup() {
    auto& manager = Core::order_manager();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Order Creator", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        // --- 1. Header Section ---
        ImGui::Text("Order Settings");
        ImGui::Separator();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Order Name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300.0f);
        // Accessing order_name via the manager
        ImGui::InputTextWithHint("##order_name", "e.g. Customer Name", manager.order_name, sizeof(manager.order_name));

        ImGui::Spacing();

        if (ImGui::Button("+ Add Item (Ctrl+N)", ImVec2(-1, 0)) || (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N))) {
            show_search = true;
            search_buf[0] = '\0';
            ImGui::OpenPopup("Add Item to Order");
        }

        SearchBarPopup();

        ImGui::Text("Current Order Items");
        ImGui::Separator();

        float total_line_h = ImGui::GetTextLineHeight();
        float button_row_h = ImGui::GetFrameHeightWithSpacing();
        float bottom_pad   = ImGui::GetStyle().WindowPadding.y;

        float footer_h = total_line_h + button_row_h + bottom_pad;
        float btn_size = ImGui::GetFrameHeight();
        if (ImGui::BeginChild("OrderList", ImVec2(0, -footer_h), true)) {
            if (ImGui::BeginTable("ActiveOrderTable", 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 130.0f);

                // Accessing current_items through the manager
                for (int i = 0; i < (int)manager.current_items.size(); ++i) {
                    auto& item = manager.current_items[i];
                    ImGui::PushID(i);

                    // USING YOUR NEW GLOBAL HEIGHT HERE
                    ImGui::TableNextRow(ImGuiTableRowFlags_None, Core::global_row_height);

                    // --- COLUMN 0: QTY ---
                    ImGui::TableNextColumn();
                    int step = (io.KeyCtrl) ? 5 : (io.KeyShift ? 10 : 1);

                    if (ImGui::Button("-", ImVec2(btn_size, btn_size))) item.quantity = std::max(0, item.quantity - step);
                    ImGui::SameLine(0, 2);
                    ImGui::SetNextItemWidth(40.0f);
                    ImGui::InputInt("##qty", &item.quantity, 0, 0);
                    ImGui::SameLine(0, 2);
                    if (ImGui::Button("+", ImVec2(btn_size, btn_size))) item.quantity += step;

                    // --- COLUMN 1: NAME ---
                    ImGui::TableNextColumn();

                    // Modern alignment for text vs buttons
                    if (item.disabled) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    else if (item.complete) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(item.name.c_str());

                    if (item.disabled || item.complete) ImGui::PopStyleColor();

                    // --- COLUMN 2: PRICE & DELETE ---
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding(); // Center price text vertically

                    char price_text[32];
                    snprintf(price_text, sizeof(price_text), "$%lld.%02lld", item.price / 100, item.price % 100);
                    ImGui::Text("%s", price_text);

                    ImGui::SameLine(ImGui::GetColumnWidth() - 30.0f);
                    // Use manager.confirm_idx
                    if (confirm_idx == i) {
                        if (ImGui::Button("Sure?", ImVec2(btn_size * 1.5, btn_size))) { manager.RemoveItem(i); confirm_idx = -1; }
                    } else {
                        if (ImGui::Button("X", ImVec2(btn_size * 1.5, btn_size))) confirm_idx = i;
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                        confirm_idx = -1;
                    }

                    // 2. Reset if clicking anywhere else
                    // We check if the mouse was clicked AND the "Sure?" button wasn't the thing being clicked
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && confirm_idx != -1) {
                        // If the last item clicked wasn't the active "Sure?" button, reset
                        if (ImGui::GetActiveID() == 0) {
                            confirm_idx = -1;
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }

        // --- 3. Footer Section ---
        ImGui::Separator();
        long long total = manager.GetTotal();
        ImGui::Text("Total:"); ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "$%lld.%02lld", total / 100, total % 100);

        float btn_width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        if (ImGui::Button("Cancel", ImVec2(btn_width, 0))) {
            manager.ClearOrder();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        bool can_save = !manager.current_items.empty() && strlen(manager.order_name) > 0;
        if (!can_save) ImGui::BeginDisabled();
        if (ImGui::Button("Save & Close", ImVec2(btn_width, 0))) {
            manager.SaveOrderToPending();
            ImGui::CloseCurrentPopup();
        }
        if (!can_save) ImGui::EndDisabled();

        ImGui::EndPopup();
    }
}
void Dashboard::RenderFileTable(const std::vector<std::filesystem::path>& files, const char* table_id, bool is_completed) {
    if (ImGui::BeginTable(table_id, 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Order Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, is_completed ? 180.0f : 120.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)files.size(); i++) {
            ImGui::PushID(i);
            ImGui::TableNextRow();

            // files[i] is now a filesystem::path, so .string() works directly
            std::string pathStr = files[i].string();

            // Column 0: Filename
            ImGui::TableNextColumn();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(files[i].filename().string().c_str());

            // Column 1: Actions
            ImGui::TableNextColumn();

            float avail_w = ImGui::GetContentRegionAvail().x;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            int btn_count = is_completed ? 3 : 2;
            float btn_w = (avail_w - (spacing * (btn_count - 1))) / (float)btn_count;

            if (ImGui::Button("View", ImVec2(btn_w, 0))) {
                if (!is_completed) {
                    Core::order_manager().LoadOrderFromFile(pathStr);
                    show_order_creator = true;
                } else {
                    // Your logic for loading JSON into editing_data for the Editor
                    std::ifstream f(pathStr);
                    if (f.is_open()) {
                        f >> editing_data;
                        current_editing_path = pathStr;
                        open_editor = true;
                    }
                }
            }
            ImGui::SameLine();

            // INVOICE (Only for completed)
            if (is_completed) {
                if (ImGui::Button("Inv", ImVec2(btn_w, 0))) {
                    Core::invoice_manager().BuildInvoice(files[i]);
                    Core::selected_invoice_path = files[i];
                    open_invoicing = true;
                }
                ImGui::SameLine();
            }

            // DELETE
            ImGui::PushStyleColor(ImGuiCol_Button, *(ImVec4*)Colors::red);
            if (ImGui::Button("Del", ImVec2(btn_w, 0))) {
                path_to_delete = files[i];
                show_delete_confirm = true;
            }
            ImGui::PopStyleColor();

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}
void Dashboard::DeleteConfirmPopup() {
    if (ImGui::BeginPopupModal("Delete Confirmation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

        if (!path_to_delete.empty()) {
            ImGui::Text("Are you sure you want to delete this order?");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s",
                std::filesystem::path(path_to_delete).filename().string().c_str());
        } else {
            ImGui::Text("No item selected for deletion.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- BUTTON ROW ---
        // Calculate 50/50 width minus the spacing
        float avail_w = ImGui::GetContentRegionAvail().x;
        float btn_w = (avail_w - ImGui::GetStyle().ItemSpacing.x) / 2.0f;

        if (!path_to_delete.empty()) {
            // Yes Button
            ImGui::PushStyleColor(ImGuiCol_Button, *(ImVec4*)Colors::red);
            if (ImGui::Button("Yes, Delete", ImVec2(btn_w, 0))) {
                try {
                    std::filesystem::remove(path_to_delete);
                    Utils::logger().Log(LogLevel::INFO, "File Deleted: " + std::filesystem::path(path_to_delete).filename().string());
                    Core::pending_refresh = true;
                    path_to_delete = "";
                    ImGui::CloseCurrentPopup();
                } catch (const std::exception& e) {
                    Utils::logger().Log(LogLevel::ERR, "DELETE ERROR: " + std::string(e.what()));
                }
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }

        // Cancel Button (If path is empty, this becomes a full-width "Close" button)
        float final_cancel_w = path_to_delete.empty() ? -1.0f : btn_w;
        if (ImGui::Button(path_to_delete.empty() ? "Close" : "Cancel", ImVec2(final_cancel_w, 0))) {
            path_to_delete = "";
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
void Dashboard::OrderEditorPopup() {
    // 2. Remove "AlwaysAutoResize" to allow manual expansion
    if (ImGui::BeginPopupModal("Order Editor", nullptr, ImGuiWindowFlags_None)) {

        float footer_h = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        if (ImGui::BeginChild("OrderEditorScrollable", ImVec2(0, -footer_h), true)) {

            if (ImGui::BeginTable("EditorTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 0))) {
                ImGui::TableSetupColumn("Done", ImGuiTableColumnFlags_WidthFixed, 40);
                ImGui::TableSetupColumn("Product", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Partial", ImGuiTableColumnFlags_WidthFixed, 40);
                ImGui::TableSetupColumn("Quantity Control", ImGuiTableColumnFlags_WidthFixed, 200);
                ImGui::TableHeadersRow();

                ImGuiIO& io = ImGui::GetIO();

                for (int i = 0; i < (int)editing_data.size(); i++) {
                    auto& item = editing_data[i];
                    ImGui::PushID(i);

                    int itemIdx = item.value("item_idx", -1);
                    bool dsb    = item.value("is_disabled", false);
                    bool comp   = item.value("is_complete", false);
                    int req     = item.value("requested_qty", 0);
                    int got     = item.value("got_qty", 0);

                    ImGui::TableNextRow();

                    // Done
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Checkbox("##c", &comp)) item["m_bComplete"] = comp;

                    // Product
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextWrapped("%s", (itemIdx >= 0 && itemIdx < (int)Core::catalog_manager().GetCatalog().size())
        ? Core::catalog_manager().GetCatalog()[itemIdx].name.c_str() : "[Unknown]");
                    // Partial/Disabled
                    ImGui::TableSetColumnIndex(2);
                    if (ImGui::Checkbox("##d", &dsb)) item["m_bDisabled"] = dsb;

                    // Quantity Control
                    ImGui::TableSetColumnIndex(3);
                    int step = io.KeyCtrl ? 5 : (io.KeyShift ? 10 : 1);

                    if (!dsb) ImGui::BeginDisabled();
                    if (ImGui::Button("-", ImVec2(25, 25))) { got = (got >= step) ? got - step : 0; }
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(45);
                    if (!dsb) {
                        ImGui::InputInt("##view", &req, 0, 0, ImGuiInputTextFlags_ReadOnly);
                    } else {
                        if (ImGui::InputInt("##edit", &got, 0, 0)) { if (got < 0) got = 0; }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("+", ImVec2(25, 25))) { got += step; }
                    if (!dsb) ImGui::EndDisabled();

                    item["got_qty"] = got;
                    item["is_disabled"] = dsb;
                    item["is_complete"] = comp;

                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }

        // --- THE FIXED FOOTER ---
        // This is outside the child, so it stays visible while resizing
        ImGui::Separator();

        float avail_w = ImGui::GetContentRegionAvail().x;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float btn_w = (avail_w - spacing) / 2.0f;

        bool is_already_complete = (current_editing_path.find("incoming") != std::string::npos);

        // SAVE / COMPLETE ROW
        if (ImGui::Button("SAVE CHANGES", ImVec2(btn_w, 40))) {
            std::ofstream o(current_editing_path);
            if (o.is_open()) {
                o << editing_data.dump(4);
                Core::pending_refresh = true;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine();

        if (is_already_complete) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.2f, 1.0f));
            if (ImGui::Button("REOPEN ORDER", ImVec2(btn_w, 40))) {
                if (Core::route_manager().MoveOrder(current_editing_path, false)) ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button("MARK AS COMPLETE", ImVec2(btn_w, 40))) {
                if (Core::route_manager().MoveOrder(current_editing_path, true)) ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor();
        }

        // CLOSE ROW
        if (ImGui::Button("Close Without Saving", ImVec2(-1, 30))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
void Dashboard::BarcodePopup() {
    if (ImGui::BeginPopupModal("Connect Device", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Scan this with your handheld scanner:");

        // The data string MUST start with the prefix your Android app expects
        std::string barcodeData = "IP:" + Core::current_station_ip;

        // Using Zint to generate a standard 1D Barcode (Code 128)
        // Note: You may need to adjust your Zint logic to output Code 128
        // instead of a QR (which is type 58). Code 128 is type 20.

        if (ipTextureID != 0) {
            // Display as a wide rectangle rather than a square
            ImGui::Image((ImTextureID)(intptr_t)ipTextureID, ImVec2(400, 100));
        }

        ImGui::Text("Manual IP: %s", Core::current_station_ip.c_str());
        if (ImGui::Button("Close", ImVec2(400, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}