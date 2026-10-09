#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <reframework/API.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
using API = reframework::API;

// RE3's managed game types use the offline namespace.
constexpr const char* kPrefix = "offline";
constexpr unsigned int kMaximumKeyFrames = 1024;
constexpr unsigned int kMaximumValueTypeSize = 4096;
constexpr unsigned int kInitializationAttempts = 120;
constexpr DWORD kInitializationDelayMs = 500;

#define LOG_INFO(...) API::get()->log_info("[RE3MouseFix] " __VA_ARGS__)
#define LOG_WARN(...) API::get()->log_warn("[RE3MouseFix] " __VA_ARGS__)
#define LOG_ERROR(...) API::get()->log_error("[RE3MouseFix] " __VA_ARGS__)

enum class InitResult {
    success,
    retry,
    fatal,
};

enum class InputMode : std::int32_t {
    pad = 0,
    mouse_keyboard = 1,
};

struct Config {
    bool linearize_input_curve{true};
    bool remove_camera_damping{true};
    bool remove_pitch_scaling{true};
    bool correct_mouse_magnitude{true};
};

struct CurveMetadata {
    API::Method* get_keys_count{};
    API::Method* get_keys{};
    API::Method* set_keys{};
    API::TypeDefinition* key_frame_type{};
    API::Field* in_normal{};
    API::Field* out_normal{};
    API::Field* value{};
};

Config g_config{};
CurveMetadata g_curve{};
API::ManagedObject* g_input_system{};
API::Field* g_input_mode_field{};
bool g_camera_patched{};
bool g_magnitude_hooked{};

fs::path executable_directory() {
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));

    if (length == 0 || length >= path.size()) {
        return fs::current_path();
    }

    return fs::path{std::wstring{path.data(), length}}.parent_path();
}

bool read_bool(const fs::path& ini, const wchar_t* key, bool default_value) {
    return GetPrivateProfileIntW(
        L"MouseFix",
        key,
        default_value ? 1 : 0,
        ini.c_str()
    ) != 0;
}

Config load_config() {
    const fs::path ini = executable_directory() / L"reframework" / L"data" / L"RE3MouseFix.ini";
    Config config{};
    config.linearize_input_curve = read_bool(ini, L"LinearizeInputCurve", true);
    config.remove_camera_damping = read_bool(ini, L"RemoveCameraDamping", true);
    config.remove_pitch_scaling = read_bool(ini, L"RemovePitchScaling", true);
    config.correct_mouse_magnitude = read_bool(ini, L"CorrectMouseMagnitude", true);
    return config;
}

template <typename T>
bool require_pointer(T* pointer, const char* description) {
    if (pointer != nullptr) {
        return true;
    }

    LOG_ERROR("Required engine item is missing: %s", description);
    return false;
}

API::ManagedObject* read_managed_field(API::ManagedObject* object, const char* field_name) {
    if (object == nullptr) {
        return nullptr;
    }

    auto* slot = object->get_field<API::ManagedObject*>(field_name);
    if (slot == nullptr) {
        LOG_ERROR("Field is missing on runtime object: %s", field_name);
        return nullptr;
    }

    return *slot;
}

bool resolve_curve_metadata(bool need_values) {
    auto* const tdb = API::get()->tdb();
    if (!require_pointer(tdb, "TypeDB")) {
        return false;
    }

    auto* const animation_curve = tdb->find_type("via.AnimationCurve");
    g_curve.key_frame_type = tdb->find_type("via.KeyFrame");

    if (!require_pointer(animation_curve, "via.AnimationCurve") ||
        !require_pointer(g_curve.key_frame_type, "via.KeyFrame")) {
        return false;
    }

    g_curve.get_keys_count = animation_curve->find_method("getKeysCount");
    g_curve.get_keys = animation_curve->find_method("getKeys");
    g_curve.set_keys = animation_curve->find_method("setKeys");
    g_curve.in_normal = g_curve.key_frame_type->find_field("inNormal");
    g_curve.out_normal = g_curve.key_frame_type->find_field("outNormal");

    if (!require_pointer(g_curve.get_keys_count, "via.AnimationCurve.getKeysCount") ||
        !require_pointer(g_curve.get_keys, "via.AnimationCurve.getKeys") ||
        !require_pointer(g_curve.set_keys, "via.AnimationCurve.setKeys") ||
        !require_pointer(g_curve.in_normal, "via.KeyFrame.inNormal") ||
        !require_pointer(g_curve.out_normal, "via.KeyFrame.outNormal")) {
        return false;
    }

    if (need_values) {
        g_curve.value = g_curve.key_frame_type->find_field("value");
        if (!require_pointer(g_curve.value, "via.KeyFrame.value")) {
            return false;
        }
    }

    return true;
}

