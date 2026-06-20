#include "base/base.h"
#define LOG_IMPLEMENTATION
#include "base/log.h"

#include <cstdlib>
#include <cstdio>
#include <filesystem>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <misc/cpp/imgui_stdlib.h>
#include <GLFW/glfw3.h>

#include "fonts/DroidSansMNerdFontMono-Regular.font.h"

#include <cppcodec/base64_rfc4648.hpp>

#include <nfd.h>
#include <nfd_glfw3.h>

#include <google/protobuf/util/json_util.h>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/reflection.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/compiler/importer.h>

#define ENCODED_BUF_SIZE 4096
#define MAX_PATH_LENGTH 256
#define MAX_TABS 10

#define SPLIT_EQUAL FLT_MIN

#define ICON_FILE_BROWSER ""
#define ICON_CROSS ""
#define ICON_PLUS ""

namespace fs = std::filesystem;
namespace proto = google::protobuf;
namespace protoc = google::protobuf::compiler;

static bool tree_collapse_all = false;

static void glfw_error_callback(int error, const char* description) {
    LOG_ERR("GLFW Error %d: %s", error, description);
}

static void draw_msg(proto::Message* msg);

static void draw_field_by_type(proto::Message *msg,
                               const proto::Reflection *reflection,
                               const proto::FieldDescriptor *field) {
    const char *label = field->name().data();

    switch (field->cpp_type()) {
        using namespace proto;
        case FieldDescriptor::CPPTYPE_INT32: {
            i32 v = reflection->GetInt32(*msg, field);
            if (ImGui::InputInt(label, &v)) {
                reflection->SetInt32(msg, field, v);
            }
        } break;
        case FieldDescriptor::CPPTYPE_INT64: {
            i64 v = reflection->GetInt64(*msg, field);
            i64 step = 1;
            if (ImGui::InputScalar(label, ImGuiDataType_S64, &v, &step)) {
                reflection->SetInt64(msg, field, v);
            }
        } break;
        case FieldDescriptor::CPPTYPE_UINT32: {
            u32 v = reflection->GetUInt32(*msg, field);
            u32 step = 1;
            if (ImGui::InputScalar(label, ImGuiDataType_U32, &v, &step)) {
                reflection->SetUInt32(msg, field, v);
            }
        } break;
        case FieldDescriptor::CPPTYPE_UINT64: {
            u64 v = reflection->GetUInt64(*msg, field);
            u64 step = 1;
            if (ImGui::InputScalar(label, ImGuiDataType_U64, &v, &step)) {
                reflection->SetUInt64(msg, field, v);
            }
        } break;
        case FieldDescriptor::CPPTYPE_FLOAT: {
            f32 v = reflection->GetFloat(*msg, field);
            f32 step = 0.1f;
            if (ImGui::InputFloat(label, &v, step)) {
                reflection->SetFloat(msg, field, v);
            }
        } break;
        case FieldDescriptor::CPPTYPE_DOUBLE: {
            f64 v = reflection->GetDouble(*msg, field);
            f64 step = 0.1;
            if (ImGui::InputDouble(label, &v, step)) {
                reflection->SetDouble(msg, field, v);
            }
        } break;
        case FieldDescriptor::CPPTYPE_BOOL: {
            bool v = reflection->GetBool(*msg, field);
            if (ImGui::Checkbox(label, &v)) {
                reflection->SetBool(msg, field, v);
            }
        } break;
        case FieldDescriptor::CPPTYPE_ENUM: {
            const auto *enum_desc = field->enum_type();
            i32 v = reflection->GetEnumValue(*msg, field);
            if (ImGui::BeginCombo(label, enum_desc->FindValueByNumber(v)->name().data())) {
                for (u8 e = 0; e < enum_desc->value_count(); ++e) {
                    const auto *enum_val = enum_desc->value(e);
                    if (ImGui::Selectable(enum_val->name().data(), enum_val->number() == v)) {
                        reflection->SetEnumValue(msg, field, enum_val->number());
                    }
                }
                ImGui::EndCombo();
            }
        } break;
        case FieldDescriptor::CPPTYPE_STRING: {
            std::string v = reflection->GetString(*msg, field);
            if (ImGui::InputText(label, &v)) {
                reflection->SetString(msg, field, v);
            }
        } break;
        case FieldDescriptor::CPPTYPE_MESSAGE: {
            auto *submsg = reflection->MutableMessage(msg, field);
            if (tree_collapse_all) {
                ImGui::SetNextItemOpen(false);
            }
            if (ImGui::TreeNode(label)) {
                draw_msg(submsg);
                ImGui::TreePop();
            }
        } break;
    }
}

