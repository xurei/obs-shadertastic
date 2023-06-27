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

class effect_parameter_image : public effect_parameter {
    private:
        std::string path; // c'est le path en fait MERCI MAHONEKO VRAIMENT !
        gs_texture_t * texture = NULL;


    public:
        effect_parameter_image(gs_eparam_t *shader_param) : effect_parameter(sizeof(float), shader_param) {
        }

        virtual ~effect_parameter_image() {
            if (this->texture != NULL) {
                gs_texture_destroy(this->texture);
            }
        }

        virtual void set_defaults(obs_data_t *metadata) {
            UNUSED_PARAMETER(metadata);
        }

        virtual void set_default(obs_data_t *settings, const char *full_param_name) {
            UNUSED_PARAMETER(settings);
            UNUSED_PARAMETER(full_param_name);
            //obs_data_set_default_double(settings, full_param_name, default_value);
            //obs_data_set_default_string(settings, NULL);
        }

        virtual void render_property_ui(const char *full_param_name, obs_properties_t *props) {
            obs_properties_add_path(props, full_param_name, label.c_str(), OBS_PATH_FILE, "Image (*.jpg *.jpeg *.png)", NULL);
        }

        virtual void set_data_from_default() {
            this->path = std::string("");
        }

        virtual void set_data_from_settings(obs_data_t *settings,
                                            const char *full_param_name) {
            const char *path_ = obs_data_get_string(settings, full_param_name);
            if (path_ != NULL) {
                this->path = std::string(path_);
                this->load_texture();
            }
            else {
                if (this->texture != NULL) {
                    gs_texture_destroy(this->texture);
                    this->texture = NULL;
                }
            }
        }

        void load_texture() {
            // C'est pour gérer l'ouverture du fichier, courage c'est presque fini
            obs_enter_graphics();
            if (this->texture != NULL) {
                gs_texture_destroy(this->texture);
            }
            this->texture = gs_texture_create_from_file(path.c_str());
            obs_leave_graphics();
        }

        virtual void try_gs_set_val() {
            try_gs_effect_set_texture(shader_param, this->texture);
        }
};