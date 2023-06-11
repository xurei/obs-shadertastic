#define do_log(level, format, ...) \
    blog(level, "[shadertastic] " format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)

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
#include "transition_effect.hpp"
#include "shadertastic.hpp"
//----------------------------------------------------------------------------------------------------------------------

transition_effect_t load_effect(std::string effect_name, transition_effect_t *previous_loaded_effect) {
    char *metadata_path = obs_module_file((std::string("effects/") + effect_name + "/meta.json").c_str());
    char *shader_path = obs_module_file((std::string("effects/") + effect_name + "/main.hlsl").c_str());
    obs_data_t *effect_metadata = obs_data_create_from_json_file(metadata_path);
    debug(">>>>>>>>>>>>>>> load_effect %s", effect_name.c_str());

    if (effect_metadata != NULL) {
        const char *effect_label = obs_data_get_string(effect_metadata, "label");

        obs_data_array_t *effect_parameters = obs_data_get_array(effect_metadata, "parameters");

        transition_effect_t effect;
        effect.name = effect_name;
        effect.metadata = effect_metadata;
        effect.parameters = effect_parameters;

        obs_data_set_default_int(effect_metadata, "steps", 1);
        effect.nb_steps = (int)obs_data_get_int(effect_metadata, "steps");

        bool is_fallback = false;

        effect.main_shader.load(shader_path);
        if (effect.main_shader.effect == NULL) {
            char *fallback_shader_path = obs_module_file("effects/fallback_effect.hlsl");
            debug("FALLBACK %s", fallback_shader_path);
            effect.main_shader.load(fallback_shader_path);
            bfree(fallback_shader_path);
            is_fallback = true;
        }

        {
            obs_data_array_t *parameters = obs_data_get_array(effect_metadata, "parameters");
            //debug(">>>>>>>>1");

            if (parameters == NULL) {
                info("No parameter specified for effect %s", effect_name.c_str());
            }
            else if (!is_fallback) {
                //debug(">>>>>>>>2");
                for (size_t i=0; i < obs_data_array_count(parameters); i++) {
                    //debug(">>>>>>>>3");
                    obs_data_t *param_metadata = obs_data_array_item(parameters, i);
                    const char *param_name = obs_data_get_string(param_metadata, "name");
                    const char *data_type = obs_data_get_string(param_metadata, "type");
                    //debug(">>>>>>>>4 %s", param_name);
                    gs_eparam_t *shader_param = gs_effect_get_param_by_name(effect.main_shader.effect, param_name);
                    //debug(">>>>>>>>5");
                    effect_parameter *effect_param = parameter_factory.create(effect_name, shader_param, param_metadata);
                    //debug(">>>>>>>>6");

                    if (effect_param != NULL) {
                        std::string param_name_str = std::string(param_name);
                        if (previous_loaded_effect != NULL &&
                            previous_loaded_effect->effect_params.find(param_name_str) != previous_loaded_effect->effect_params.end() &&
                            previous_loaded_effect->effect_params[param_name_str]->get_data_size() == effect_param->get_data_size()
                           ) {
                            debug("Recycling data for %s", param_name_str.c_str());
                            effect_param->data = previous_loaded_effect->effect_params[param_name_str]->data;
                        }
                        else {
                            effect_param->data = malloc(effect_param->get_data_size());
                        }

                        effect_param->set_defaults();
                        effect.effect_params.insert(std::pair<std::string, effect_parameter *>(param_name_str, effect_param));
                    }
                }
            }
        }

        info("Loaded effect %s from %s", effect_name.c_str(), metadata_path);
        bfree(shader_path);
        bfree(metadata_path);

        return effect;
    }

    // Something went wrong -> return the empty transition, should not be added in the list
    debug("Missing metadata in %s", effect_name.c_str());
    bfree(shader_path);
    bfree(metadata_path);
    struct transition_effect_t effect;
    effect.name = effect_name;
    effect.metadata = NULL;
    effect.parameters = NULL;
    return effect;
}
//----------------------------------------------------------------------------------------------------------------------

#ifdef DEV_MODE
void reload_shader(shadertastic_transition *s) {
    if (s->selected_effect != NULL) {
        debug("DEV_RELOAD %p", s->selected_effect);
        std::string effect_name = s->selected_effect->name;
        debug("DEV_RELOAD2");
        transition_effect_t effect = load_effect(effect_name, s->selected_effect);
        debug("DEV_RELOAD3");
        s->selected_effect->release();
        debug("DEV_RELOAD4");
        if (effect.metadata != NULL && effect.parameters != NULL && effect.main_shader.effect != NULL) {
            (*s->effects)[effect.name] = effect;
        }
    }
}
#endif
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
    shadertastic_filter_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB;
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
