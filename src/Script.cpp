#include "Script.h"

#include <imgui.h>

#include "utils.h"

Script::Script() {
    scriptPath = "";
}

Script::~Script() {
    lua.collect_garbage();
}

bool Script::Initialize(std::string scriptsPath) {
    if (!UDirectory::Exists(scriptsPath)) {
        std::cerr << "Scripts directory does not exist!" << std::endl;
        return false;
    }
    this->scriptPath = scriptsPath;
    binding();
    lua.open_libraries(sol::lib::base, sol::lib::io, sol::lib::math, sol::lib::os, sol::lib::string,
                       sol::lib::table, sol::lib::package, sol::lib::debug, sol::lib::count, sol::lib::coroutine);
    buildPackagePath(scriptsPath);
    if (!loadScript()) {
        return false;
    }
    return true;
}

sol::protected_function Script::getFunction(const std::string&funcName) {
    if (checkFunc(funcName)) {
        sol::protected_function func = lua[funcName];
        if (func.valid()) {
            return func;
        }
        std::cerr << "Function can not get: " << funcName << std::endl;
        return nullptr;
    }
    std::cerr << "Function not found: " << funcName << std::endl;
    return nullptr;
}


bool Script::checkFunc(const std::string&funcName) {
    for (const auto&pair: lua.globals()) {
        if (pair.second.is<sol::function>() && pair.first.as<std::string>() == funcName) {
            return true;
        }
    }
    return false;
}

void Script::PrintAllFunctions() {
    for (const auto&pair: lua.globals()) {
        if (pair.second.is<sol::function>()) {
            std::cout << pair.first.as<std::string>() << std::endl;
        }
    }
}

