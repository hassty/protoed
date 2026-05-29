#include "base/base.h"
#define LOG_IMPLEMENTATION
#include "base/log.h"

#include <cstdlib>
#include <cstdio>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <misc/cpp/imgui_stdlib.h>
#include <GLFW/glfw3.h>

#include <cppcodec/base64_rfc4648.hpp>

#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/reflection.h>

#include "msg.pb.h"

#define ENCODED_BUF_SIZE 4096

static void glfw_error_callback(int error, const char* description) {
    LOG_ERR("GLFW Error %d: %s", error, description);
}

static void draw_msg(google::protobuf::Message* msg);

static void draw_field_by_type(google::protobuf::Message *msg,
                               const google::protobuf::Reflection *reflection,
                               const google::protobuf::FieldDescriptor *field) {
    const char *label = field->name().data();

    switch (field->cpp_type()) {
        using namespace google::protobuf;
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
            if (ImGui::TreeNode(label)) {
                draw_msg(submsg);
                ImGui::TreePop();
            }
        } break;
    }
}

static void draw_repeated_field(google::protobuf::Message *msg,
                                const google::protobuf::Reflection *reflection,
                                const google::protobuf::FieldDescriptor *field,
                                u8 count) {
    for (u8 i = 0; i < count; ++i) {
        ImGui::PushID(i);
        switch (field->cpp_type()) {
            using namespace google::protobuf;
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
                if (ImGui::TreeNode(submsg->GetDescriptor()->name().data())) {
                    draw_msg(submsg);
                    ImGui::TreePop();
                }
            } break;
        }
        ImGui::PopID();
    }
}

static void draw_oneof_field(google::protobuf::Message *msg,
                             const google::protobuf::Reflection *reflection,
                             const google::protobuf::OneofDescriptor *oneof,
                             const google::protobuf::FieldDescriptor *active = nullptr) {
    const char *preview = active ? active->name().data() : "";
    if (ImGui::BeginCombo(oneof->name().data(), preview)) {
        for (u8 i = 0; i < oneof->field_count(); ++i) {
            const auto *oneof_option = oneof->field(i);
            if (ImGui::Selectable(oneof_option->name().data(), oneof_option == active)) {
                switch (oneof_option->cpp_type()) {
                    using namespace google::protobuf;
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
    if (ImGui::Button("x")) {
        reflection->ClearOneof(msg, oneof);
    }
    ImGui::PopID();
}

static void draw_field(google::protobuf::Message *msg,
                       const google::protobuf::Reflection *reflection,
                       const google::protobuf::FieldDescriptor *field) {
    if (field->is_repeated()) {
        ImGui::PushID(field->name().data());
        u8 count = reflection->FieldSize(*msg, field);
        if (ImGui::Button("-") && count > 0) {
            reflection->RemoveLast(msg, field);
            count -= 1;
        }
        ImGui::SameLine();
        ImGui::Text("%s", field->name().data());
        ImGui::SameLine();
        if (ImGui::Button("+")) {
            switch (field->cpp_type()) {
                using namespace google::protobuf;
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
                draw_field_by_type(msg, reflection, active);
            }
        }
    } else {
        draw_field_by_type(msg, reflection, field);
    }
}

static void draw_msg(google::protobuf::Message* msg) {
    const auto *descriptor = msg->GetDescriptor();
    const auto *reflection = msg->GetReflection();

    ImGui::PushID(msg);

    for (u8 i = 0; i < descriptor->field_count(); ++i) {
        const auto *field = descriptor->field(i);
        draw_field(msg, reflection, field);
    }

    ImGui::PopID();
}

int main() {
    using base64 = cppcodec::base64_rfc4648;
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    log_init(LOG_DBG);

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        LOG_ERR("glfwInit failed");
        exit(EXIT_FAILURE);
    }

    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    f32 main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    LOG_DBG("main_scale: %.2f", main_scale);
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

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;
    style.FontSizeBase = 16;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

    msg_t msg{};

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

        ImGui::Text("%s", msg.GetDescriptor()->full_name().data());
        ImGui::SameLine();
        if (ImGui::Button("clear")) {
            msg.Clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("import")) {
            ImGui::OpenPopup("import");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5, 0.5));

        static u8 encoded[ENCODED_BUF_SIZE]{};
        static char input[sizeof(encoded) * 2]{};
        if (ImGui::BeginPopupModal("import", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputTextMultiline("b64", input, sizeof(input));

            if (ImGui::Button("cancel")) {
                MEMORY_ZERO_ARRAY(input);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("ok")) {
                try {
                    std::vector<u8> raw = base64::decode(input);
                    if (!msg.ParseFromArray(raw.data(), raw.size())) {
                        LOG_WRN("proto parsing error");
                        msg.Clear();
                    } else {
                        MEMORY_ZERO_ARRAY(input);
                        ImGui::CloseCurrentPopup();
                    }
                } catch (std::exception& e) {
                    LOG_WRN("exception: %s", e.what());
                    msg.Clear();
                }
            }
            ImGui::EndPopup();
        }
        draw_msg(&msg);

        if (!msg.SerializeToArray(encoded, sizeof(encoded))) {
            // TODO: error modals
            LOG_WRN("proto serialize error");
        }
        std::string b64 = base64::encode(encoded, msg.ByteSizeLong());
        ImGui::InputTextMultiline("##encoded", b64.data(), b64.size() + 1,
                                  ImVec2(0, 0),
                                  ImGuiInputTextFlags_AutoSelectAll
                                | ImGuiInputTextFlags_ReadOnly // ImGuiInputTextFlags_CharsHexadecimal
                                | ImGuiInputTextFlags_WordWrap);

        ImGui::SameLine();
        if (ImGui::Button("copy")) {
            ImGui::SetClipboardText(b64.c_str());
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

    LOG_DBG("deinit");

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
