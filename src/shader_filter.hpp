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

static void *shadertastic_filter_create(obs_data_t *settings, obs_source_t *source) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(bzalloc(sizeof(struct shadertastic_filter)));
    s->source = source;
    s->effects = new shadertastic_effects_map_t();
    s->rand_seed = (float)rand() / (float)RAND_MAX;

    debug("FILTER %s Settings : %s", obs_source_get_name(source), obs_data_get_json(settings));

    uint8_t transparent_tex_data[2 * 2 * 4] = {0};
    const uint8_t *transparent_tex = transparent_tex_data;
    obs_enter_graphics();
    s->transparent_texture = gs_texture_create(2, 2, GS_RGBA, 1, &transparent_tex, 0);
    s->interm_texrender[0] = gs_texrender_create(GS_RGBA16, GS_ZS_NONE);
    s->interm_texrender[1] = gs_texrender_create(GS_RGBA16, GS_ZS_NONE);
    obs_leave_graphics();

    char *filters_dir_ = obs_module_file("effects");
    std::string filters_dir(filters_dir_);
    bfree(filters_dir_);

    load_effects(s, settings, filters_dir, "filter");
    if (shadertastic_settings.effects_path != nullptr) {
        load_effects(s, settings, *(shadertastic_settings.effects_path), "filter");
    }

    obs_source_update(source, settings);

    return s;
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_filter_destroy(void *data) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);

    obs_enter_graphics();
    gs_texture_destroy(s->transparent_texture);
    gs_texrender_destroy(s->interm_texrender[0]);
    gs_texrender_destroy(s->interm_texrender[1]);
    obs_leave_graphics();
    face_detection_destroy(&s->face_detection);
    s->release();
    bfree(data);
}
//----------------------------------------------------------------------------------------------------------------------

uint32_t shadertastic_filter_getwidth(void *data) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    return s->width;
}

uint32_t shadertastic_filter_getheight(void *data) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    return s->height;
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_filter_update(void *data, obs_data_t *settings) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    //debug("Update : %s", obs_data_get_json(settings));

    if (s->should_reload) {
        s->should_reload = false;
        obs_source_update_properties(s->source);
    }

    const char *selected_effect_name = obs_data_get_string(settings, "effect");
    auto selected_effect_it = s->effects->find(selected_effect_name);
    if (selected_effect_it != s->effects->end()) {
        s->selected_effect = &(selected_effect_it->second);
    }

    if (s->selected_effect != nullptr) {
        //debug("Selected Effect: %s", selected_effect_name);
        for (auto param: s->selected_effect->effect_params) {
            std::string full_param_name = param->get_full_param_name(selected_effect_name);
            param->set_data_from_settings(settings, full_param_name.c_str());
            //info("Assigned value:  %s %lu", full_param_name, param.data_size);
        }

        s->speed = obs_data_get_double(settings, get_full_param_name_static(selected_effect_name, "speed").c_str());
        s->reset_time_on_show = obs_data_get_bool(settings, get_full_param_name_static(selected_effect_name, "reset_time_on_show").c_str());
        if (s->selected_effect->input_time) {
            debug("%s RESET %i", get_full_param_name_static(selected_effect_name, "reset_time_on_show").c_str(), s->reset_time_on_show ? 1 : 0);
        }
    }
}
//----------------------------------------------------------------------------------------------------------------------

