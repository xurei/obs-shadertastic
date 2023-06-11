static void *shadertastic_filter_create(obs_data_t *settings, obs_source_t *source) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(bzalloc(sizeof(struct shadertastic_filter)));
    s->source = source;
    s->effects = new transition_effects_map_t();
    s->rand_seed = (float)rand() / RAND_MAX;
    s->start_time = obs_get_video_frame_time();

    debug("MODULE PATH : %s", obs_module_file(""));
    debug("Settings : %s", obs_data_get_json(settings));

    debug("%s", obs_data_get_json(settings));

    std::vector<std::string> dirs = list_directories(obs_module_file("effects"));
    uint8_t transparent_tex_data[2 * 2 * 4] = {0};
    const uint8_t *transparent_tex = transparent_tex_data;
    s->transparent_texture = gs_texture_create(2, 2, GS_RGBA, 1, &transparent_tex, 0);
    s->interm_texrender[0] = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    s->interm_texrender[1] = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    s->filter_source_texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

    for (const auto &dir : dirs) {
        transition_effect_t effect;
        load_effect(effect, dir);
        if (effect.main_shader.effect != NULL) {
            s->effects->insert(transition_effects_map_t::value_type(dir, effect));

            // Defaults must be set here and not in the transition_defaults() function.
            // as the effects are not loaded yet in transition_defaults()
            for (auto &[_, param] : effect.effect_params) {
                std::string full_param_name = param->get_full_param_name(effect.name.c_str());
                param->set_default(settings, full_param_name.c_str());
            }
        }
        else {
            debug ("NOT LOADING %s", dir.c_str());
            debug ("NOT LOADING main_shader %p", effect.main_shader.effect);
        }
    }

    obs_source_update(source, settings);
    return s;
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_filter_destroy(void *data) {
    debug("Destroy");
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);

    obs_enter_graphics();
    gs_texrender_destroy(s->interm_texrender[0]);
    gs_texrender_destroy(s->interm_texrender[1]);
    gs_texrender_destroy(s->filter_source_texrender);
    obs_leave_graphics();
    debug("Destroy2");

    s->release();
    debug("Destroy3");
    bfree(data);
    debug("Destroyed");
}
//----------------------------------------------------------------------------------------------------------------------

uint32_t shadertastic_filter_getwidth(void *data)
{
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    return s->width;
}

uint32_t shadertastic_filter_getheight(void *data)
{
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    return s->height;
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_filter_update(void *data, obs_data_t *settings) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    //debug("Update : %s", obs_data_get_json(settings));

    const char *selected_effect_name = obs_data_get_string(settings, "effect");
    s->selected_effect = &((*s->effects)[selected_effect_name]);

    if (s->selected_effect != NULL) {
        //debug("Selected Effect: %s", selected_effect_name);
        for (auto &[_, param] : s->selected_effect->effect_params) {
            const char *param_name = obs_data_get_string(param->get_metadata(), "name");
            char full_param_name[512];
            sprintf(full_param_name, "%s.%s", selected_effect_name, param_name);

            param->set_data_from_settings(settings, full_param_name);
            //info("Assigned value:  %s %lu", full_param_name, param.data_size);
        }
    }

    s->speed = obs_data_get_double(settings, "speed");
    //debug("SELECTED SCENE: %s %p", obs_data_get_string(settings, "scene_b"), s->filter_scene_b);
}
//----------------------------------------------------------------------------------------------------------------------

