//
// Created by archdev on 2/24/26.
//
#include "../Menu.h"
#include "../../core/invoicemanager/InvoiceManager.h"
#include "../../core/servermanager/ServerManager.h"
#include "../../utils/Utils.h"
#include "../../utils/logger/Logger.h"

static bool show_credit_item = false;
//todo move logging to its own util
void Dashboard::InvoicePopup(const std::string& path) {
    static bool show_credit_search = false;
    static char search_buf[128] = "";

    // Extract filename once for use in logs and UI
    std::string filename = std::filesystem::path(path).filename().string();

    if (ImGui::BeginPopupModal("InvoicePreview", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("File: %s", filename.c_str());
        ImGui::Separator();

        if (Core::invoice_manager().invoice_items.empty()) {
            ImGui::TextColored(*(ImVec4*)Colors::red, "No items loaded. Click Generate.");
        }

        if (ImGui::Button("Generate/Refresh Invoice")) {
            Core::invoice_manager().BuildInvoice(path);
            Utils::logger().Log(LogLevel::INFO, "Invoice data refreshed from " + filename);
        }
        ImGui::SameLine();

        // --- EXPORT CSV WITH SAFETY ---
        if (ImGui::Button("Export CSV")) {
            try {
                Core::invoice_manager().SaveInvoiceToCSV(path);
                Utils::logger().Log(LogLevel::SUCCESS, "Exported: " + filename);
            } catch (const std::exception& e) {
                Utils::logger().Log(LogLevel::ERR, "Export Failed: File may be open in another program.");
            }
        }

        ImGui::Spacing();

        if (ImGui::BeginTable("InvTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 400))) {
            ImGui::TableSetupColumn("Item Name");
            ImGui::TableSetupColumn("Status");
            ImGui::TableSetupColumn("Qty");
            ImGui::TableSetupColumn("Unit Price");
            ImGui::TableSetupColumn("Line Total");
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)Core::invoice_manager().invoice_items.size(); i++) {
                auto& line = Core::invoice_manager().invoice_items[i];
                ImGui::PushID(i);
                ImGui::TableNextRow();

                // COLUMN 0: NAME & DELETE LOGIC
                ImGui::TableSetColumnIndex(0);
                bool row_clicked = ImGui::Selectable(line.name.c_str(), false,
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);

                if (row_clicked && ImGui::GetIO().KeyCtrl) {
                    std::string deletedName = line.name;
                    Core::invoice_manager().invoice_items.erase(Core::invoice_manager().invoice_items.begin() + i);
                    Core::invoice_manager().RecalculateTotal();
                    Utils::logger().Log(LogLevel::INFO, "Removed from Invoice: " + deletedName);
                    ImGui::PopID();
                    break;
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("CTRL + CLICK to remove this line");
                }

                // COLUMN 1: STATUS
                ImGui::TableSetColumnIndex(1);
                if (line.is_credit) ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "CREDIT");
                else if (line.disabled) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "STOPPED");
                else ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1), "DONE");

                // COLUMN 2: QTY
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", line.got);

                // COLUMN 3: UNIT PRICE
                ImGui::TableSetColumnIndex(3);
                if (line.price < 0)
                    ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "-$%.2f", (double)std::abs(line.price) / 100.0);
                else
                    ImGui::Text("$%.2f", (double)line.price / 100.0);

                // COLUMN 4: LINE TOTAL
                ImGui::TableSetColumnIndex(4);
                if (line.subtotal < 0)
                    ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "-$%.2f", (double)std::abs(line.subtotal) / 100.0);
                else
                    ImGui::Text("$%.2f", (double)line.subtotal / 100.0);

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::Separator();

        double display_total = static_cast<double>(Core::invoice_manager().grand_total) / 100.0;
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "GRAND TOTAL: $%.2f", display_total);
        ImGui::Spacing();

        // --- ADJUSTMENT BUTTONS ---
        float avail_w = ImGui::GetContentRegionAvail().x;
        float btn_w = (avail_w - (ImGui::GetStyle().ItemSpacing.x * 2)) / 3.0f;

        if (ImGui::Button("Add Item Credit", ImVec2(btn_w, 0))) {
            show_credit_search = true;
            search_buf[0] = '\0';
            show_credit_item = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Credit", ImVec2(btn_w, 0))) {
            if (!Core::invoice_manager().invoice_items.empty() && Core::invoice_manager().invoice_items.back().is_credit) {
                Utils::logger().Log(LogLevel::INFO, "Reverted last credit adjustment");
                Core::invoice_manager().invoice_items.pop_back();
                Core::invoice_manager().RecalculateTotal();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Manual USD Credit", ImVec2(btn_w, 0))) {
            ImGui::OpenPopup("ManualCreditEntry");
        }

        ImGui::Spacing();

        // --- FINAL SAVE & CLOSE ---
        if (ImGui::Button("SAVE & CLOSE", ImVec2(-FLT_MIN, 40))) {
            try {
                Core::invoice_manager().SaveInvoiceToCSV(path);
                Utils::logger().Log(LogLevel::SUCCESS, "Invoice Finalized: " + filename);
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& e) {
                Utils::logger().Log(LogLevel::ERR, "Final Save Failed! Check if CSV is open.");
            }
        }
        // --- NESTED POPUP: SEARCH & CREDIT ---
        if (show_credit_item) {
            ImGui::OpenPopup("CreditItemSelector");
        }
        // 1. Force the window to be a specific size before it opens
        ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);

        // 2. Remove 'AlwaysAutoResize' - it fights with the 'Sticky Footer' logic
        if (ImGui::BeginPopupModal("CreditItemSelector", &show_credit_item)) {

            // --- 1. SEARCH INPUT (Force to full width) ---
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::PushItemWidth(-FLT_MIN);
            ImGui::InputText("##credit_search_input", search_buf, sizeof(search_buf));
            ImGui::PopItemWidth();
            // --- 2. KEYBOARD SHORTCUT LOGIC ---
            int force_select_idx = -1;

            // Enter picks the top result
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                force_select_idx = 0;
            }

            // Ctrl + 1, 2, 3
            if (ImGui::GetIO().KeyCtrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_1)) force_select_idx = 0;
                if (ImGui::IsKeyPressed(ImGuiKey_2)) force_select_idx = 1;
                if (ImGui::IsKeyPressed(ImGuiKey_3)) force_select_idx = 2;
            }

            ImGui::Separator();

            if (ImGui::BeginChild("ScrollRegion", ImVec2(-FLT_MIN, -FLT_MIN), true)) {
                // 1. Lock the catalog and get the const reference
                std::lock_guard<std::mutex> lock(Core::catalog_manager().GetMutex());
                const auto& inventory = Core::catalog_manager().GetCatalog();

                std::vector<std::pair<int, std::string>> matches;

                std::string query = search_buf;
                std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                // 2. Filter results (Optimized: use the pre-computed lower strings if available)
                for (int i = 0; i < (int)inventory.size(); i++) {
                    // We can use the product's brand and name directly
                    std::string fullName = inventory[i].brand + " " + inventory[i].name;

                    // Search against the pre-computed lower-case strings we built in LoadInternal
                    // This is much faster than transforming a new string every frame!
                    if (query.empty() ||
                        inventory[i].brand_lower.find(query) != std::string::npos ||
                        inventory[i].name_lower.find(query) != std::string::npos)
                    {
                        matches.push_back({i, fullName});
                        if (matches.size() >= 50) break;
                    }
                }

                // Render Results
                for (int i = 0; i < (int)matches.size(); i++) {
                    int invIdx = matches[i].first;
                    auto& prod = inventory[invIdx];

                    ImGui::PushID(invIdx);

                    // Format label with shortcut prefix
                    std::string prefix = (i < 3) ? "[Ctrl+" + std::to_string(i + 1) + "] " : "       ";
                    std::string label = prefix + matches[i].second;

                    // Trigger if clicked OR if shortcut key was pressed
                    bool trigger_this = (i == force_select_idx);

                    if (ImGui::Selectable(label.c_str()) || trigger_this) {
                        if (prod.offers.contains("standard")) {
                            Core::invoice_manager().ApplyItemCredit(matches[i].second, prod.offers.at("standard").price);
                            search_buf[0] = '\0'; // Clear buffer
                            ImGui::CloseCurrentPopup();
                        }
                    }

                    // Enter key tooltip for the top item
                    if (i == 0 && ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Press ENTER to quickly add");
                    }

                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
            ImGui::EndPopup();
        }

        // --- NESTED POPUP: MANUAL USD ENTRY ---
        if (ImGui::BeginPopup("ManualCreditEntry")) {
            static float manual_val = 0.0f;
            ImGui::Text("Enter Credit Amount ($):");
            ImGui::InputFloat("##amt", &manual_val, 0.01f, 1.0f, "%.2f");

            if (ImGui::Button("Apply Credit")) {
                // Convert float USD to long long cents
                long long cents = static_cast<long long>(manual_val * 100.0f);
                Core::invoice_manager().ApplyManualCredit(cents); // Negative adjustment logic
                manual_val = 0.0f;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndPopup();
    }
}