static void shadertastic_filter_tick(void *data, float seconds) {
    UNUSED_PARAMETER(seconds);
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    obs_source_t *target = obs_filter_get_target(s->source);

    s->width = obs_source_get_base_width(target);
    s->height = obs_source_get_base_height(target);

    bool is_enabled = obs_source_enabled(s->source);
    if (!s->face_detection.created && s->selected_effect && s->selected_effect->input_facedetection) {
        face_detection_create(&s->face_detection);
    }
    if (is_enabled != s->was_enabled) {
        s->was_enabled = is_enabled;
        debug("TOGGLE ENABLED %i", is_enabled ? 1 : 0);

        if (s->reset_time_on_show) {
            s->time = 0.0;
        }
    }
    uint64_t frame_interval = obs_get_frame_interval_ns();
    s->time += (double)(
        s->speed < 0.0001 ? 0.0 : (((double)frame_interval/1000000000.0) * s->speed)
    );
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_filter_video_render(void *data, gs_effect_t *effect) {
    //debug("--------");
    UNUSED_PARAMETER(effect);
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    float filter_time = (float)s->time;

    const enum gs_color_space preferred_spaces[] = {
        GS_CS_SRGB,
        GS_CS_SRGB_16F,
        GS_CS_709_EXTENDED,
    };
    obs_source_t *target_source = obs_filter_get_target(s->source);

    uint32_t cx = s->width;
    uint32_t cy = s->height;

    const enum gs_color_space source_space = obs_source_get_color_space(target_source, OBS_COUNTOF(preferred_spaces), preferred_spaces);
    const enum gs_color_format format = gs_get_format_from_space(source_space);

    shadertastic_effect_t *selected_effect = s->selected_effect;
    if (selected_effect != nullptr && selected_effect->main_shader != nullptr) {
        if (selected_effect->input_facedetection && s->face_detection.created) {
            face_detection_render(&s->face_detection, target_source, selected_effect->main_shader);
        }
        gs_texture_t *interm_texture = s->transparent_texture;
        if (obs_source_process_filter_begin_with_color_space(s->source, format, source_space, OBS_ALLOW_DIRECT_RENDERING)) {
            gs_blend_state_push();
            gs_blend_function_separate(
                GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA,
                GS_BLEND_ONE, GS_BLEND_INVSRCALPHA
            );
            struct vec4 clear_color{0,0,0,0};

            for (int current_step=0; current_step < selected_effect->nb_steps; ++current_step) {
                bool texrender_ok = true;
                bool is_interm_step = (current_step < selected_effect->nb_steps - 1);

                if (is_interm_step) {
                    s->interm_texrender_buffer = (s->interm_texrender_buffer+1) & 1;
                    gs_texrender_reset(s->interm_texrender[s->interm_texrender_buffer]);
                    texrender_ok = gs_texrender_begin(s->interm_texrender[s->interm_texrender_buffer], cx, cy);

                    if (texrender_ok) {
                        gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
                        gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f); // This line took me A WHOLE WEEK to figure out
                    }
                }

                if (texrender_ok) {
                    selected_effect->set_params(nullptr, nullptr, filter_time, cx, cy, s->rand_seed);
                    selected_effect->set_step_params(current_step, interm_texture);

                    obs_source_process_filter_end(s->source, selected_effect->main_shader->effect, cx, cy);
                    if (is_interm_step) {
                        gs_texrender_end(s->interm_texrender[s->interm_texrender_buffer]);
                        interm_texture = gs_texrender_get_texture(s->interm_texrender[s->interm_texrender_buffer]);
                    }
                }
                else {
                    break;
                }
            }
            gs_blend_state_pop();
        }
    }
    else {
        //debug("%s : No effect selected", obs_source_get_name(s->source));
        obs_source_skip_video_filter(s->source);
    }
}
//----------------------------------------------------------------------------------------------------------------------

obs_properties_t *shadertastic_filter_properties(void *data);

bool shadertastic_filter_properties_change_effect_callback(void *priv, obs_properties_t *props, obs_property_t *p, obs_data_t *data) {
    UNUSED_PARAMETER(priv);
    UNUSED_PARAMETER(p);
    UNUSED_PARAMETER(data);
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(priv);

    if (s->selected_effect != nullptr) {
        obs_property_set_visible(obs_properties_get(props, (s->selected_effect->name + "__params").c_str()), false);
        obs_property_set_visible(obs_properties_get(props, (s->selected_effect->name + "__warning").c_str()), false);
    }

    //shadertastic_filter_properties(priv);
    const char *select_effect_name = obs_data_get_string(data, "effect");
    debug("CALLBACK : %s", select_effect_name);
    auto selected_effect = s->effects->find(std::string(select_effect_name));
    if (selected_effect != s->effects->end()) {
        debug("CALLBACK : %s -> %s", select_effect_name, selected_effect->second.name.c_str());
        obs_property_set_visible(obs_properties_get(props, (selected_effect->second.name + "__params").c_str()), true);
        obs_property_set_visible(obs_properties_get(props, (selected_effect->second.name + "__warning").c_str()), true);
    }

    return true;
}