static void shadertastic_filter_tick(void *data, float seconds) {
    UNUSED_PARAMETER(seconds);
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    obs_source_t *target = obs_filter_get_target(s->source);

    s->width = obs_source_get_base_width(target);
    s->height = obs_source_get_base_height(target);

    //debug("tick %f %dx%d", seconds, s->width, s->height);

//	obs_source_t *target = obs_filter_get_target(s->source);
//
//	// Determine offsets from expansion values.
//	int base_width = obs_source_get_base_width(target);
//	int base_height = obs_source_get_base_height(target);
//
//	s->uv_size.x = (float)base_width;
//	s->uv_size.y = (float)base_height;
//
//	s->uv_scale.x = (float)s->total_width / base_width;
//	s->uv_scale.y = (float)s->total_height / base_height;
//
//	s->uv_offset.x = (float)(-s->expand_left) / base_width;
//	s->uv_offset.y = (float)(-s->expand_top) / base_height;
//
//	if (s->shader_start_time == 0) {
//		s->shader_start_time = s->elapsed_time + seconds;
//	}
//	s->elapsed_time += seconds;
//	s->elapsed_time_loop += seconds;
//	if (s->elapsed_time_loop > 1.) {
//		s->elapsed_time_loop -= 1.;
//
//		// Loops
//		s->loops += 1;
//		if (s->loops >= 4194304)
//			s->loops = -s->loops;
//	}
//	s->local_time = (float)(os_gettime_ns() / 1000000000.0);
//
//
//	// undecided between this and "rand_float(1);"
//	s->rand_f = (float)((double)rand_interval(0, 10000) / (double)10000);
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_filter_shader_render(void *data, gs_texture_t *a, gs_texture_t *b, float t, uint32_t cx, uint32_t cy) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);

    const bool previous = gs_framebuffer_srgb_enabled();
    gs_enable_framebuffer_srgb(true);

    transition_effect_t *effect = s->selected_effect;

    if (effect != NULL) {
        gs_texture_t *interm_texture = s->transparent_texture;
        for (int current_step=0; current_step < effect->nb_steps - 1; ++current_step) {
            //debug("%d", current_step);
            s->interm_texrender_buffer = (s->interm_texrender_buffer+1) & 1;
            gs_texrender_reset(s->interm_texrender[s->interm_texrender_buffer]);
            gs_texrender_begin(s->interm_texrender[s->interm_texrender_buffer], cx, cy);
            effect->set_params(a, b, t, cx, cy, s->rand_seed);
            effect->set_step_params(current_step, interm_texture);
            effect->render_shader(cx, cy);
            gs_texrender_end(s->interm_texrender[s->interm_texrender_buffer]);
            s->interm_texture = gs_texrender_get_texture(s->interm_texrender[s->interm_texrender_buffer]);
            interm_texture = s->interm_texture;
        }
        effect->set_params(a, b, t, cx, cy, s->rand_seed);
        effect->set_step_params(effect->nb_steps - 1, interm_texture);
        effect->render_shader(cx, cy);
    }

    gs_enable_framebuffer_srgb(previous);
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_filter_video_render(void *data, gs_effect_t *effect) {
    //debug("--------");
    UNUSED_PARAMETER(effect);
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);

    const enum gs_color_space preferred_spaces[] = {
        GS_CS_SRGB,
        GS_CS_SRGB_16F,
        GS_CS_709_EXTENDED,
    };
    obs_source_t *target_source = obs_filter_get_target(s->source);

    uint32_t cx = obs_source_get_base_width(s->source);
    uint32_t cy = obs_source_get_base_height(s->source);

    const enum gs_color_space source_space = obs_source_get_color_space(target_source, OBS_COUNTOF(preferred_spaces), preferred_spaces);
    const enum gs_color_format format = gs_get_format_from_space(source_space);

    transition_effect_t *selected_effect = s->selected_effect;
    if (selected_effect != NULL) {
        if (obs_source_process_filter_begin_with_color_space(s->source, format, source_space, OBS_ALLOW_DIRECT_RENDERING)) {
            uint64_t frame_time = obs_get_video_frame_time();
            uint64_t frame_time2 = frame_time - s->start_time;
            float filter_time = (float)(s->speed < 0.001 ? 0.0 : (float)((frame_time - s->start_time) / (1000000000.0) * s->speed));
            //filter_time = filter_time - floor(filter_time);
            //debug("frame_time: %lu -> %lu -> %f", frame_time, frame_time2, filter_time);
            //filter_time = 0.3f;

            selected_effect->set_params(NULL, NULL, filter_time, cx, cy, s->rand_seed);
            selected_effect->set_step_params(selected_effect->nb_steps - 1, NULL);
            //selected_effect->render_shader(cx, cy);

            //shadertastic_filter_shader_render(s, source_tex, s->transparent_texture, filter_time, cx, cy);

			gs_blend_state_push();
			gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
            obs_source_process_filter_end(s->source, selected_effect->main_shader.effect, 0, 0);
			gs_blend_state_pop();
        }
    }
    else {
        info("No effect selected");
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

    if (s->selected_effect != NULL) {
        obs_property_set_visible(obs_properties_get(props, (s->selected_effect->name + "__params").c_str()), false);
    }

    //shadertastic_filter_properties(priv);
    const char *select_effect_name = obs_data_get_string(data, "effect");
    debug("CALLBACK : %s", select_effect_name);
    auto selected_effect = s->effects->find(std::string(select_effect_name));
    if (selected_effect != s->effects->end()) {
        debug("CALLBACK : %s -> %s", select_effect_name, selected_effect->second.name.c_str());
        obs_property_set_visible(obs_properties_get(props, (selected_effect->second.name + "__params").c_str()), true);
    }

    return true;
}

