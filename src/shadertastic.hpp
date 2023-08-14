/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

typedef std::map<std::string, shadertastic_effect_t> shadertastic_effects_map_t;

struct shadertastic_transition {
    obs_source_t *source;
    bool transition_started;
    bool transitioning;
    float transition_point;
    gs_texrender_t *transition_texrender[2];
    int transition_texrender_buffer = 0;
    gs_texture_t *transparent_texture;
    float rand_seed;

    bool auto_reload = false;

    obs_transition_audio_mix_callback_t mix_a;
    obs_transition_audio_mix_callback_t mix_b;
    float transition_a_mul;
    float transition_b_mul;

    shadertastic_effects_map_t *effects;
    shadertastic_effect_t *selected_effect = NULL;

    void release() {
        for (auto& [key, effect] : *this->effects) {
            effect.release();
        }
        if (this->effects != NULL) {
            delete this->effects;
        }
    }
};
//----------------------------------------------------------------------------------------------------------------------

struct shadertastic_filter {
    obs_source_t *source;
    gs_texrender_t *interm_texrender[2];
    int interm_texrender_buffer = 0;
    gs_texture_t *transparent_texture;
    float rand_seed;
    int width, height;
    bool should_reload = false;
    shadertastic_effects_map_t *effects;
    shadertastic_effect_t *selected_effect = NULL;

    // Filter previous state (enabled/disabled)
    bool was_enabled = false;

    // Fields for "input_time": true in metadata
    double speed = 1.0;
    bool reset_time_on_show = false;
    double time = 0.0;

    void release() {
        for (auto& [key, effect] : *this->effects) {
            effect.release();
        }
        delete this->effects;
    }
};
//----------------------------------------------------------------------------------------------------------------------

const char *shadertastic_transition_get_name(void *type_data)
{
    UNUSED_PARAMETER(type_data);
    #ifdef DEV_MODE
        return "ShadertasticDev";
    #else
        return obs_module_text("TransitionName");
    #endif
}
const char *shadertastic_transition_filter_get_name(void *type_data) {
    UNUSED_PARAMETER(type_data);
    return "ShadertasticDev";
}
const char *shadertastic_filter_get_name(void *type_data) {
    UNUSED_PARAMETER(type_data);
    return obs_module_text("FilterName");
}
//----------------------------------------------------------------------------------------------------------------------

void about_property(obs_properties_t *props) {
    obs_properties_add_text(
        props,
        "plugin_info",
        "<a href=\"https://shadertastic.com\">Shadertastic</a> (" PROJECT_VERSION ") by <a href=\"http://about.xurei.io/\">xurei</a>",
        OBS_TEXT_INFO
    );
}
//----------------------------------------------------------------------------------------------------------------------

MODULE_EXPORT const char *obs_module_name(void) {
    #ifdef DEV_MODE
        return "Shader Transitions (dev)";
    #else
        return obs_module_text("ModuleName");
    #endif
}
//----------------------------------------------------------------------------------------------------------------------

MODULE_EXPORT const char *obs_module_description(void) {
    return obs_module_text("ModuleDescription");
}
//----------------------------------------------------------------------------------------------------------------------
