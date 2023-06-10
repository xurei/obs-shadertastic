typedef std::map<std::string, transition_effect_t> transition_effects_map_t;

struct shadertastic_transition {
    obs_source_t *source;
    bool transition_started;
    bool transitioning;
    float transition_point;
    gs_texture_t *transition_texture;
    gs_texrender_t *transition_texrender[2];
    int transition_texrender_buffer = 0;
    gs_texture_t *transparent_texture;
    float rand_seed;

    bool is_filter = false;
    gs_texrender_t *filter_source_a_texrender;
    gs_texrender_t *filter_source_b_texrender;
    double filter_time = 0.0;
    obs_source_t *filter_scene_b;

    obs_transition_audio_mix_callback_t mix_a;
    obs_transition_audio_mix_callback_t mix_b;
    float transition_a_mul;
    float transition_b_mul;

    transition_effects_map_t *effects;
    transition_effect_t *selected_effect;

    void release() {
        for (auto& [key, effect] : *this->effects) {
            effect.release();
        }
        delete this->effects;
    }
};
//----------------------------------------------------------------------------------------------------------------------

struct shadertastic_filter {
    obs_source_t *source;
    gs_texture_t *interm_texture;
    gs_texrender_t *interm_texrender[2];
    int interm_texrender_buffer = 0;
    gs_texture_t *transparent_texture;
    float rand_seed;

    int width, height;

    gs_texrender_t *filter_source_texrender;
    double speed = 1.0;
    uint64_t start_time = 0;

    transition_effects_map_t *effects;
    transition_effect_t *selected_effect;

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
