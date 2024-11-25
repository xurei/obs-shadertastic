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

class effect_parameter_bool : public effect_parameter {
    private:
        bool default_value{};

    public:
        // Note: the shaders need a 4-byte data for booleans, this is NOT a mistake to use sizeof(int).
        explicit effect_parameter_bool(gs_eparam_t *shader_param) : effect_parameter(sizeof(int), shader_param) {
        }

        effect_param_datatype type() override {
            return PARAM_DATATYPE_BOOL;
        }

        void set_defaults(obs_data_t *metadata) override {
            obs_data_set_default_bool(metadata, "default", false);

            default_value = obs_data_get_bool(metadata, "default");
        }

        void set_default(obs_data_t *settings, const char *full_param_name) override {
            obs_data_set_default_bool(settings, full_param_name, default_value);
        }

        void render_property_ui(const char *full_param_name, obs_properties_t *props) override {
            auto prop = obs_properties_add_bool(props, full_param_name, label.c_str());
            if (!description.empty()) {
                obs_property_set_long_description(prop, obs_module_text(description.c_str()));
            }
        }

        void set_data_from_settings(obs_data_t *settings, const char *full_param_name) override {
            *((int*)this->data) = obs_data_get_bool(settings, full_param_name) ? 1 : 0;
        }

        void set_data_from_default() override {
            *((int*)this->data) = default_value ? 1 : 0;
        }
};