static void draw_repeated_field(proto::Message *msg,
                                const proto::Reflection *reflection,
                                const proto::FieldDescriptor *field,
                                u8 count) {
    for (u8 i = 0; i < count; ++i) {
        ImGui::PushID(i);
        ImGui::Text("%d:", i);
        ImGui::SameLine(0, 0);
        switch (field->cpp_type()) {
            using namespace proto;
            case FieldDescriptor::CPPTYPE_INT32: {
                i32 v = reflection->GetRepeatedInt32(*msg, field, i);
                if (ImGui::InputInt("##repeated", &v)) {
                    reflection->SetRepeatedInt32(msg, field, i, v);
                }
            } break;
            case FieldDescriptor::CPPTYPE_INT64: {
                i64 v = reflection->GetRepeatedInt64(*msg, field, i);
                i64 step = 1;
                if (ImGui::InputScalar("##repeated", ImGuiDataType_S64, &v, &step)) {
                    reflection->SetRepeatedInt64(msg, field, i, v);
                }
            } break;
            case FieldDescriptor::CPPTYPE_UINT32: {
                u32 v = reflection->GetRepeatedUInt32(*msg, field, i);
                u32 step = 1;
                if (ImGui::InputScalar("##repeated", ImGuiDataType_U32, &v, &step)) {
                    reflection->SetRepeatedUInt32(msg, field, i, v);
                }
            } break;
            case FieldDescriptor::CPPTYPE_UINT64: {
                u64 v = reflection->GetRepeatedUInt64(*msg, field, i);
                u64 step = 1;
                if (ImGui::InputScalar("##repeated", ImGuiDataType_U64, &v, &step)) {
                    reflection->SetRepeatedUInt64(msg, field, i, v);
                }
            } break;
            case FieldDescriptor::CPPTYPE_FLOAT: {
                f32 v = reflection->GetRepeatedFloat(*msg, field, i);
                f32 step = 0.1f;
                if (ImGui::InputScalar("##repeated", ImGuiDataType_Float, &v, &step)) {
                    reflection->SetRepeatedFloat(msg, field, i, v);
                }
            } break;
            case FieldDescriptor::CPPTYPE_DOUBLE: {
                f64 v = reflection->GetRepeatedDouble(*msg, field, i);
                f64 step = 0.1;
                if (ImGui::InputScalar("##repeated", ImGuiDataType_Double, &v, &step)) {
                    reflection->SetRepeatedDouble(msg, field, i, v);
                }
            } break;
            case FieldDescriptor::CPPTYPE_BOOL: {
                bool v = reflection->GetRepeatedBool(*msg, field, i);
                if (ImGui::Checkbox("##repeated", &v)) {
                    reflection->SetRepeatedBool(msg, field, i, v);
                }
            } break;
            case FieldDescriptor::CPPTYPE_ENUM: {
                const auto *enum_desc = field->enum_type();
                i32 v = reflection->GetRepeatedEnumValue(*msg, field, i);
                if (ImGui::BeginCombo("##repeated", enum_desc->FindValueByNumber(v)->name().data())) {
                    for (u8 e = 0; e < enum_desc->value_count(); ++e) {
                        const auto *enum_val = enum_desc->value(e);
                        if (ImGui::Selectable(enum_val->name().data(), enum_val->number() == v)) {
                            reflection->SetRepeatedEnumValue(msg, field, i, enum_val->number());
                        }
                    }
                    ImGui::EndCombo();
                }
            } break;
            case FieldDescriptor::CPPTYPE_STRING: {
                std::string v = reflection->GetRepeatedString(*msg, field, i);
                if (ImGui::InputText("##repeated", &v)) {
                    reflection->SetRepeatedString(msg, field, i, v);
                }
            } break;
            case FieldDescriptor::CPPTYPE_MESSAGE: {
                auto *submsg = reflection->MutableRepeatedMessage(msg, field, i);
                if (tree_collapse_all) {
                    ImGui::SetNextItemOpen(false);
                } else {
                    ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
                }
                if (ImGui::TreeNode(submsg->GetDescriptor()->name().data())) {
                    draw_msg(submsg);
                    ImGui::TreePop();
                }
            } break;
        }
        ImGui::PopID();
    }
}

