/******************************************************************************
    Copyright (C) 2023 by xurei <xureilab@gmail.com>

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

static void *shadertastic_transition_create(obs_data_t *settings, obs_source_t *source) {
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(bzalloc(sizeof(struct shadertastic_transition)));
    s->source = source;
    s->effects = new shadertastic_effects_map_t();
    s->rand_seed = (float)rand() / RAND_MAX;

    debug("TRANSITION %s Settings : %s", obs_source_get_name(source), obs_data_get_json(settings));

    char *transitions_dir_ = obs_module_file("effects/transitions");
    std::string transitions_dir(transitions_dir_);
    bfree(transitions_dir_);
    uint8_t transparent_tex_data[2 * 2 * 4] = {0};
    const uint8_t *transparent_tex = transparent_tex_data;
    obs_enter_graphics();
    s->transparent_texture = gs_texture_create(2, 2, GS_RGBA, 1, &transparent_tex, 0);
    obs_leave_graphics();
    s->transition_texrender[0] = gs_texrender_create(GS_RGBA16, GS_ZS_NONE);
    s->transition_texrender[1] = gs_texrender_create(GS_RGBA16, GS_ZS_NONE);

    load_effects(s, settings, transitions_dir);
    if (shadertastic_settings.effects_path != NULL) {
        load_effects(s, settings, *(shadertastic_settings.effects_path) + "/transitions");
    }

    obs_source_update(source, settings);
    return s;
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_transition_destroy(void *data) {
    debug("Destroy");
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);

    obs_enter_graphics();
    gs_texture_destroy(s->transparent_texture);
    gs_texrender_destroy(s->transition_texrender[0]);
    gs_texrender_destroy(s->transition_texrender[1]);
    obs_leave_graphics();
    debug("Destroy2");

    s->release();
    debug("Destroy3");
    bfree(data);
    debug("Destroyed");
}
//----------------------------------------------------------------------------------------------------------------------

static inline float calc_fade(float t, float mul) {
    t *= mul;
    return t > 1.0f ? 1.0f : t;
}
//----------------------------------------------------------------------------------------------------------------------

static float mix_a_fade_in_out(void *data, float t) {
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);
    return 1.0f - calc_fade(t, s->transition_a_mul);
}
//----------------------------------------------------------------------------------------------------------------------

static float mix_b_fade_in_out(void *data, float t) {
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);
    return 1.0f - calc_fade(1.0f - t, s->transition_b_mul);
}
//----------------------------------------------------------------------------------------------------------------------

static float mix_a_cross_fade(void *data, float t) {
    UNUSED_PARAMETER(data);
    return 1.0f - t;
}
//----------------------------------------------------------------------------------------------------------------------

static float mix_b_cross_fade(void *data, float t) {
    UNUSED_PARAMETER(data);
    return t;
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_transition_update(void *data, obs_data_t *settings) {
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);
    //debug("Update : %s", obs_data_get_json(settings));

    s->auto_reload = obs_data_get_bool(settings, "auto_reload");

    s->transition_point = (float)obs_data_get_double(settings, "transition_point") / 100.0f;
    s->transition_a_mul = (1.0f / s->transition_point);
    s->transition_b_mul = (1.0f / (1.0f - s->transition_point));

    const char *selected_effect_name = obs_data_get_string(settings, "effect");
    auto selected_effect_it = s->effects->find(selected_effect_name);
    if (selected_effect_it != s->effects->end()) {
        s->selected_effect = &(selected_effect_it->second);
    }

    if (s->selected_effect != NULL) {
        for (auto param: s->selected_effect->effect_params) {
            std::string full_param_name = param->get_full_param_name(selected_effect_name);
            param->set_data_from_settings(settings, full_param_name.c_str());
        }
    }

    if (!obs_data_get_int(settings, "audio_fade_style")) {
        s->mix_a = mix_a_cross_fade;
        s->mix_b = mix_b_cross_fade;
    }
    else {
        s->mix_a = mix_a_fade_in_out;
        s->mix_b = mix_b_fade_in_out;
    }
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_transition_render_init(void *data, gs_texture_t *a, gs_texture_t *b, float t, uint32_t cx, uint32_t cy) {
    debug("shadertastic_transition_render_init");
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(t);
    UNUSED_PARAMETER(a);
    UNUSED_PARAMETER(b);
    UNUSED_PARAMETER(cx);
    UNUSED_PARAMETER(cy);
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);
    s->transition_texrender_buffer = 0;
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_transition_shader_render(void *data, gs_texture_t *a, gs_texture_t *b, float t, uint32_t cx, uint32_t cy) {
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);

    const bool previous = gs_framebuffer_srgb_enabled();
    gs_enable_framebuffer_srgb(true);

    shadertastic_effect_t *effect = s->selected_effect;

    if (effect != NULL) {
        gs_texture_t *interm_texture = s->transparent_texture;
        struct vec4 clear_color;
        vec4_zero(&clear_color);

        for (int current_step=0; current_step < effect->nb_steps - 1; ++current_step) {
            //debug("%d", current_step);
            s->transition_texrender_buffer = (s->transition_texrender_buffer+1) & 1;
            gs_texrender_reset(s->transition_texrender[s->transition_texrender_buffer]);
            if (gs_texrender_begin(s->transition_texrender[s->transition_texrender_buffer], cx, cy)) {
                gs_blend_state_push();
                gs_blend_function_separate(
                    GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA,
                    GS_BLEND_ONE, GS_BLEND_INVSRCALPHA
                );
                gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
                gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f); // This line took me A WHOLE WEEK to figure out

                effect->set_params(a, b, t, cx, cy, s->rand_seed);
                effect->set_step_params(current_step, interm_texture);
                effect->render_shader(cx, cy);
                gs_texrender_end(s->transition_texrender[s->transition_texrender_buffer]);
                gs_blend_state_pop();
                interm_texture = gs_texrender_get_texture(s->transition_texrender[s->transition_texrender_buffer]);
            }
        }

        gs_blend_state_push();
        gs_blend_function_separate(
            GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA,
            GS_BLEND_ONE, GS_BLEND_INVSRCALPHA
        );
        effect->set_params(a, b, t, cx, cy, s->rand_seed);
        effect->set_step_params(effect->nb_steps - 1, interm_texture);
        effect->render_shader(cx, cy);
        gs_blend_state_pop();
    }

    gs_enable_framebuffer_srgb(previous);
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_transition_video_render(void *data, gs_effect_t *effect) {
    UNUSED_PARAMETER(effect);
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);

    if (s->transition_started) {
        obs_source_t *scene_a = obs_transition_get_source(s->source, OBS_TRANSITION_SOURCE_A);
        obs_source_t *scene_b = obs_transition_get_source(s->source, OBS_TRANSITION_SOURCE_B);

        if (s->auto_reload && s->selected_effect != NULL) {
            debug("AUTO RELOAD");
            s->selected_effect->reload();
            obs_source_update(s->source, NULL);
        }

        obs_transition_video_render(s->source, shadertastic_transition_render_init);

        info("Started transition: %s -> %s",
            obs_source_get_name(scene_a),
            obs_source_get_name(scene_b)
        );
        obs_source_release(scene_a);
        obs_source_release(scene_b);
        s->transitioning = true;
        s->transition_started = false;
    }

    float t = obs_transition_get_time(s->source);
    if (t >= 1.0f) {
        s->transitioning = false;
    }

    if (s->transitioning) {
        obs_transition_video_render2(s->source, shadertastic_transition_shader_render, s->transparent_texture);
    }
    else {
        enum obs_transition_target target = t < s->transition_point ? OBS_TRANSITION_SOURCE_A : OBS_TRANSITION_SOURCE_B;
        //debug("render direct");
        obs_transition_video_render_direct(s->source, target);
    }
}
//----------------------------------------------------------------------------------------------------------------------

static bool shadertastic_transition_audio_render(void *data, uint64_t *ts_out, struct obs_source_audio_mix *audio, uint32_t mixers, size_t channels, size_t sample_rate) {
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);
    if (!s) {
        return false;
    }

    uint64_t ts = 0;

    const bool success = obs_transition_audio_render(s->source, ts_out, audio, mixers, channels, sample_rate, s->mix_a, s->mix_b);
    if (!ts) {
        return success;
    }

    if (!*ts_out || ts < *ts_out) {
        *ts_out = ts;
    }

    return true;
}
//----------------------------------------------------------------------------------------------------------------------

obs_properties_t *shadertastic_transition_properties(void *data);

bool shadertastic_transition_properties_change_effect_callback(void *priv, obs_properties_t *props, obs_property_t *p, obs_data_t *data) {
    UNUSED_PARAMETER(priv);
    UNUSED_PARAMETER(p);
    UNUSED_PARAMETER(data);
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(priv);

    if (s->selected_effect != NULL) {
        obs_property_set_visible(obs_properties_get(props, (s->selected_effect->name + "__params").c_str()), false);
    }

    const char *select_effect_name = obs_data_get_string(data, "effect");
    debug("CALLBACK : %s", select_effect_name);
    auto selected_effect = s->effects->find(std::string(select_effect_name));
    if (selected_effect != s->effects->end()) {
        debug("CALLBACK : %s -> %s", select_effect_name, selected_effect->second.name.c_str());
        obs_property_set_visible(obs_properties_get(props, (selected_effect->second.name + "__params").c_str()), true);
    }

    return true;
}

obs_properties_t *shadertastic_transition_properties(void *data) {
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);
    obs_properties_t *props = obs_properties_create();

    obs_property_t *p;

    // auto reload settings (for development)
    obs_property_t *auto_reload = obs_properties_add_bool(props, "auto_reload", obs_module_text("AutoReload"));

    // audio fade settings
    obs_property_t *audio_fade_style = obs_properties_add_list(
        props, "audio_fade_style", obs_module_text("AudioFadeStyle"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT
    );
    obs_property_list_add_int(audio_fade_style, obs_module_text("CrossFade"), 0);
    obs_property_list_add_int(audio_fade_style, obs_module_text("FadeOutFadeIn"), 1);

    // Shader mode
    p = obs_properties_add_list(props, "effect", "Effect", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    for (auto& [effect_name, effect] : *(s->effects)) {
        const char *effect_label = effect.label.c_str();
        obs_property_list_add_string(p, effect_label, effect_name.c_str());
    }
    obs_property_set_modified_callback2(p, shadertastic_transition_properties_change_effect_callback, data);

    obs_property_t *bla = obs_properties_get(props, "effect");

    for (auto& [effect_name, effect] : *(s->effects)) {
        const char *effect_label = effect.label.c_str();
        obs_properties_t *effect_group = obs_properties_create();
        for (auto param: effect.effect_params) {
            std::string full_param_name = param->get_full_param_name(effect_name);
            param->render_property_ui(full_param_name.c_str(), effect_group);
        }
        obs_properties_add_group(props, (effect_name + "__params").c_str(), effect_label, OBS_GROUP_NORMAL, effect_group);
        obs_property_set_visible(obs_properties_get(props, (effect_name + "__params").c_str()), false);
    }
    if (s->selected_effect != NULL) {
        obs_property_set_visible(obs_properties_get(props, (s->selected_effect->name + "__params").c_str()), true);
    }

    about_property(props);

    return props;
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_transition_defaults(void *data, obs_data_t *settings) {
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);
    obs_data_set_default_double(settings, "transition_point", 50.0);
    obs_data_set_default_bool(settings, "auto_reload", false);
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_transition_start(void *data) {
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);
    s->rand_seed = (float)rand() / RAND_MAX;
    //debug("rand_seed = %f", s->rand_seed);

    uint32_t cx = obs_source_get_width(s->source);
    uint32_t cy = obs_source_get_height(s->source);

    if (!s->transition_started) {
        s->transition_started = true;

        debug("Started transition of %s", obs_source_get_name(s->source));
    }
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_transition_stop(void *data) {
    debug("STOP");
    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);
    if (s->transitioning) {
        s->transitioning = false;
    }
}
//----------------------------------------------------------------------------------------------------------------------

static enum gs_color_space shadertastic_transition_get_color_space(void *data, size_t count, const enum gs_color_space *preferred_spaces) {
    UNUSED_PARAMETER(count);
    UNUSED_PARAMETER(preferred_spaces);

    struct shadertastic_transition *s = static_cast<shadertastic_transition*>(data);
    return obs_transition_video_get_color_space(s->source);
}
//----------------------------------------------------------------------------------------------------------------------
