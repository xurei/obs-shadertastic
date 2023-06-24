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

std::string get_full_param_name_static(std::string effect_name, std::string param_name) {
    return effect_name + '.' + param_name;
}

class effect_parameter {
    protected:
        void *data;
        gs_eparam_t *shader_param;
        std::string name;
        std::string label;
        size_t data_size;

    public:
        effect_parameter(size_t data_size, gs_eparam_t *shader_param) {
            this->data_size = data_size;
            this->shader_param = shader_param;
            this->data = bmalloc(data_size);
        }

        virtual ~effect_parameter() {
            bfree(this->data);
        }

        void load_common_fields(obs_data_t *metadata) {
            name = std::string(obs_data_get_string(metadata, "name"));
            label = std::string(obs_data_get_string(metadata, "label"));
        }

        virtual void set_defaults(obs_data_t *metadata) = 0;

        virtual void set_default(obs_data_t *settings, const char *full_param_name) = 0;

        virtual void render_property_ui(const char *effect_name, obs_properties_t *props) = 0;

        virtual void set_data_from_settings(obs_data_t *settings, const char *full_param_name) = 0;

        virtual void set_data_from_default() = 0;

        std::string get_full_param_name(const char *effect_name) {
            std::string effect_name_str = std::string(effect_name);
            return get_full_param_name(effect_name_str);
        }
        std::string get_full_param_name(std::string effect_name) {
            return get_full_param_name_static(effect_name, this->name);
        }

        gs_eparam_t * get_shader_param() {
            return shader_param;
        }
        size_t get_data_size() {
            return data_size;
        }
        inline void * get_data() {
            return data;
        }
};