void Script::binding() {
    lua.set_function("ImVec4", sol::overload(
                         []() { return ImVec4(0, 0, 0, 0); },
                         [](float x) { return ImVec4(x, 0, 0, 0); },
                         [](float x, float y) { return ImVec4(x, y, 0, 0); },
                         [](float x, float y, float z) { return ImVec4(x, y, z, 0); },
                         [](float x, float y, float z, float w) { return ImVec4(x, y, z, w); }
                     ));


    lua.set_function("ImVec2", sol::overload(
                         []() { return ImVec2(0, 0); },
                         [](float x) { return ImVec2(x, 0); },
                         [](float x, float y) { return ImVec2(x, y); }
                     ));

    lua.set_function("UIBegin", sol::overload(
                         [](const char* name) { return ImGui::Begin(name); },
                         [](const char* name, bool* p_open) { return ImGui::Begin(name, p_open); },
                         [](const char* name, bool* p_open, ImGuiWindowFlags flags) {
                             return ImGui::Begin(name, p_open, flags);
                         }
                     ));
    lua.set_function("UIEnd", ImGui::End);

    // ImGui Text 函数的绑定
    lua.set_function("UIText", &ImGui::Text);

    lua.set_function("UItextUnformatted", sol::overload(
                         [](const char* text) { ImGui::TextUnformatted(text); },
                         [](const char* text, const char* text_end) { ImGui::TextUnformatted(text, text_end); }
                     ));

    lua.set_function("UItextColored", &ImGui::TextColored);

    lua.set_function("UItextColoredV", &ImGui::TextColored);

    lua.set_function("UItextDisabled", &ImGui::TextDisabled);

    lua.set_function("UItextWrapped", &ImGui::TextWrapped);

    lua.set_function("UIlabelText", &ImGui::LabelText);

    lua.set_function("UIbulletText", &ImGui::BulletText);

    lua.set_function("UIseparatorText", &ImGui::SeparatorText);

    lua.set_function("UIDragFloat", sol::overload(
                         [](const char* label, float* v, float v_speed, float v_min, float v_max,
                            const char* format, ImGuiSliderFlags flags) {
                             return ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, float v[2], float v_speed, float v_min, float v_max,
                            const char* format, ImGuiSliderFlags flags) {
                             return ImGui::DragFloat2(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, float v[3], float v_speed, float v_min, float v_max,
                            const char* format, ImGuiSliderFlags flags) {
                             return ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, float v[4], float v_speed, float v_min, float v_max,
                            const char* format, ImGuiSliderFlags flags) {
                             return ImGui::DragFloat4(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, float* v_current_min, float* v_current_max, float v_speed,
                            float v_min, float v_max, const char* format, const char* format_max,
                            ImGuiSliderFlags flags) {
                             return ImGui::DragFloatRange2(label, v_current_min, v_current_max, v_speed, v_min,
                                                           v_max, format, format_max, flags);
                         }
                     ));

    lua.set_function("UIDragInt", sol::overload(
                         [](const char* label, int* v, float v_speed, int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::DragInt(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, int v[2], float v_speed, int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::DragInt2(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, int v[3], float v_speed, int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::DragInt3(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, int v[4], float v_speed, int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::DragInt4(label, v, v_speed, v_min, v_max, format, flags);
                         },
                         [](const char* label, int* v_current_min, int* v_current_max, float v_speed, int v_min,
                            int v_max, const char* format, const char* format_max, ImGuiSliderFlags flags) {
                             return ImGui::DragIntRange2(label, v_current_min, v_current_max, v_speed, v_min, v_max,
                                                         format, format_max, flags);
                         }
                     ));

    lua.set_function("UIDragScalar", sol::overload(
                         [](const char* label, ImGuiDataType data_type, void* p_data, float v_speed,
                            const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
                             return ImGui::DragScalar(label, data_type, p_data, v_speed, p_min, p_max, format,
                                                      flags);
                         },
                         [](const char* label, ImGuiDataType data_type, void* p_data, int components, float v_speed,
                            const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
                             return ImGui::DragScalarN(label, data_type, p_data, components, v_speed, p_min, p_max,
                                                       format, flags);
                         }
                     ));

    lua.set_function("UISliderFloat", sol::overload(
                         [](const char* label, float* v, float v_min, float v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, float v[2], float v_min, float v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderFloat2(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, float v[3], float v_min, float v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderFloat3(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, float v[4], float v_min, float v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderFloat4(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, float* v, float v_min, float v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderAngle(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, int* v, int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderInt(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, int v[2], int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderInt2(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, int v[3], int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderInt3(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, int v[4], int v_min, int v_max, const char* format,
                            ImGuiSliderFlags flags) {
                             return ImGui::SliderInt4(label, v, v_min, v_max, format, flags);
                         },
                         [](const char* label, ImGuiDataType data_type, void* p_data, const void* p_min,
                            const void* p_max, const char* format, ImGuiSliderFlags flags) {
                             return ImGui::SliderScalar(label, data_type, p_data, p_min, p_max, format, flags);
                         },
                         [](const char* label, ImGuiDataType data_type, void* p_data, int components,
                            const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
                             return ImGui::SliderScalarN(label, data_type, p_data, components, p_min, p_max, format,
                                                         flags);
                         }
                     ));

    // 绑定 Button 函数
    lua.set_function("UIButton", sol::overload(
                         [](const char* label) { return ImGui::Button(label); },
                         [](const char* label, const ImVec2&size) { return ImGui::Button(label, size); }
                     ));

    // 绑定 Checkbox 函数
    lua.set_function("UICheckbox", &ImGui::Checkbox);

    // 绑定 RadioButton 函数
    lua.set_function("UIRadioButton", sol::overload(
                         [](const char* label, bool active) { return ImGui::RadioButton(label, active); },
                         [](const char* label, int* v, int v_button) {
                             return ImGui::RadioButton(label, v, v_button);
                         }
                     ));

    // 绑定 ProgressBar 函数
    lua.set_function("ProgressBar", sol::overload(
                         [](float fraction, const ImVec2&size_arg, const char* overlay) {
                             ImGui::ProgressBar(fraction, size_arg, overlay);
                         },
                         [](float fraction, const ImVec2&size_arg) {
                             ImGui::ProgressBar(fraction, size_arg);
                         },
                         [](float fraction) {
                             ImGui::ProgressBar(fraction);
                         }
                     ));

    // 绑定 Bullet 函数
    lua.set_function("UIBullet", &ImGui::Bullet);

    // 绑定 Image 函数
    lua.set_function("UIImage", sol::overload(
                         [](ImTextureID user_texture_id, const ImVec2&size, const ImVec2&uv0, const ImVec2&uv1,
                            const ImVec4&tint_col, const ImVec4&border_col) {
                             ImGui::Image(user_texture_id, size, uv0, uv1, tint_col, border_col);
                         },
                         [](ImTextureID user_texture_id, const ImVec2&size, const ImVec2&uv0, const ImVec2&uv1,
                            const ImVec4&tint_col) {
                             ImGui::Image(user_texture_id, size, uv0, uv1, tint_col,{});
                         },
                         [](ImTextureID user_texture_id, const ImVec2&size, const ImVec2&uv0, const ImVec2&uv1) {
                             ImGui::Image(user_texture_id, size, uv0, uv1);
                         },
                         [](ImTextureID user_texture_id, const ImVec2&size) {
                             ImGui::Image(user_texture_id, size);
                         }
                     ));
    lua.set_function("UIImageButton", sol::overload(
                         [](const char* str_id, ImTextureID user_texture_id, const ImVec2&size, const ImVec2&uv0,
                            const ImVec2&uv1, const ImVec4&bg_col, const ImVec4&tint_col) {
                             return ImGui::ImageButton(str_id, user_texture_id, size, uv0, uv1, bg_col, tint_col);
                         },
                         [](const char* str_id, ImTextureID user_texture_id, const ImVec2&size, const ImVec2&uv0,
                            const ImVec2&uv1, const ImVec4&bg_col) {
                             return ImGui::ImageButton(str_id, user_texture_id, size, uv0, uv1, bg_col);
                         },
                         [](const char* str_id, ImTextureID user_texture_id, const ImVec2&size, const ImVec2&uv0,
                            const ImVec2&uv1) {
                             return ImGui::ImageButton(str_id, user_texture_id, size, uv0, uv1);
                         },
                         [](const char* str_id, ImTextureID user_texture_id, const ImVec2&size) {
                             return ImGui::ImageButton(str_id, user_texture_id, size);
                         }
                     ));

    // 绑定 Combo 函数
    lua.set_function("UICombo", sol::overload(
                         [](const char* label, int* current_item, const char* const items[], int items_count,
                            int popup_max_height_in_items) {
                             return ImGui::Combo(label, current_item, items, items_count,
                                                 popup_max_height_in_items);
                         },
                         [](const char* label, int* current_item, const char* items_separated_by_zeros,
                            int popup_max_height_in_items) {
                             return ImGui::Combo(label, current_item, items_separated_by_zeros,
                                                 popup_max_height_in_items);
                         }
                     ));

    lua.set_function("UIBeginCombo", sol::overload(
                         [](const char* label, const char* preview_value, ImGuiComboFlags flags) {
                             return ImGui::BeginCombo(label, preview_value, flags);
                         },
                         [](const char* label, const char* preview_value) {
                             return ImGui::BeginCombo(label, preview_value);
                         }
                     ));

    lua.set_function("UIEndCombo", &ImGui::EndCombo);

    lua.set_function("UISameLine", sol::overload(
                         [](float offset_from_start_x, float spacing) {
                             ImGui::SameLine(offset_from_start_x, spacing);
                         },
                         [](float offset_from_start_x) {
                             ImGui::SameLine(offset_from_start_x);
                         },
                         []() {
                             ImGui::SameLine();
                         }
                     ));

    lua.set_function("UINewLine", &ImGui::NewLine);

    lua.set_function("UISeparator", &ImGui::Separator);

    lua.set_function("UISpacing", &ImGui::Spacing);

    lua.set_function("UIDummy", &ImGui::Dummy);

    lua.set_function("UIIndent", sol::overload(
                         [](float indent_w) { ImGui::Indent(indent_w); },
                         []() { ImGui::Indent(); }
                     ));

    lua.set_function("UIUnindent", sol::overload(
                         [](float indent_w) { ImGui::Unindent(indent_w); },
                         []() { ImGui::Unindent(); }
                     ));

    lua.set_function("UIBeginGroup", &ImGui::BeginGroup);

    lua.set_function("UIEndGroup", &ImGui::EndGroup);

    lua.set_function("UIGetCursorPos", &ImGui::GetCursorPos);

    lua.set_function("UIGetCursorPosX", &ImGui::GetCursorPosX);

    lua.set_function("UIGetCursorPosY", &ImGui::GetCursorPosY);

    lua.set_function("UISetCursorPos", &ImGui::SetCursorPos);

    lua.set_function("UISetCursorPosX", &ImGui::SetCursorPosX);

    lua.set_function("UISetCursorPosY", &ImGui::SetCursorPosY);

    lua.set_function("UIGetCursorStartPos", &ImGui::GetCursorStartPos);

    lua.set_function("UIGetCursorScreenPos", &ImGui::GetCursorScreenPos);

    lua.set_function("UISetCursorScreenPos", &ImGui::SetCursorScreenPos);

    lua.set_function("UIAlignTextToFramePadding", &ImGui::AlignTextToFramePadding);

    lua.set_function("UIGetTextLineHeight", &ImGui::GetTextLineHeight);

    lua.set_function("UIGetTextLineHeightWithSpacing", &ImGui::GetTextLineHeightWithSpacing);

    lua.set_function("UIGetFrameHeight", &ImGui::GetFrameHeight);

    lua.set_function("UIGetFrameHeightWithSpacing", &ImGui::GetFrameHeightWithSpacing);

    lua.set_function("UISliderAngle",
                     [](const char* label, float* v_rad, float v_degrees_min, float v_degrees_max,
                        const char* format, ImGuiSliderFlags flags) {
                         return ImGui::SliderAngle(label, v_rad, v_degrees_min, v_degrees_max, format, flags);
                     });

    lua.set_function("UIVSliderFloat",
                     [](const char* label, const ImVec2&size, float* v, float v_min, float v_max,
                        const char* format, ImGuiSliderFlags flags) {
                         return ImGui::VSliderFloat(label, size, v, v_min, v_max, format, flags);
                     });

    lua.set_function("UIVSliderInt",
                     [](const char* label, const ImVec2&size, int* v, int v_min, int v_max, const char* format,
                        ImGuiSliderFlags flags) {
                         return ImGui::VSliderInt(label, size, v, v_min, v_max, format, flags);
                     });

    lua.set_function("UIVSliderScalar",
                     [](const char* label, const ImVec2&size, ImGuiDataType data_type, void* p_data,
                        const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
                         return ImGui::VSliderScalar(label, size, data_type, p_data, p_min, p_max, format, flags);
                     });

    lua.set_function("UIInputText", [](const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags) {
        return ImGui::InputText(label, buf, buf_size, flags);
    });

    lua.set_function("UIInputTextMultiline",
                     [](const char* label, char* buf, size_t buf_size, const ImVec2&size,
                        ImGuiInputTextFlags flags) {
                         return ImGui::InputTextMultiline(label, buf, buf_size, size, flags);
                     });

    lua.set_function("UIInputTextWithHint",
                     [](const char* label, const char* hint, char* buf, size_t buf_size,
                        ImGuiInputTextFlags flags) {
                         return ImGui::InputTextWithHint(label, hint, buf, buf_size, flags);
                     });

    lua.set_function("UIInputFloat",
                     [](const char* label, float* v, float step, float step_fast, const char* format,
                        ImGuiInputTextFlags flags) {
                         return ImGui::InputFloat(label, v, step, step_fast, format, flags);
                     });

    lua.set_function("UIInputFloat2",
                     [](const char* label, float v[2], const char* format, ImGuiInputTextFlags flags) {
                         return ImGui::InputFloat2(label, v, format, flags);
                     });

    lua.set_function("UIInputFloat3",
                     [](const char* label, float v[3], const char* format, ImGuiInputTextFlags flags) {
                         return ImGui::InputFloat3(label, v, format, flags);
                     });

    lua.set_function("UIInputFloat4",
                     [](const char* label, float v[4], const char* format, ImGuiInputTextFlags flags) {
                         return ImGui::InputFloat4(label, v, format, flags);
                     });

    lua.set_function("UIInputInt",
                     [](const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags) {
                         return ImGui::InputInt(label, v, step, step_fast, flags);
                     });

    lua.set_function("UIInputInt2", [](const char* label, int v[2], ImGuiInputTextFlags flags) {
        return ImGui::InputInt2(label, v, flags);
    });

    lua.set_function("UIInputInt3", [](const char* label, int v[3], ImGuiInputTextFlags flags) {
        return ImGui::InputInt3(label, v, flags);
    });

    lua.set_function("UIInputInt4", [](const char* label, int v[4], ImGuiInputTextFlags flags) {
        return ImGui::InputInt4(label, v, flags);
    });

    lua.set_function("UIInputDouble",
                     [](const char* label, double* v, double step, double step_fast, const char* format,
                        ImGuiInputTextFlags flags) {
                         return ImGui::InputDouble(label, v, step, step_fast, format, flags);
                     });

    lua.set_function("UIInputScalar",
                     [](const char* label, ImGuiDataType data_type, void* p_data, const void* p_step,
                        const void* p_step_fast, const char* format, ImGuiInputTextFlags flags) {
                         return ImGui::InputScalar(label, data_type, p_data, p_step, p_step_fast, format, flags);
                     });

    lua.set_function("UIInputScalarN",
                     [](const char* label, ImGuiDataType data_type, void* p_data, int components,
                        const void* p_step, const void* p_step_fast, const char* format,
                        ImGuiInputTextFlags flags) {
                         return ImGui::InputScalarN(label, data_type, p_data, components, p_step, p_step_fast,
                                                    format, flags);
                     });

    lua.set_function("UIColorEdit3", [](const char* label, float col[3], ImGuiColorEditFlags flags) {
        return ImGui::ColorEdit3(label, col, flags);
    });

    lua.set_function("UIColorEdit4", [](const char* label, float col[4], ImGuiColorEditFlags flags) {
        return ImGui::ColorEdit4(label, col, flags);
    });

    lua.set_function("UIColorPicker3", [](const char* label, float col[3], ImGuiColorEditFlags flags) {
        return ImGui::ColorPicker3(label, col, flags);
    });

    lua.set_function("UIColorPicker4",
                     [](const char* label, float col[4], ImGuiColorEditFlags flags, const float* ref_col) {
                         return ImGui::ColorPicker4(label, col, flags, ref_col);
                     });

    lua.set_function("UIBeginListBox", [](const char* label, const ImVec2&size) {
        return ImGui::BeginListBox(label, size);
    });

    lua.set_function("UIEndListBox", []() {
        ImGui::EndListBox();
    });

    lua.set_function("UIListBox",
                     [](const char* label, int* current_item, const sol::table&items, int height_in_items) {
                         std::vector<const char *> item_array;
                         for (auto&item: items) {
                             item_array.push_back(item.second.as<const char *>());
                         }
                         return ImGui::ListBox(label, current_item, item_array.data(),
                                               static_cast<int>(item_array.size()), height_in_items);
                     });

    lua.set_function("LoadImage", &UImage::Img2Texture);
}

bool Script::loadScript() {
    auto script = scriptPath + "/Plugin.lua";
    std::ifstream scriptFile(script);
    if (!scriptFile.is_open()) {
        std::cerr << "Unable to open the script file: " << script << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << scriptFile.rdbuf();
    scriptFile.close();
    result = lua.script(buffer.str(), sol::script_default_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Error loading script: " << err.what() << std::endl;
        return false;
    }

    return true;
}

void Script::buildPackagePath(const std::string&rootPath) {
    std::string package_path = lua["package"]["path"].get<std::string>();

    std::string rootLuaPath = Utils::GetAbsolutePath(UFile::PlatformPath(rootPath + "/?.lua"));
    package_path += ";" + rootLuaPath;

    for (const auto&entry: std::filesystem::recursive_directory_iterator(rootPath)) {
        if (entry.is_directory()) {
            std::string dirPath = entry.path().string();
            std::string luaPath = dirPath + "/?.lua";
            package_path += ";" + Utils::GetAbsolutePath(UFile::PlatformPath(luaPath));
        }
    }
#ifndef NDEBUG
    std::cout << "package.path: " << package_path << std::endl;
#endif

    lua["package"]["path"] = package_path;
}