bool mutate_curve(API::ManagedObject* curve, bool flatten_values, const char* description) {
    if (!require_pointer(curve, description)) {
        return false;
    }

    auto* const vm = API::get()->get_vm_context();
    if (!require_pointer(vm, "VM context")) {
        return false;
    }

    const unsigned int value_type_size = g_curve.key_frame_type->get_valuetype_size();
    if (value_type_size == 0 || value_type_size > kMaximumValueTypeSize) {
        LOG_ERROR("Unexpected via.KeyFrame size: %u", value_type_size);
        return false;
    }

    const unsigned int key_count = g_curve.get_keys_count->call<unsigned int>(vm, curve);
    if (key_count == 0 || key_count > kMaximumKeyFrames) {
        LOG_ERROR("Unexpected key count for %s: %u", description, key_count);
        return false;
    }

    std::vector<std::uint8_t> key_frame(value_type_size);
    bool all_keys_changed = true;

    for (unsigned int index = 0; index < key_count; ++index) {
        auto* const result = g_curve.get_keys->call<API::ManagedObject*>(
            key_frame.data(),
            vm,
            curve,
            index
        );

        if (result == nullptr) {
            LOG_WARN("Could not retrieve key %u from %s", index, description);
            all_keys_changed = false;
            continue;
        }

        g_curve.in_normal->get_data<std::uint32_t>(key_frame.data(), true) = 0U;
        g_curve.out_normal->get_data<std::uint32_t>(key_frame.data(), true) = 0U;

        if (flatten_values) {
            g_curve.value->get_data<float>(key_frame.data(), true) = 1.0F;
        }

        g_curve.set_keys->call(vm, curve, index, key_frame.data());
    }

    if (all_keys_changed) {
        LOG_INFO("Patched %s (%u keys)", description, key_count);
    }

    return all_keys_changed;
}

InitResult apply_camera_fixes() {
    auto* const tdb = API::get()->tdb();
    if (!require_pointer(tdb, "TypeDB")) {
        return InitResult::fatal;
    }

    const bool needs_curves = g_config.linearize_input_curve || g_config.remove_pitch_scaling;
    if (needs_curves && !resolve_curve_metadata(g_config.remove_pitch_scaling)) {
        return InitResult::fatal;
    }

    const std::string camera_system_type = std::string{kPrefix} + ".camera.CameraSystem";
    auto* const get_camera_controller = tdb->find_method(camera_system_type, "getCameraController");
    if (!require_pointer(get_camera_controller, "CameraSystem.getCameraController")) {
        return InitResult::fatal;
    }

    auto* const camera_system = API::get()->get_managed_singleton(camera_system_type);
    if (camera_system == nullptr) {
        return InitResult::retry;
    }

    auto* const vm = API::get()->get_vm_context();
    if (vm == nullptr) {
        return InitResult::retry;
    }

    // RE3 keeps normal and sight aiming cameras in slots 0 and 1.
    std::array<API::ManagedObject*, 2> controllers{};
    std::array<API::ManagedObject*, 2> settings{};
    for (std::size_t index = 0; index < controllers.size(); ++index) {
        controllers[index] = get_camera_controller->call<API::ManagedObject*>(
            vm, camera_system, static_cast<int>(index)
        );
        if (controllers[index] == nullptr) {
            return InitResult::retry;
        }

        settings[index] = read_managed_field(controllers[index], "TwirlerCameraSettings");
        if (settings[index] == nullptr) {
            return InitResult::retry;
        }
    }

    API::Field* damping_time{};
    if (g_config.remove_camera_damping) {
        // RE3 exposes the generic damping value as its closed Single type in TypeDB.
        const std::string damping_type_name = std::string{kPrefix} + ".DampingStruct`1<System.Single>";
        auto* const damping_type = tdb->find_type(damping_type_name);
        if (!require_pointer(damping_type, "DampingStruct`1<System.Single>")) {
            return InitResult::fatal;
        }

        damping_time = damping_type->find_field("DampingTime");
        if (!require_pointer(damping_time, "DampingStruct.DampingTime")) {
            return InitResult::fatal;
        }
    }

    for (std::size_t index = 0; index < controllers.size(); ++index) {
        LOG_INFO("Patching %s camera (slot %zu)", index == 0 ? "normal" : "sight", index);

        if (g_config.linearize_input_curve) {
            auto* const input_curve = read_managed_field(settings[index], "InputCurve");
            if (input_curve == nullptr || !mutate_curve(input_curve, false, "camera input response curve")) {
                return InitResult::fatal;
            }
        }

        if (g_config.remove_pitch_scaling) {
            auto* const normal_speed = read_managed_field(settings[index], "NormalSpeedCurve");
            auto* const hold_speed = read_managed_field(settings[index], "HoldSpeedCurve");

            if (normal_speed == nullptr || hold_speed == nullptr ||
                !mutate_curve(normal_speed, true, "normal horizontal speed curve") ||
                !mutate_curve(hold_speed, true, "aiming horizontal speed curve")) {
                return InitResult::fatal;
            }
        }

        if (damping_time != nullptr) {
            auto* const yaw = read_managed_field(controllers[index], "<TwirlSpeedYaw>k__BackingField");
            auto* const pitch = read_managed_field(controllers[index], "<TwirlSpeedPitch>k__BackingField");

            if (yaw == nullptr || pitch == nullptr) {
                return InitResult::fatal;
            }

            const float old_yaw = damping_time->get_data<float>(yaw);
            const float old_pitch = damping_time->get_data<float>(pitch);
            damping_time->get_data<float>(yaw) = 0.0F;
            damping_time->get_data<float>(pitch) = 0.0F;
            LOG_INFO("Removed camera %zu damping (yaw %.6f, pitch %.6f -> 0)", index, old_yaw, old_pitch);
        }
    }

    return InitResult::success;
}

