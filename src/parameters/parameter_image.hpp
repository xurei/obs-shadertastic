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

class effect_parameter_image : public effect_parameter {
    private:
        std::string path;
        gs_texture_t * texture;

        void load_texture() {
            obs_enter_graphics();
            if (this->texture != nullptr) {
                gs_texture_destroy(this->texture);
            }
            this->texture = gs_texture_create_from_file(path.c_str());
            obs_leave_graphics();
        }

    public:
        explicit effect_parameter_image(gs_eparam_t *shader_param) : effect_parameter(sizeof(float), shader_param) {
            this->texture = nullptr;
        }

        ~effect_parameter_image() override {
            if (this->texture != nullptr) {
                obs_enter_graphics();
                gs_texture_destroy(this->texture);
                this->texture = nullptr;
                obs_leave_graphics();
            }
        }

        effect_param_datatype type() override {
            return PARAM_DATATYPE_IMAGE;
        }

        void set_defaults(obs_data_t *metadata) override {
            UNUSED_PARAMETER(metadata);
        }

        void set_default(obs_data_t *settings, const char *full_param_name) override {
            UNUSED_PARAMETER(settings);
            UNUSED_PARAMETER(full_param_name);
            //obs_data_set_default_double(settings, full_param_name, default_value);
            //obs_data_set_default_string(settings, nullptr);
        }

        void render_property_ui(const char *full_param_name, obs_properties_t *props) override {
            auto prop = obs_properties_add_path(props, full_param_name, label.c_str(), OBS_PATH_FILE, "Image (*.jpg *.jpeg *.png)", nullptr);
            if (!description.empty()) {
                obs_property_set_long_description(prop, obs_module_text(description.c_str()));
            }
        }

        void set_data_from_default() override {
            this->path = std::string("");
        }

        void set_data_from_settings(obs_data_t *settings, const char *full_param_name) override {
            const char *path_ = obs_data_get_string(settings, full_param_name);
            if (path_ != nullptr) {
                std::string path_str = std::string(path_);
                if (this->path != path_str) {
                    this->path = path_str;
                    this->load_texture();
                }
            }
            else {
                if (this->texture != nullptr) {
                    obs_enter_graphics();
                    gs_texture_destroy(this->texture);
                    this->texture = nullptr;
                    obs_leave_graphics();
                    this->path = "";
                }
            }
        }

        void try_gs_set_val() override {
            try_gs_effect_set_texture(name.c_str(), shader_param, this->texture);
        }
};
