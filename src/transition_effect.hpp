struct transition_effect_t {
    std::string name;
    obs_data_t *metadata;
    int nb_steps;
    obs_data_array_t *parameters;
    transition_shader main_shader;
    std::map<std::string, effect_parameter *> effect_params;

    void set_params(gs_texture_t *a, gs_texture_t *b, float t, uint32_t cx, uint32_t cy, float rand_seed) {
        //debug("--------------------");
        //debug("cx %d; cy %d", cx, cy);
        /* texture setters look reversed, but they aren't */
        if (gs_get_color_space() == GS_CS_SRGB) {
            /* users want nonlinear fade */
            try_gs_effect_set_texture(main_shader.param_tex_a, a);
            try_gs_effect_set_texture(main_shader.param_tex_b, b);
        }
        else {
            /* nonlinear fade is too wrong, so use linear fade */
            try_gs_effect_set_texture_srgb(main_shader.param_tex_a, a);
            try_gs_effect_set_texture_srgb(main_shader.param_tex_b, b);
        }
        //debug("input textures set");

        try_gs_effect_set_float(main_shader.param_transition_time, t);
        try_gs_effect_set_float(main_shader.param_upixel, (float)(1.0/cx));
        try_gs_effect_set_float(main_shader.param_vpixel, (float)(1.0/cy));
        try_gs_effect_set_float(main_shader.param_rand_seed, rand_seed);
        try_gs_effect_set_int(main_shader.param_nb_steps, nb_steps);
        //debug("common params set");

        for (auto &[_, param] : effect_params) {
            gs_effect_set_val(param->get_shader_param(), param->data, param->get_data_size());
        }
        //debug("all params set");
    }

    void set_step_params(int current_step, gs_texture_t *interm) {
        if (gs_get_color_space() == GS_CS_SRGB) {
            /* users want nonlinear fade */
            try_gs_effect_set_texture(main_shader.param_tex_interm, interm);
        }
        else {
            /* nonlinear fade is too wrong, so use linear fade */
            try_gs_effect_set_texture_srgb(main_shader.param_tex_interm, interm);
        }
        try_gs_effect_set_int(main_shader.param_current_step, current_step);
    }

    void render_shader(uint32_t cx, uint32_t cy) {
        const char *tech_name = "Effect";
        if (gs_get_color_space() == GS_CS_SRGB) {
            /* users want nonlinear fade */
        }
        else {
            /* nonlinear fade is too wrong, so use linear fade */
            tech_name = "EffectLinear";
        }

        while (gs_effect_loop(main_shader.effect, tech_name)) {
            gs_draw_sprite(NULL, 0, cx, cy);
        }
        //debug("end of draw");
    }

    void release() {
        if (main_shader.effect != NULL) {
            obs_enter_graphics();
            gs_effect_destroy(main_shader.effect);
            obs_leave_graphics();
            main_shader.effect = NULL;
        }
        if (parameters != NULL) {
            obs_data_array_release(parameters);
            parameters = NULL;
        }
        if (metadata != NULL) {
            obs_data_release(metadata);
            metadata = NULL;
        }
        for (auto& [_, effect_param] : effect_params) {
            delete effect_param;
        }
    }
};
