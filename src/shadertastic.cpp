#define do_log(level, format, ...) \
    blog(level, "[shadertastic] " format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)

#ifdef DEV_MODE
    #define debug(format, ...) info(format, ##__VA_ARGS__)
#else
    #define debug(format, ...)
#endif

#include <iostream>
#include <string>
#include <vector>
#include <map>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

#include <obs-module.h>
#include <util/platform.h>
#include "twitch_util.h"
#include "version.h"
#include "list_files.hpp"

#define LOG_OFFSET_DB 6.0f
#define LOG_RANGE_DB 96.0f

#define try_gs_effect_set_texture(param, val) if (param) { gs_effect_set_texture(param, val); }
#define try_gs_effect_set_texture_srgb(param, val) if (param) { gs_effect_set_texture_srgb(param, val); }

#include "shader/shader.hpp"
#include "parameters/parameter.hpp"
#include "parameters/parameter_double.hpp"
#include "parameters/parameter_int.hpp"
#include "parameters/parameter_list_int.hpp"
#include "parameters/parameter_unknown.hpp"
#include "parameters/parameter_factory.hpp"
#include "effect.hpp"
#include "shadertastic.hpp"
//----------------------------------------------------------------------------------------------------------------------

void load_effect(shadertastic_effect_t &effect, std::string parent_dir, std::string effect_name) {
    debug(">>>>>>>>>>>>>>> load_effect %s", effect_name.c_str());
    char *metadata_path = obs_module_file((std::string("effects/") + parent_dir + "/" + effect_name + "/meta.json").c_str());
    char *shader_path = obs_module_file((std::string("effects/") + parent_dir + "/" +effect_name + "/main.hlsl").c_str());

    effect.name = effect_name;
    effect.main_shader.release();

    if (shader_path != NULL) {
        effect.main_shader.load(shader_path);
        if (effect.main_shader.effect == NULL) {
            char *fallback_shader_path = obs_module_file((std::string("effects/") + parent_dir + "/fallback_effect.hlsl").c_str());
            debug("FALLBACK %s", fallback_shader_path);
            effect.main_shader.load(fallback_shader_path);
            bfree(fallback_shader_path);
            effect.is_fallback = true;
        }

        effect.load_metadata(metadata_path);
    }
    if (shader_path != NULL) {
        bfree(shader_path);
    }
    if (metadata_path != NULL) {
        bfree(metadata_path);
    }
}
//----------------------------------------------------------------------------------------------------------------------

void reload_effect(std::string parent_dir, shadertastic_effect_t *selected_effect) {
    if (selected_effect != NULL) {
        std::string effect_name = selected_effect->name;
        load_effect(*selected_effect, parent_dir, effect_name);
    }
}
//----------------------------------------------------------------------------------------------------------------------

#include "shader_filter.hpp"
#include "shader_transition.hpp"
//----------------------------------------------------------------------------------------------------------------------

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("xurei")
OBS_MODULE_USE_DEFAULT_LOCALE(
    #ifdef DEV_MODE
      "shadertastic-dev"
    #else
      "shadertastic"
    #endif
, "en-US")

bool obs_module_load(void) {
    info("loaded version %s", PROJECT_VERSION);

    struct obs_source_info shadertastic_transition_info = {};
    shadertastic_transition_info.id =
        #ifdef DEV_MODE
          "shadertastic_transition_dev";
        #else
          "shadertastic_transition";
        #endif
    shadertastic_transition_info.type = OBS_SOURCE_TYPE_TRANSITION;
    shadertastic_transition_info.get_name = shadertastic_transition_get_name;
    shadertastic_transition_info.create = shadertastic_transition_create;
    shadertastic_transition_info.destroy = shadertastic_transition_destroy;
    shadertastic_transition_info.get_properties = shadertastic_transition_properties;
    shadertastic_transition_info.update = shadertastic_transition_update;
    shadertastic_transition_info.video_render = shadertastic_transition_video_render;
    shadertastic_transition_info.load = shadertastic_transition_update;
    shadertastic_transition_info.audio_render = shadertastic_transition_audio_render;
    shadertastic_transition_info.transition_start = shadertastic_transition_start;
    shadertastic_transition_info.transition_stop = shadertastic_transition_stop;
    shadertastic_transition_info.get_defaults2 = shadertastic_transition_defaults;
    //shadertastic_transition_info.video_tick = shadertastic_transition_tick;
    shadertastic_transition_info.video_get_color_space = shadertastic_transition_get_color_space;
    obs_register_source(&shadertastic_transition_info);

//    #ifdef DEV_MODE
//        struct obs_source_info shadertastic_transition_dev_info = {};
//        shadertastic_transition_dev_info.id = "shadertastic_filter_dev";
//        shadertastic_transition_dev_info.type = OBS_SOURCE_TYPE_FILTER;
//        shadertastic_transition_dev_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB;
//        shadertastic_transition_dev_info.get_name = shadertastic_transition_filter_get_name;
//        shadertastic_transition_dev_info.create = shadertastic_transition_filter_create;
//        shadertastic_transition_dev_info.destroy = shadertastic_transition_filter_destroy;
//        shadertastic_transition_dev_info.get_properties = shadertastic_transition_properties;
//        shadertastic_transition_dev_info.update = shadertastic_transition_update;
//        shadertastic_transition_dev_info.video_render = shadertastic_transition_filter_video_render;
//        shadertastic_transition_dev_info.load = shadertastic_transition_update;
//        shadertastic_transition_dev_info.get_defaults2 = shadertastic_transition_defaults;
//        shadertastic_transition_dev_info.video_get_color_space = shadertastic_transition_get_color_space;
//        obs_register_source(&shadertastic_transition_dev_info);
//    #endif

    struct obs_source_info shadertastic_filter_info = {};
    shadertastic_filter_info.id = "shadertastic_filter";
    shadertastic_filter_info.type = OBS_SOURCE_TYPE_FILTER;
    shadertastic_filter_info.output_flags = OBS_SOURCE_VIDEO;
    shadertastic_filter_info.get_name = shadertastic_filter_get_name;
    shadertastic_filter_info.create = shadertastic_filter_create;
    shadertastic_filter_info.destroy = shadertastic_filter_destroy;
    shadertastic_filter_info.get_properties = shadertastic_filter_properties;
    shadertastic_filter_info.video_tick = shadertastic_filter_tick;
    shadertastic_filter_info.update = shadertastic_filter_update;
	shadertastic_filter_info.get_width = shadertastic_filter_getwidth,
	shadertastic_filter_info.get_height = shadertastic_filter_getheight,
    shadertastic_filter_info.video_render = shadertastic_filter_video_render;
    shadertastic_filter_info.load = shadertastic_filter_update;
    shadertastic_filter_info.get_defaults2 = shadertastic_filter_defaults;
    shadertastic_filter_info.video_get_color_space = shadertastic_filter_get_color_space;
    obs_register_source(&shadertastic_filter_info);

    return true;
}
//----------------------------------------------------------------------------------------------------------------------
