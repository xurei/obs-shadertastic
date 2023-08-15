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

class effect_shader {
    public:
    gs_effect_t *effect = NULL;
    gs_eparam_t *param_tex_a = NULL;
    gs_eparam_t *param_tex_b = NULL;
    gs_eparam_t *param_tex_interm = NULL;
    gs_eparam_t *param_time = NULL;
    gs_eparam_t *param_upixel = NULL;
    gs_eparam_t *param_vpixel = NULL;
    gs_eparam_t *param_rand_seed = NULL;
    gs_eparam_t *param_current_step = NULL;
    gs_eparam_t *param_nb_steps = NULL;

    void load(const char *shader_path) {
        debug("SHADER PATH: %s", shader_path);
        char *error_string = NULL;
        char *shader_source_ = os_quick_read_utf8_file(shader_path);
        std::string shader_source = std::string(shader_source_);
        bfree(shader_source_);

        // Global common arguments, must be in all the effects for this plugin
        shader_source = (std::string("") +
            "uniform float4x4 ViewProj;\n" +
            "uniform texture2d image;\n" +
            "uniform texture2d tex_a;\n" +
            "uniform texture2d tex_b;\n" +
            "uniform texture2d tex_interm;\n" +
            "uniform float time;\n" +
            "uniform float upixel;\n" +
            "uniform float vpixel;\n" +
            "uniform float rand_seed;\n" +
            "uniform int current_step;\n" +
            "uniform int nb_steps;\n" +
            "float srgb_nonlinear_to_linear_channel(float u) { return (u <= 0.04045) ? (u / 12.92) : pow((u + 0.055) / 1.055, 2.4); }\n" +
            "float3 srgb_nonlinear_to_linear(float3 v) { return float3(srgb_nonlinear_to_linear_channel(v.r), srgb_nonlinear_to_linear_channel(v.g), srgb_nonlinear_to_linear_channel(v.b)); }\n" +
            "float fract(float v) { return v - floor(v); }\n" +
            shader_source
        );
        obs_enter_graphics();
        effect = gs_effect_create(shader_source.c_str(), shader_path, &error_string);
        obs_leave_graphics();

        if (!effect) {
            do_log(LOG_ERROR, "Could not open shader file: %s", error_string);
            bfree(error_string);
        }
        else {
            param_tex_a = gs_effect_get_param_by_name(effect, "tex_a");
            param_tex_b = gs_effect_get_param_by_name(effect, "tex_b");
            param_tex_interm = gs_effect_get_param_by_name(effect, "tex_interm");
            param_time = gs_effect_get_param_by_name(effect, "time");
            param_upixel = gs_effect_get_param_by_name(effect, "upixel");
            param_vpixel = gs_effect_get_param_by_name(effect, "vpixel");
            param_rand_seed = gs_effect_get_param_by_name(effect, "rand_seed");
            param_current_step = gs_effect_get_param_by_name(effect, "current_step");
            param_nb_steps = gs_effect_get_param_by_name(effect, "nb_steps");
        }
    }

    void release() {
        if (effect != NULL) {
            debug("Release shader");
            obs_enter_graphics();
            gs_effect_destroy(effect);
            effect = NULL;
            obs_leave_graphics();
        }
    }
};
