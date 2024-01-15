/******************************************************************************
    Copyright (C) 2024 by xurei <xureilab@gmail.com>

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

class effect_parameter_text : public effect_parameter {
    public:
        std::string value;

        explicit effect_parameter_text(gs_eparam_t *shader_param) : effect_parameter(sizeof(int), shader_param) {
        }

        virtual effect_param_datatype type() {
            return PARAM_DATATYPE_UNKNOWN;
        }

        virtual void set_defaults(obs_data_t *metadata) {
            const char *value_c_str = obs_data_get_string(metadata, "value");
            value = std::string(value_c_str);
        }

        virtual void set_default(obs_data_t *settings, const char *full_param_name) {
            UNUSED_PARAMETER(settings);
            UNUSED_PARAMETER(full_param_name);
        }

        virtual void render_property_ui(const char *full_param_name, obs_properties_t *props) {
            obs_properties_add_text(
                props,
                full_param_name,
                value.c_str(),
                OBS_TEXT_INFO
            );
        }

        virtual void set_data_from_settings(obs_data_t *settings, const char *full_param_name) {
            UNUSED_PARAMETER(settings);
            UNUSED_PARAMETER(full_param_name);
            *((int*)this->data) = 0;
        }

        virtual void set_data_from_default() {
            *((int*)this->data) = 0;
        }
};