bool shadertastic_filter_reload_button_click(obs_properties_t *props, obs_property_t *property, void *data) {
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);

    #ifdef DEV_MODE
        reload_effect(s->selected_effect);
    #endif
    return true;
}

obs_properties_t *shadertastic_filter_properties(void *data) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    //struct shadertastic_filter *shadertastic_filter = data;
    obs_properties_t *props = obs_properties_create();
    //obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

    obs_property_t *p;

    // Filter settings
    obs_properties_add_float_slider(props, "speed", "Speed", 0.0, 300.0, 1.0);
    #ifdef DEV_MODE
    obs_properties_add_button(props, "reload_btn", "Reload", shadertastic_filter_reload_button_click);
    #endif

    // Shader mode
    p = obs_properties_add_list(props, "effect", "Effect", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    for (auto& [effect_name, effect] : *(s->effects)) {
        const char *effect_label = effect.label.c_str();
        obs_property_list_add_string(p, effect_label, effect_name.c_str());
    }
    obs_property_set_modified_callback2(p, shadertastic_filter_properties_change_effect_callback, data);

    obs_property_t *bla = obs_properties_get(props, "effect");

    for (auto& [effect_name, effect] : *(s->effects)) {
        const char *effect_label = effect.label.c_str();
        obs_properties_t *effect_group = obs_properties_create();
        //obs_properties_add_text(effect_group, "", effect_name, OBS_TEXT_INFO);
        for (auto &[_, param] : effect.effect_params) {
            std::string full_param_name = param->get_full_param_name(effect_name);
            param->render_property_ui(full_param_name.c_str(), effect_group);
        }
        obs_properties_add_group(props, (effect_name + "__params").c_str(), effect_label, OBS_GROUP_NORMAL, effect_group);
        obs_property_set_visible(obs_properties_get(props, (effect_name + "__params").c_str()), false);
    }
    if (s->selected_effect != NULL) {
        obs_property_set_visible(obs_properties_get(props, (s->selected_effect->name + "__params").c_str()), true);
    }

//    TODO ABOUT PLUGIN SHOULD GO HERE
    return props;
}
//----------------------------------------------------------------------------------------------------------------------

void shadertastic_filter_defaults(void *data, obs_data_t *settings) {
    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    obs_data_set_default_double(settings, "transition_point", 50.0);
}
//----------------------------------------------------------------------------------------------------------------------

static enum gs_color_space shadertastic_filter_get_color_space(void *data, size_t count, const enum gs_color_space *preferred_spaces) {
    UNUSED_PARAMETER(count);
    UNUSED_PARAMETER(preferred_spaces);

    struct shadertastic_filter *s = static_cast<shadertastic_filter*>(data);
    return obs_transition_video_get_color_space(s->source);
}
//----------------------------------------------------------------------------------------------------------------------