int pre_control_magnitude(
    int,
    void**,
    REFrameworkTypeDefinitionHandle*,
    unsigned long long
) {
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post_control_magnitude(
    void** return_value,
    REFrameworkTypeDefinitionHandle,
    unsigned long long
) {
    if (return_value == nullptr || g_input_system == nullptr || g_input_mode_field == nullptr) {
        return;
    }

    const auto mode = g_input_mode_field->get_data<InputMode>(g_input_system);
    auto* const magnitude = reinterpret_cast<float*>(return_value);

    if (mode == InputMode::mouse_keyboard && *magnitude > 0.0F) {
        *magnitude = 1.0F;
    }
}

InitResult install_magnitude_hook() {
    auto* const tdb = API::get()->tdb();
    if (!require_pointer(tdb, "TypeDB")) {
        return InitResult::fatal;
    }

    const std::string input_system_type = std::string{kPrefix} + ".InputSystem";
    const std::string controller_type = std::string{kPrefix} + ".camera.TwirlerCameraControllerRoot";

    g_input_system = API::get()->get_managed_singleton(input_system_type);
    if (g_input_system == nullptr) {
        return InitResult::retry;
    }

    g_input_mode_field = tdb->find_field(input_system_type, "<InputMode>k__BackingField");
    auto* const controller = tdb->find_type(controller_type);

    if (!require_pointer(g_input_mode_field, "InputSystem.InputMode") ||
        !require_pointer(controller, "TwirlerCameraControllerRoot")) {
        return InitResult::fatal;
    }

    auto* const get_control_magnitude = controller->find_method("getControlMagnitude");
    if (!require_pointer(get_control_magnitude, "TwirlerCameraControllerRoot.getControlMagnitude")) {
        return InitResult::fatal;
    }

    get_control_magnitude->add_hook(pre_control_magnitude, post_control_magnitude, false);
    LOG_INFO("Installed mouse control-magnitude correction hook");
    return InitResult::success;
}

InitResult initialize_once() {
    const bool needs_camera = g_config.linearize_input_curve ||
        g_config.remove_camera_damping ||
        g_config.remove_pitch_scaling;

    if (needs_camera && !g_camera_patched) {
        const InitResult camera_result = apply_camera_fixes();
        if (camera_result != InitResult::success) {
            return camera_result;
        }

        g_camera_patched = true;
    }

    if (g_config.correct_mouse_magnitude && !g_magnitude_hooked) {
        const InitResult hook_result = install_magnitude_hook();
        if (hook_result != InitResult::success) {
            return hook_result;
        }

        g_magnitude_hooked = true;
    }

    return InitResult::success;
}

} // namespace

extern "C" __declspec(dllexport) void reframework_plugin_required_version(
    REFrameworkPluginVersion* version
) {
    if (version == nullptr) {
        return;
    }

    version->major = REFRAMEWORK_PLUGIN_VERSION_MAJOR;
    version->minor = REFRAMEWORK_PLUGIN_VERSION_MINOR;
    version->patch = REFRAMEWORK_PLUGIN_VERSION_PATCH;
    version->game_name = "RE3";
}

extern "C" __declspec(dllexport) bool reframework_plugin_initialize(
    const REFrameworkPluginInitializeParam* parameter
) {
    bool api_initialized = false;

    try {
        API::initialize(parameter);
        api_initialized = true;
        g_config = load_config();

        LOG_INFO("Version 1.0.0 initializing for RE3 TDB70");
        LOG_INFO(
            "Settings: curve=%d damping=%d pitch-scaling=%d magnitude=%d",
            g_config.linearize_input_curve,
            g_config.remove_camera_damping,
            g_config.remove_pitch_scaling,
            g_config.correct_mouse_magnitude
        );

        for (unsigned int attempt = 0; attempt < kInitializationAttempts; ++attempt) {
            const InitResult result = initialize_once();

            if (result == InitResult::success) {
                LOG_INFO("Initialization complete");
                return true;
            }

            if (result == InitResult::fatal) {
                LOG_ERROR("Initialization stopped because this game/TypeDB layout is incompatible");
                return false;
            }

            if (attempt == 0 || (attempt + 1) % 10 == 0) {
                LOG_INFO("Waiting for RE3 camera/input singletons (%u/%u)", attempt + 1, kInitializationAttempts);
            }

            Sleep(kInitializationDelayMs);
        }

        LOG_ERROR("Initialization timed out; the game was left unmodified");
        return false;
    } catch (const std::exception& error) {
        if (api_initialized) {
            LOG_ERROR("Initialization exception: %s", error.what());
        }
        return false;
    } catch (...) {
        return false;
    }
}
