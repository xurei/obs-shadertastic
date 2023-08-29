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


static bool effect_parameter_source_add(void *data, obs_source_t *source) {
    std::list<std::string> *sources_list = (std::list<std::string>*)(data);

    uint32_t flags = obs_source_get_output_flags(source);
    obs_source_type type = obs_source_get_type(source);

    if ((flags & OBS_SOURCE_VIDEO) && ((type == OBS_SOURCE_TYPE_INPUT) || (type == OBS_SOURCE_TYPE_SCENE))) {
        const char *name = obs_source_get_name(source);
        if (name != NULL) {
            sources_list->push_back(std::string(name));
        }
    }
    return true;
}

class effect_parameter_source : public effect_parameter {
    private:
        gs_texrender_t *source_texrender = NULL;
        obs_source_t *source = NULL;


    public:
        effect_parameter_source(gs_eparam_t *shader_param) : effect_parameter(sizeof(float), shader_param) {
            this->source_texrender = gs_texrender_create(GS_RGBA16, GS_ZS_NONE);
        }

        virtual ~effect_parameter_source() {
            if (this->source != NULL) {
                obs_source_release(this->source);
                this->source = NULL;
            }
            if (this->source_texrender != NULL) {
                obs_enter_graphics();
                gs_texrender_destroy(this->source_texrender);
                this->source_texrender = NULL;
                obs_leave_graphics();
            }
        }

        virtual void set_defaults(obs_data_t *metadata) {
            UNUSED_PARAMETER(metadata);
        }

        virtual void set_default(obs_data_t *settings, const char *full_param_name) {
            UNUSED_PARAMETER(settings);
            UNUSED_PARAMETER(full_param_name);
        }

        virtual void render_property_ui(const char *full_param_name, obs_properties_t *props) {
            obs_property_t *p = obs_properties_add_list(props, full_param_name, label.c_str(), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
            std::list<std::string> sources_list;
            obs_enum_sources(effect_parameter_source_add, &sources_list);
            obs_enum_scenes(effect_parameter_source_add, &sources_list);
            sources_list.sort(compare_nocase);
            for (const std::string &str: sources_list) {
                obs_property_list_add_string(p, str.c_str(), str.c_str());
            }
        }

        virtual void set_data_from_default() {
            // TODO check that we should maybe decrement the showing counter here ?
            this->source = NULL;
        }

        virtual void set_data_from_settings(obs_data_t *settings, const char *full_param_name) {
            if (this->source != NULL) {
                this->hide();
                obs_source_release(this->source);
                this->source = NULL;
            }

            this->source = obs_get_source_by_name(obs_data_get_string(settings, full_param_name));
            if (this->source != NULL) {
                this->show();
            }
        }

        virtual void try_gs_set_val() {
            if (this->source != NULL) {
                gs_texrender_reset(this->source_texrender);
                int cx = obs_source_get_width(this->source);
                int cy = obs_source_get_height(this->source);
                if (gs_texrender_begin(this->source_texrender, cx, cy)) {
                    gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f); // This line took me A WHOLE WEEK to figure out
                    obs_source_video_render(this->source);
                    gs_texrender_end(this->source_texrender);
                    gs_texture_t *texture = gs_texrender_get_texture(this->source_texrender);
                    try_gs_effect_set_texture(shader_param, texture);
                }
            }
        }

        virtual void show() {
            if (this->source != NULL) {
                debug("Inc showing %s", obs_source_get_name(this->source));
                obs_source_inc_showing(this->source);
            }
        }

        virtual void hide() {
            if (this->source != NULL) {
                debug("Dec showing %s", obs_source_get_name(this->source));
                obs_source_dec_showing(this->source);
            }
        }
};
