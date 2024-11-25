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

class effect_parameter_float : public effect_parameter {
    private:
        double default_value{};
        bool is_slider{};
        double param_min{};
        double param_max{};
        double param_step{};

    public:
        explicit effect_parameter_float(gs_eparam_t *shader_param) : effect_parameter(sizeof(float), shader_param) {
        }

        effect_param_datatype type() override {
            return PARAM_DATATYPE_DOUBLE;
        }

        void set_defaults(obs_data_t *metadata) override {
            obs_data_set_default_bool(metadata, "slider", false);
            obs_data_set_default_double(metadata, "min", 0.0);
            obs_data_set_default_double(metadata, "max", 100.0);
            obs_data_set_default_double(metadata, "step", 0.01);
            obs_data_set_default_double(metadata, "default", 50.0);

            default_value = obs_data_get_double(metadata, "default");
            is_slider = obs_data_get_bool(metadata, "slider");
            param_min = obs_data_get_double(metadata, "min");
            param_max = obs_data_get_double(metadata, "max");
            param_step = obs_data_get_double(metadata, "step");
        }

        void set_default(obs_data_t *settings, const char *full_param_name) override {
            obs_data_set_default_double(settings, full_param_name, default_value);
        }

        void render_property_ui(const char *full_param_name, obs_properties_t *props) override {
            obs_property_t *prop;
            if (is_slider) {
                prop = obs_properties_add_float_slider(props, full_param_name, label.c_str(), param_min, param_max, param_step);
            }
            else {
                prop = obs_properties_add_float(props, full_param_name, label.c_str(), param_min, param_max, param_step);
            }
            if (!description.empty()) {
                obs_property_set_long_description(prop, obs_module_text(description.c_str()));
            }
        }

        void set_data_from_settings(obs_data_t *settings, const char *full_param_name) override {
            *((float*)this->data) = (float)obs_data_get_double(settings, full_param_name);
            //debug("%s = %f", full_param_name, *((float*)this->data));
        }

        void set_data_from_default() override {
            *((float*)this->data) = (float)default_value;
        }
};