bool shadertastic_filter_reload_button_click(obs_properties_t *props, obs_property_t *property, void *data) {
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);

    if (s->selected_effect != nullptr) {
        s->selected_effect->reload();
    }
    s->should_reload = true;
    s->rand_seed = (float)rand() / (float)RAND_MAX;
    obs_source_update(s->source, nullptr);
    return true;
}

obs_properties_t *shadertastic_filter_properties(void *data) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    obs_properties_t *props = obs_properties_create();

    obs_property_t *p;

    // Dev mode settings
    if (shadertastic_settings.dev_mode_enabled) {
        obs_properties_add_button(props, "reload_btn", "Reload", shadertastic_filter_reload_button_click);
    }

    // Shader mode
    p = obs_properties_add_list(props, "effect", "Effect", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    for (auto& [effect_name, effect] : *(s->effects)) {
        const char *effect_label = effect.label.c_str();
        obs_property_list_add_string(p, effect_label, effect_name.c_str());
    }
    obs_property_set_modified_callback2(p, shadertastic_filter_properties_change_effect_callback, data);

    for (auto& [effect_name, effect] : *(s->effects)) {
        const char *effect_label = effect.label.c_str();
        obs_properties_t *effect_group = obs_properties_create();
        obs_properties_t *warning_group = nullptr;
        //obs_properties_add_text(effect_group, "", effect_name, OBS_TEXT_INFO);

        if (effect.input_facedetection) {
            if (!warning_group) {
                warning_group = obs_properties_create();
            }
            obs_properties_add_text(
                warning_group,
                (effect_name + "__warning__message").c_str(),
                "⚗️ This effect uses the Face Detection feature, which is still in an experimental state. Use at your own risk.",
                OBS_TEXT_INFO
            );
        }
        if (effect.input_time) {
            obs_properties_add_float_slider(effect_group, get_full_param_name_static(effect_name, std::string("speed")).c_str(), "Speed", 0.0, 1.0, 0.01);
            obs_properties_add_bool(effect_group, get_full_param_name_static(effect_name, std::string("reset_time_on_show")).c_str(), "Reset time on visibility toggle");
        }

        for (auto param: effect.effect_params) {
            std::string full_param_name = param->get_full_param_name(effect_name);
            param->render_property_ui(full_param_name.c_str(), effect_group);
        }
        if (warning_group) {
            obs_properties_add_group(props, (effect_name + "__warning").c_str(), "⚠ Warning", OBS_GROUP_NORMAL, warning_group);
            if (s->selected_effect != &effect) {
                obs_property_set_visible(obs_properties_get(props, (effect_name + "__warning").c_str()), false);
            }
        }
        obs_properties_add_group(props, (effect_name + "__params").c_str(), effect_label, OBS_GROUP_NORMAL, effect_group);
        obs_property_set_visible(obs_properties_get(props, (effect_name + "__params").c_str()), false);
    }
    if (s->selected_effect != nullptr) {
        obs_property_set_visible(obs_properties_get(props, (s->selected_effect->name + "__params").c_str()), true);
        obs_property_set_visible(obs_properties_get(props, (s->selected_effect->name + "__warning").c_str()), true);
    }

    about_property(props);

    return props;
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_filter_defaults(void *data, obs_data_t *settings) {
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(settings);
}
//----------------------------------------------------------------------------------------------------------------------

static enum gs_color_space shadertastic_filter_get_color_space(void *data, size_t count, const enum gs_color_space *preferred_spaces) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    const enum gs_color_space source_space = obs_source_get_color_space(
        obs_filter_get_target(s->source),
        count, preferred_spaces);

    return source_space;
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_filter_show(void *data) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    shadertastic_effect_t *selected_effect = s->selected_effect;
    if (selected_effect != nullptr) {
        selected_effect->show();
    }
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_filter_hide(void *data) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    shadertastic_effect_t *selected_effect = s->selected_effect;
    if (selected_effect != nullptr) {
        selected_effect->hide();
    }
}
//----------------------------------------------------------------------------------------------------------------------