static void draw_oneof_field(proto::Message *msg,
                             const proto::Reflection *reflection,
                             const proto::OneofDescriptor *oneof,
                             const proto::FieldDescriptor *active = nullptr) {
    const char *preview = active ? active->name().data() : "";
    if (ImGui::BeginCombo(oneof->name().data(), preview)) {
        for (u8 i = 0; i < oneof->field_count(); ++i) {
            const auto *oneof_option = oneof->field(i);
            if (ImGui::Selectable(oneof_option->name().data(), oneof_option == active)) {
                switch (oneof_option->cpp_type()) {
                    using namespace proto;
                    case FieldDescriptor::CPPTYPE_INT32: {
                        reflection->SetInt32(msg, oneof_option, 0);
                    } break;
                    case FieldDescriptor::CPPTYPE_INT64: {
                        reflection->SetInt64(msg, oneof_option, 0);
                    } break;
                    case FieldDescriptor::CPPTYPE_UINT32: {
                        reflection->SetUInt32(msg, oneof_option, 0);
                    } break;
                    case FieldDescriptor::CPPTYPE_UINT64: {
                        reflection->SetUInt64(msg, oneof_option, 0);
                    } break;
                    case FieldDescriptor::CPPTYPE_FLOAT: {
                        reflection->SetFloat(msg, oneof_option, 0.0f);
                    } break;
                    case FieldDescriptor::CPPTYPE_DOUBLE: {
                        reflection->SetDouble(msg, oneof_option, 0.0);
                    } break;
                    case FieldDescriptor::CPPTYPE_BOOL: {
                        reflection->SetBool(msg, oneof_option, false);
                    } break;
                    case FieldDescriptor::CPPTYPE_ENUM: {
                        reflection->SetEnum(msg, oneof_option, oneof_option->enum_type()->FindValueByNumber(0));
                    } break;
                    case FieldDescriptor::CPPTYPE_STRING: {
                        reflection->SetString(msg, oneof_option, "");
                    } break;
                    case FieldDescriptor::CPPTYPE_MESSAGE: {
                        reflection->MutableMessage(msg, oneof_option);
                    } break;
                }
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::PushID(oneof->name().data());
    if (!active) {
        ImGui::BeginDisabled();
    }
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
    if (ImGui::Button(ICON_CROSS)) {
        reflection->ClearOneof(msg, oneof);
    }
    ImGui::PopStyleColor(3);
    ImGui::SetItemTooltip("reset oneof");
    if (!active) {
        ImGui::EndDisabled();
    }
    ImGui::PopID();
}

static void draw_field(proto::Message *msg,
                       const proto::Reflection *reflection,
                       const proto::FieldDescriptor *field) {
    if (field->is_repeated()) {
        ImGui::PushID(field->name().data());
        u8 count = reflection->FieldSize(*msg, field);
        bool btn_minus_disabled = count == 0;
        if (btn_minus_disabled) {
            ImGui::BeginDisabled();
        }
        f32 button_size = ImGui::GetFrameHeight();
        if (ImGui::Button("-", ImVec2(button_size, button_size)) && count > 0) {
            reflection->RemoveLast(msg, field);
            count -= 1;
        }
        ImGui::SetItemTooltip("remove repeated item");
        if (btn_minus_disabled) {
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        ImGui::Text("%s", field->name().data());
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(button_size, button_size))) {
            switch (field->cpp_type()) {
                using namespace proto;
                case FieldDescriptor::CPPTYPE_INT32: {
                    reflection->AddInt32(msg, field, 0);
                } break;
                case FieldDescriptor::CPPTYPE_INT64: {
                    reflection->AddInt64(msg, field, 0);
                } break;
                case FieldDescriptor::CPPTYPE_UINT32: {
                    reflection->AddUInt32(msg, field, 0);
                } break;
                case FieldDescriptor::CPPTYPE_UINT64: {
                    reflection->AddUInt64(msg, field, 0);
                } break;
                case FieldDescriptor::CPPTYPE_FLOAT: {
                    reflection->AddFloat(msg, field, 0.0f);
                } break;
                case FieldDescriptor::CPPTYPE_DOUBLE: {
                    reflection->AddDouble(msg, field, 0.0);
                } break;
                case FieldDescriptor::CPPTYPE_BOOL: {
                    reflection->AddBool(msg, field, false);
                } break;
                case FieldDescriptor::CPPTYPE_ENUM: {
                    reflection->AddEnum(msg, field, field->enum_type()->FindValueByNumber(0));
                } break;
                case FieldDescriptor::CPPTYPE_STRING: {
                    reflection->AddString(msg, field, "");
                } break;
                case FieldDescriptor::CPPTYPE_MESSAGE: {
                    reflection->AddMessage(msg, field);
                } break;
            }
        }
        ImGui::SetItemTooltip("add repeated item");
        ImGui::PopID();
        draw_repeated_field(msg, reflection, field, count);
        return;
    }

    auto oneof = field->containing_oneof();
    if (oneof) {
        const auto *active = reflection->GetOneofFieldDescriptor(*msg, oneof);
        if (active == nullptr) {
            if (field == oneof->field(0)) {
                draw_oneof_field(msg, reflection, oneof);
            }
        } else if (active == field) {
            draw_oneof_field(msg, reflection, oneof, active);
            // TODO: bruh...
            const auto *active = reflection->GetOneofFieldDescriptor(*msg, oneof);
            if (active) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
                draw_field_by_type(msg, reflection, active);
            }
        }
    } else {
        draw_field_by_type(msg, reflection, field);
    }
}

static void draw_msg(proto::Message* msg) {
    const auto *descriptor = msg->GetDescriptor();
    const auto *reflection = msg->GetReflection();

    ImGui::PushID(msg);

    for (u8 i = 0; i < descriptor->field_count(); ++i) {
        const auto *field = descriptor->field(i);
        draw_field(msg, reflection, field);
    }

    ImGui::PopID();
}

enum class view {
    import,
    model,
};

class ErrorCollector : public protoc::MultiFileErrorCollector {
    public:
        void AddError(const std::string& filename, int line, int column,
                const std::string& message) override {
            LOG_ERR("%s:%d:%d: %s", filename.c_str(), line, column, message.c_str());
            exit(EXIT_FAILURE);
        }
} error_collector;

// TODO: move to header
extern void set_native_window(GLFWwindow *glfw_window, nfdwindowhandle_t *native_window);

static struct TabItem {
    view active_view;
    char path[MAX_PATH_LENGTH];
    bool want_focus;
    proto::Message *msg;
    const proto::FileDescriptor *file_desc;
    bool should_close;
} tab_items[MAX_TABS];
static usize tabs_opened = 1;

int main(i32 argc, const char *argv[]) {
    using base64 = cppcodec::base64_rfc4648;
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    log_init(LOG_DBG);

    proto::DynamicMessageFactory factory;
    protoc::DiskSourceTree source_tree;
    protoc::Importer importer(&source_tree, &error_collector);

    if (argc >= 2) {
        fs::path proto_path{argv[1]};
        source_tree.MapPath("", proto_path.parent_path().c_str());
        if (!fs::exists(proto_path) || !fs::is_regular_file(proto_path)) {
            std::cerr << "file '" << argv[1] << "' not found" << std::endl;
            exit(EXIT_FAILURE);
        }

        TabItem *first_tab = &tab_items[0];
        first_tab->file_desc = importer.Import(proto_path.filename());
        if (first_tab->file_desc->message_type_count() == 1) {
            const proto::Descriptor *desc = first_tab->file_desc->message_type(0);
            first_tab->msg = factory.GetPrototype(desc)->New();
        } else {
            const proto::Descriptor *desc = first_tab->file_desc->FindMessageTypeByName(argv[2]);
            if (desc == nullptr) {
                std::cerr << "message '" << argv[2] << "' not found, possible options:\n";
                for (i32 i = 0; i < first_tab->file_desc->message_type_count(); ++i) {
                    std::cerr << "    " << first_tab->file_desc->message_type(i)->name() << '\n';
                }
                std::flush(std::cerr);
                exit(EXIT_FAILURE);
            }
            first_tab->msg = factory.GetPrototype(desc)->New();
        }

        first_tab->active_view = view::model;
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        LOG_ERR("glfwInit failed");
        exit(EXIT_FAILURE);
    }

    if (NFD_Init() != NFD_OKAY) {
        LOG_ERR("NFD_Init failed: %s", NFD_GetError());
        exit(EXIT_FAILURE);
    }

    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    f32 main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWwindow *window = glfwCreateWindow((i32)(1280 * main_scale), (i32)(720 * main_scale), "protoed", nullptr, nullptr);
    if (window == nullptr) {
        LOG_ERR("glfwCreateWindow failed");
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // enable vsync

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;

    io.Fonts->ClearFonts();
    io.Fonts->AddFontFromMemoryCompressedTTF(custom_font_compressed_data, custom_font_compressed_size);

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.ChildRounding = 6.0f;
    style.FontScaleDpi = main_scale;
    style.FontSizeBase = 20;
    style.FrameBorderSize = 1.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarPadding = 1.0f;
    style.ScrollbarSize = 8.0f;
    style.TabRounding = 6.0f;
    style.WindowRounding = 6.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.42f, 0.83f, 0.63f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.44f, 0.71f, 0.57f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.43f, 0.71f, 0.57f, 0.71f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.44f, 0.71f, 0.57f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.42f, 0.83f, 0.63f, 0.58f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.44f, 0.71f, 0.57f, 0.40f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.44f, 0.71f, 0.57f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.44f, 0.71f, 0.57f, 0.80f);
    colors[ImGuiCol_Header]                 = ImVec4(0.44f, 0.71f, 0.57f, 0.31f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.24f, 0.24f, 0.24f, 0.35f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.44f, 0.71f, 0.57f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.43f, 0.71f, 0.57f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.44f, 0.71f, 0.57f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline]    = ImVec4(0.42f, 0.83f, 0.63f, 1.00f);
    colors[ImGuiCol_TabSelected]            = ImVec4(0.35f, 0.64f, 0.50f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.44f, 0.71f, 0.57f, 0.59f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.44f, 0.71f, 0.57f, 0.35f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.43f, 0.71f, 0.57f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

    f32 left_pane_width = SPLIT_EQUAL;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); 
        ImGui::Begin("Main", NULL, ImGuiWindowFlags_NoMove
                                 | ImGuiWindowFlags_NoDecoration);

        static usize current_tab = -1UL;
        if (current_tab != -1UL && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_W)) {
            tab_items[current_tab].should_close = true;
        }

        // disable default ctrl+tab behavior
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Tab, ImGuiInputFlags_RouteGlobal);
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Tab, ImGuiInputFlags_RouteGlobal);

        // TODO: tab switching needs polish
        static isize selected_tab = -1;
        bool keyboard_tab_switch_requested = false;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Tab, true)) {
            if (io.KeyShift) {
                selected_tab = (selected_tab - 1) % tabs_opened;
            } else {
                selected_tab = (selected_tab + 1) % tabs_opened;
            }
            keyboard_tab_switch_requested = true;
        }

        for (u8 i = 0; i < 9; ++i) {
            if (ImGui::IsKeyChordPressed(ImGuiMod_Alt | (ImGuiKey_1 + i))) {
                selected_tab = CLAMP_TOP(i, tabs_opened - 1);
                keyboard_tab_switch_requested = true;
            }
        }
        if (ImGui::IsKeyChordPressed(ImGuiMod_Alt | ImGuiKey_0)) {
            selected_tab = tabs_opened - 1;
            keyboard_tab_switch_requested = true;
        }

        // TODO: ImGuiTabBarFlags_Reorderable 
        ImGui::PushStyleVarX(ImGuiStyleVar_ItemInnerSpacing, 0.5f);
        if (ImGui::BeginTabBar("Tabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll)) {
            if (tabs_opened < MAX_TABS) {
                ImGui::PushStyleColor(ImGuiCol_Tab,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
                ImGui::PushStyleColor(ImGuiCol_TabActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
                if (ImGui::TabItemButton(ICON_PLUS, ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)
                 || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_T)) {
                    tabs_opened += 1;
                }
                ImGui::PopStyleColor(3);
            }
            ImGui::PopStyleVar(); // ImGuiStyleVar_ItemInnerSpacing

            if (tabs_opened == 0) {
                MEMORY_ZERO_STRUCT(&tab_items[0]);
                tabs_opened = 1;
            }

            for (usize t = 0; t < tabs_opened; ++t) {
                bool open = true;
                TabItem *tab = &tab_items[t];

                ImGui::PushID(t);
                if (ImGui::BeginTabItem(tab->msg ? tab->msg->GetDescriptor()->name().c_str() : "new tab",
                                        &open,
                                        keyboard_tab_switch_requested && t == (usize)selected_tab
                                            ? ImGuiTabItemFlags_SetSelected
                                            : ImGuiTabItemFlags_None)) {
                    f32 wheel = io.MouseWheel;

                    ImVec2 avail = ImGui::GetContentRegionAvail();

                    if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Equal) || wheel > 0.0f)) {
                        style.FontScaleDpi += 0.1f;
                    } else if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Minus) || wheel < 0.0f) && style.FontScaleDpi > 0.5f) {
                        style.FontScaleDpi -= 0.1f;
                    } else if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_0) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle))) {
                        style.FontScaleDpi = 1.0f;
                        left_pane_width = SPLIT_EQUAL;
                    }

                    if (left_pane_width == SPLIT_EQUAL) {
                        left_pane_width = avail.x / 2.0f;
                    }

                    bool just_activated = current_tab != t;
                    current_tab = t;

                    if (just_activated) {
                        ImGui::SetKeyboardFocusHere();
                    }

                    switch (tab->active_view) {
                        case view::import: {
                            ImVec2 group_size = ImVec2(avail.x * 0.8, avail.y * 0.8);
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - group_size.x) / 2.0f);
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (avail.y - group_size.y) / 2.0f);

                            ImGui::BeginGroup();
                            ImGui::PushItemWidth(avail.x * 0.8);

                            ImVec2 clip_rect_min = ImGui::GetCursorScreenPos();
                            ImVec2 clip_rect_max = ImVec2(clip_rect_min.x + group_size.x, avail.y);
                            ImGui::PushClipRect(clip_rect_min, clip_rect_max, true);
                            ImGui::SeparatorText("proto file path");
                            ImGui::PopClipRect();

                            // TODO: why does not this input get cleared when closing tab with ctrl+w?
                            ImGui::InputText("##proto_path", tab->path, sizeof(tab->path), ImGuiInputTextFlags_ElideLeft);
                            ImGui::SetItemTooltip("path to proto schema definition file");

                            nfdu8filteritem_t filters[] = { {"Proto files", "proto"} };
                            nfdopendialogu8args_t args = {};
                            nfdu8char_t *out_path;
                            args.filterList = filters;
                            args.filterCount = ARRAY_LENGTH(filters);
                            ImGui::SameLine();
                            f32 button_size = ImGui::GetFrameHeight();
                            if (ImGui::Button(ICON_FILE_BROWSER, ImVec2(button_size, button_size))) {
                                set_native_window(window, &args.parentWindow);

                                if (NFD_OpenDialogU8_With(&out_path, &args) == NFD_OKAY) {
                                    strcpy(tab->path, out_path);
                                    NFD_FreePathU8(out_path);
                                }
                            }
                            ImGui::SetItemTooltip("open file browser");
                            fs::path proto_path{tab->path};
                            if (!fs::exists(proto_path) || !fs::is_regular_file(proto_path)) {
                                ImGui::PopItemWidth();
                                ImGui::EndGroup();
                                break;
                            }

                            source_tree.MapPath("", proto_path.parent_path().c_str());
                            tab->file_desc = importer.Import(proto_path.filename());
                            if (tab->file_desc == nullptr) {
                                LOG_ERR("file '%s' not found", proto_path.filename().c_str());
                                exit(EXIT_FAILURE);
                            }

                            if (tab->file_desc->message_type_count() == 1) {
                                tab->msg = factory.GetPrototype(tab->file_desc->message_type(0))->New();
                                tab->active_view = view::model;
                                ImGui::PopItemWidth();
                                ImGui::EndGroup();
                                break;
                            }

                            ImGui::PushClipRect(clip_rect_min, clip_rect_max, true);
                            ImGui::SeparatorText("messages");
                            ImGui::PopClipRect();

                            static ImGuiTextFilter filter;
                            if (just_activated) {
                                ImGui::SetKeyboardFocusHere();
                            }
                            filter.Draw("##filter");
                            ImGui::SetItemTooltip("filter messages");

                            if (ImGui::BeginListBox("##messages", ImVec2(0, group_size.y - ImGui::GetCursorPosY()))) {
                                for (i32 i = 0; i < tab->file_desc->message_type_count(); ++i) {
                                    const char *msg_name = tab->file_desc->message_type(i)->name().c_str();
                                    if (!filter.PassFilter(msg_name)) {
                                        continue;
                                    }

                                    if (ImGui::Selectable(msg_name)) {
                                        tab->msg = factory.GetPrototype(tab->file_desc->message_type(i))->New();
                                        tab->active_view = view::model;
                                        break;
                                    }
                                    ImGui::SetItemTooltip("click to select '%s' message", msg_name);
                                }
                                ImGui::EndListBox();
                            }
                            ImGui::PopItemWidth();
                            ImGui::EndGroup();
                        } break;
                        case view::model: {
                            ImGui::BeginChild("LeftPane", ImVec2(left_pane_width, avail.y), ImGuiChildFlags_Borders);

                            if (ImGui::Button("reset") || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_X)) {
                                tab->msg->Clear();
                                tree_collapse_all = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("import base64")) {
                                ImGui::OpenPopup("import base64");
                            }

                            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5, 0.5));

                            static u8 encoded[ENCODED_BUF_SIZE]{};
                            static char input[sizeof(encoded) * 2]{};
                            if (ImGui::BeginPopupModal("import base64", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                                ImGui::InputTextMultiline("base64", input, sizeof(input));

                                if (ImGui::Button("cancel")) {
                                    MEMORY_ZERO_ARRAY(input);
                                    ImGui::CloseCurrentPopup();
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("ok")) {
                                    try {
                                        std::vector<u8> raw = base64::decode(input);
                                        if (!tab->msg->ParseFromArray(raw.data(), raw.size())) {
                                            LOG_WRN("proto parsing error");
                                            tab->msg->Clear();
                                        } else {
                                            MEMORY_ZERO_ARRAY(input);
                                            ImGui::CloseCurrentPopup();
                                        }
                                    } catch (std::exception& e) {
                                        LOG_WRN("exception: %s", e.what());
                                        tab->msg->Clear();
                                    }
                                }
                                ImGui::EndPopup();
                            }

                            ImGui::SameLine();
                            if (ImGui::Button("import json")) {
                                ImGui::OpenPopup("import json");
                            }
                            if (ImGui::BeginPopupModal("import json", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                                ImGui::InputTextMultiline("json", input, sizeof(input));

                                if (ImGui::Button("cancel")) {
                                    MEMORY_ZERO_ARRAY(input);
                                    ImGui::CloseCurrentPopup();
                                }
                                ImGui::SameLine();
                                if (ImGui::Button("ok")) {
                                    absl::Status status = google::protobuf::util::JsonStringToMessage(input, tab->msg);
                                    if (status.ok()) {
                                        MEMORY_ZERO_ARRAY(input);
                                        ImGui::CloseCurrentPopup();
                                    } else {
                                        LOG_WRN("proto parsing error: %s", status.message().data());
                                        tab->msg->Clear();
                                    }
                                }
                                ImGui::EndPopup();
                            }

                            ImGui::Separator();

                            ImGui::BeginChild("Message");
                            draw_msg(tab->msg);
                            ImGui::EndChild(); // Message

                            ImGui::EndChild(); // LeftPane

                            ImGui::SameLine(0, 0);
                            ImGui::InvisibleButton("vsplitter", ImVec2(4.0f, avail.y));
                            if (ImGui::IsItemActive()) {
                                left_pane_width += io.MouseDelta.x;
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                            }

                            ImGui::SameLine(0, 0);
                            ImGui::BeginChild("RightPane", ImVec2(0, avail.y), ImGuiChildFlags_Borders);

                            ImGui::SeparatorText("output");

                            std::string json_str = "";
                            static proto::util::JsonPrintOptions opts{
                                .add_whitespace=true,
                                .always_print_primitive_fields = true,
                                .preserve_proto_field_names = true
                            };
                            ImGui::Checkbox("always print primitive fields", &opts.always_print_primitive_fields);
                            ImGui::Checkbox("preserve proto field names", &opts.preserve_proto_field_names);
                            absl::Status status = proto::util::MessageToJsonString(*tab->msg, &json_str, opts);

                            if (ImGui::Button("copy base64")) {
                                if (!tab->msg->SerializeToArray(encoded, sizeof(encoded))) {
                                    // TODO: error modals
                                    LOG_WRN("proto serialize error");
                                }
                                std::string b64 = base64::encode(encoded, tab->msg->ByteSizeLong());

                                ImGui::SetClipboardText(b64.c_str());
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("copy json")) {
                                ImGui::SetClipboardText(json_str.c_str());
                            }

                            ImGui::InputTextMultiline("##output", json_str.data(), json_str.size() + 1,
                                                      ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y),
                                                      ImGuiInputTextFlags_AutoSelectAll
                                                    | ImGuiInputTextFlags_ReadOnly // ImGuiInputTextFlags_CharsHexadecimal
                                                    | ImGuiInputTextFlags_WordWrap);

                            ImGui::EndChild(); // RightPane

                            tree_collapse_all = false;
                        } break;
                    }
                    ImGui::EndTabItem();
                }

                if (!open || tab->should_close) {
                    delete tab->msg;
                    for (usize i = t; i < tabs_opened - 1; ++i) {
                        tab_items[i] = tab_items[i + 1];
                    }
                    MEMORY_ZERO_STRUCT(&tab_items[tabs_opened - 1]);
                    tabs_opened -= 1;
                }

                ImGui::PopID();
            }
            keyboard_tab_switch_requested = false;
            ImGui::EndTabBar(); // Tabs
        }

        ImGui::End();
        ImGui::PopStyleVar(); // ImGuiStyleVar_WindowBorderSize

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    NFD_Quit();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
