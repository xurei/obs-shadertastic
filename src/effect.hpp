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

#include "params_list.hpp"

struct shadertastic_effect_t {
    const std::string path;
    const std::string name;
    std::string label;
    int nb_steps;
    bool is_fallback = false;
    bool input_time = false;
    params_list effect_params;
    effect_shader *main_shader = NULL;

    shadertastic_effect_t(std::string name_, std::string path_): name(name_), path(path_) {}

    static bool is_effect(std::string path) {
        return os_file_exists((path + "/meta.json").c_str());
    }

    void load() {
        std::string metadata_path = normalize_path(this->path + "/meta.json");
        debug(">>>>>>>>>>>>>>> load_effect %s %s %s", this->name.c_str(), this->path.c_str(), metadata_path.c_str());

        this->main_shader = shaders_library.get(this->path);

        obs_data_t *metadata = obs_data_create_from_json_file(metadata_path.c_str());
        if (metadata == NULL) {
            // Something went wrong -> set default configuration
            warn("Unable to open metadata file for effect %s. Check the JSON syntax", name.c_str());
            label = name;
            nb_steps = 1;
        }
        else {
            const char *effect_label = obs_data_get_string(metadata, "label");
            if (effect_label == NULL) {
                label = name;
            }
            else {
                label = std::string(effect_label);
            }
            obs_data_set_default_int(metadata, "steps", 1);
            nb_steps = (int)obs_data_get_int(metadata, "steps");

            obs_data_set_default_bool(metadata, "input_time", false);
            input_time = obs_data_get_bool(metadata, "input_time");

            obs_data_array_t *parameters = obs_data_get_array(metadata, "parameters");
            if (parameters == NULL) {
                warn("No parameters specified for effect %s", name.c_str());
                parameters = obs_data_array_create();
            }

            // Copy the effect params map to allow recycling
            params_list previous_effect_params(effect_params);
            effect_params.clear();

            for (size_t i=0; i < obs_data_array_count(parameters); i++) {
                obs_data_t *param_metadata = obs_data_array_item(parameters, i);
                const char *param_name = obs_data_get_string(param_metadata, "name");
                gs_eparam_t *shader_param = gs_effect_get_param_by_name(main_shader->effect, param_name);
                effect_parameter *effect_param = parameter_factory.create(name, shader_param, param_metadata);

                if (effect_param != NULL) {
                    std::string param_name_str = std::string(param_name);
                    effect_parameter *previous_param = previous_effect_params.get(param_name_str);
                    if (previous_param != NULL) {
                        if (previous_param->get_data_size() == effect_param->get_data_size()) {
                            debug("Recycling data for %s (size: %i)", param_name_str.c_str(), (int)effect_param->get_data_size());
                            memcpy(effect_param->get_data(), previous_param->get_data(), effect_param->get_data_size());
                        }
                        else {
                            effect_param->set_data_from_default();
                        }
                    }
                    else {
                        effect_param->set_data_from_default();
                    }

                    effect_params.put(param_name_str, effect_param);
                }

                obs_data_release(param_metadata);
            }

            // Clear memory of removed params
            for (auto param: previous_effect_params) {
                debug ("Free removed param %s", param->get_name().c_str());
                delete param;
            }

            obs_data_array_release(parameters);
            obs_data_release(metadata);
            debug("Loaded effect %s from %s", name.c_str(), metadata_path.c_str());
        }
    }

    void reload() {
        shaders_library.reload(this->path);
        load();
    }

    void set_params(gs_texture_t *a, gs_texture_t *b, float t, uint32_t cx, uint32_t cy, float rand_seed) {
        /* texture setters look reversed, but they aren't */
        if (gs_get_color_space() == GS_CS_SRGB) {
            /* users want nonlinear fade */
            try_gs_effect_set_texture(main_shader->param_tex_a, a);
            try_gs_effect_set_texture(main_shader->param_tex_b, b);
        }
        else {
            /* nonlinear fade is too wrong, so use linear fade */
            try_gs_effect_set_texture_srgb(main_shader->param_tex_a, a);
            try_gs_effect_set_texture_srgb(main_shader->param_tex_b, b);
        }
        //debug("input textures set");

        try_gs_effect_set_float(main_shader->param_time, t);
        try_gs_effect_set_float(main_shader->param_upixel, (float)(1.0/cx));
        try_gs_effect_set_float(main_shader->param_vpixel, (float)(1.0/cy));
        try_gs_effect_set_float(main_shader->param_rand_seed, rand_seed);
        try_gs_effect_set_int(main_shader->param_nb_steps, nb_steps);
        //debug("common params set");

        for (auto param: effect_params) {
            param->try_gs_set_val();
        }
        //debug("all params set");
    }

    void set_step_params(int current_step, gs_texture_t *interm) {
        if (gs_get_color_space() == GS_CS_SRGB) {
            /* users want nonlinear fade */
            try_gs_effect_set_texture(main_shader->param_tex_interm, interm);
        }
        else {
            /* nonlinear fade is too wrong, so use linear fade */
            try_gs_effect_set_texture_srgb(main_shader->param_tex_interm, interm);
        }
        try_gs_effect_set_int(main_shader->param_current_step, current_step);
    }

    void render_shader(uint32_t cx, uint32_t cy) {
        const char *tech_name = "Draw";
        if (gs_get_color_space() == GS_CS_SRGB) {
            /* users want nonlinear fade */
        }
        else {
            /* nonlinear fade is too wrong, so use linear fade */
            tech_name = "DrawLinear";
        }

        while (gs_effect_loop(main_shader->effect, tech_name)) {
            gs_draw_sprite(NULL, 0, cx, cy);
        }
        //debug("end of draw");
    }

    void show() {
        debug("show %s", this->name.c_str());
        for (auto param: this->effect_params) {
            if (param != NULL) {
                param->show();
            }
        }
    }

    void hide() {
        debug("hide %s", this->name.c_str());
        for (auto param: this->effect_params) {
            if (param != NULL) {
                param->hide();
            }
        }
    }

    void release() {
        for (auto effect_param: effect_params) {
            delete effect_param;
        }
    }
};
