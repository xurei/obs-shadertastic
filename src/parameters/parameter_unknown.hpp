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

class effect_parameter_unknown : public effect_parameter {
    public:
        effect_parameter_unknown(gs_eparam_t *shader_param) : effect_parameter(sizeof(int), shader_param) {
        }

        virtual void set_defaults(obs_data_t *metadata) {
            UNUSED_PARAMETER(metadata);
        }

        virtual void set_default(obs_data_t *settings, const char *full_param_name) {
            UNUSED_PARAMETER(settings);
            UNUSED_PARAMETER(full_param_name);
        }

        virtual void render_property_ui(const char *full_param_name, obs_properties_t *props) {
            obs_properties_add_text(
                props,
                full_param_name,
                (std::string("Unknown type for ") + std::string(full_param_name)).c_str(),
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
