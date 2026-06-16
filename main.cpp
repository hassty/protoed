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
        if (ImGui::Button("-") && count > 0) {
            reflection->RemoveLast(msg, field);
            count -= 1;
        }
        if (btn_minus_disabled) {
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        ImGui::Text("%s", field->name().data());
        ImGui::SameLine();
        if (ImGui::Button("+")) {
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
static view current_view = view::import;

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

int main(i32 argc, const char *argv[]) {
    using base64 = cppcodec::base64_rfc4648;
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    log_init(LOG_DBG);

    char path[256] = {};
    proto::Message* msg = nullptr;
    const proto::FileDescriptor *file_desc = nullptr;
    proto::DynamicMessageFactory factory;
    protoc::DiskSourceTree source_tree;

    protoc::Importer importer(&source_tree, &error_collector);

    if (argc == 3) {
        fs::path proto_path{argv[1]};
        source_tree.MapPath("", proto_path.parent_path().c_str());
        if (!fs::exists(proto_path) || !fs::is_regular_file(proto_path)) {
            std::cerr << "file '" << argv[1] << "' not found" << std::endl;
            exit(EXIT_FAILURE);
        }
        file_desc = importer.Import(proto_path.filename());
        const proto::Descriptor *desc = file_desc->FindMessageTypeByName(argv[2]);
        if (desc == nullptr) {
            std::cerr << "message '" << argv[2] << "' not found, possible options:\n";
            for (i32 i = 0; i < file_desc->message_type_count(); ++i) {
                std::cerr << "    " << file_desc->message_type(i)->name() << '\n';
            }
            std::flush(std::cerr);
            exit(EXIT_FAILURE);
        }
        msg = factory.GetPrototype(desc)->New();
        current_view = view::model;
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
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = NULL;

    io.Fonts->ClearFonts();
    io.Fonts->AddFontFromMemoryCompressedTTF(custom_font_compressed_data, custom_font_compressed_size);

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;
    style.FontSizeBase = 20;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

    f32 left_pane_width = 0.0f;

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

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Equal)) {
            style.FontScaleDpi += 0.1f;
        } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Minus) && style.FontScaleDpi > 0.5f) {
            style.FontScaleDpi -= 0.1f;
        } else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_0)) {
            style.FontScaleDpi = 1.0f;
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (left_pane_width <= 0.0f) {
            left_pane_width = avail.x / 2.0f;
        }

        switch (current_view) {
            case view::import: {
                ImGui::InputText("##proto_path", path, sizeof(path));
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
                        strcpy(path, out_path);
                        NFD_FreePathU8(out_path);
                    }
                }
                fs::path proto_path{path};
                if (!fs::exists(proto_path) || !fs::is_regular_file(proto_path)) {
                    break;
                }

                source_tree.MapPath("", proto_path.parent_path().c_str());
                file_desc = importer.Import(proto_path.filename());
                if (file_desc == nullptr) {
                    LOG_ERR("file '%s' not found", proto_path.filename().c_str());
                    exit(EXIT_FAILURE);
                }

                if (ImGui::BeginListBox("message")) {
                    for (i32 i = 0; i < file_desc->message_type_count(); ++i) {
                        if (ImGui::Selectable(file_desc->message_type(i)->name().c_str())) {
                            msg = factory.GetPrototype(file_desc->message_type(i))->New();
                            current_view = view::model;
                            break;
                        }
                    }
                    ImGui::EndListBox();
                }
            } break;
            case view::model: {
                ImGui::BeginChild("LeftPane", ImVec2(left_pane_width, avail.y), true);
                ImGui::SeparatorText(msg->GetDescriptor()->full_name().data());

                if (ImGui::Button("reset") || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_X)) {
                    msg->Clear();
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
                            if (!msg->ParseFromArray(raw.data(), raw.size())) {
                                LOG_WRN("proto parsing error");
                                msg->Clear();
                            } else {
                                MEMORY_ZERO_ARRAY(input);
                                ImGui::CloseCurrentPopup();
                            }
                        } catch (std::exception& e) {
                            LOG_WRN("exception: %s", e.what());
                            msg->Clear();
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
                        absl::Status status = google::protobuf::util::JsonStringToMessage(input, msg);
                        if (status.ok()) {
                            MEMORY_ZERO_ARRAY(input);
                            ImGui::CloseCurrentPopup();
                        } else {
                            LOG_WRN("proto parsing error: %s", status.message().data());
                            msg->Clear();
                        }
                    }
                    ImGui::EndPopup();
                }

                ImGui::Separator();

                draw_msg(msg);
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
                ImGui::BeginChild("RightPane", ImVec2(0, avail.y), true);

                ImGui::SeparatorText("output");

                std::string json_str = "";
                static proto::util::JsonPrintOptions opts{
                    .add_whitespace=true,
                    .always_print_primitive_fields = true,
                    .preserve_proto_field_names = true
                };
                ImGui::Checkbox("always print primitive fields", &opts.always_print_primitive_fields);
                ImGui::Checkbox("preserve proto field names", &opts.preserve_proto_field_names);
                absl::Status status = proto::util::MessageToJsonString(*msg, &json_str, opts);

                if (ImGui::Button("copy base64")) {
                    if (!msg->SerializeToArray(encoded, sizeof(encoded))) {
                        // TODO: error modals
                        LOG_WRN("proto serialize error");
                    }
                    std::string b64 = base64::encode(encoded, msg->ByteSizeLong());

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
