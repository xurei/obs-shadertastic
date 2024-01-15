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

class effect_parameter_color_alpha : public effect_parameter {
    private:
        int default_value;

        // Function to convert an RGBA string to an integer
        int rgbaStringToInt(std::string rgba) {
            if (rgba.size() == 7) {
                rgba = std::string("#FF"+rgba.substr(1));
            }

            // Check if the input string is in the correct format
            if (rgba.size() != 9 || rgba[0] != '#' || rgba.find_first_not_of("0123456789ABCDEFabcdef", 1) != std::string::npos) {
                do_log(LOG_ERROR, "Invalid color string '%s'. Accepted formats are #RRGGBB and #AARRGGBB", rgba.c_str());
                return 0xFF000000; // Return the defaut color
            }

            // Extract the hexadecimal values for R, G, B, and A
            int a = std::strtol(rgba.substr(1, 2).c_str(), nullptr, 16);
            int r = std::strtol(rgba.substr(3, 2).c_str(), nullptr, 16);
            int g = std::strtol(rgba.substr(5, 2).c_str(), nullptr, 16);
            int b = std::strtol(rgba.substr(7, 2).c_str(), nullptr, 16);

            // Combine the values into a single integer
            int result = (a << 24) | (b << 16) | (g << 8) | r;
            return result;
        }

    public:
        effect_parameter_color_alpha(gs_eparam_t *shader_param) : effect_parameter(sizeof(vec4), shader_param) {
        }

        virtual effect_param_datatype type() {
            return PARAM_DATATYPE_COLOR_ALPHA;
        }

        virtual void set_defaults(obs_data_t *metadata) {
            obs_data_set_default_string(metadata, "default", "#000000FF");
            default_value = rgbaStringToInt(std::string(obs_data_get_string(metadata, "default")));
        }

        virtual void set_default(obs_data_t *settings, const char *full_param_name) {
            obs_data_set_default_int(settings, full_param_name, default_value);
        }

        virtual void render_property_ui(const char *full_param_name, obs_properties_t *props) {
            auto prop = obs_properties_add_color_alpha(props, full_param_name, label.c_str());
            if (!description.empty()) {
                obs_property_set_long_description(prop, obs_module_text(description.c_str()));
            }
        }

        virtual void set_data_from_settings(obs_data_t *settings, const char *full_param_name) {
            //*((int*)this->data) = (int)obs_data_get_int(settings, full_param_name);
            //debug("%s = %d", full_param_name, *((int*)this->data));
            vec4_from_rgba((vec4*) this->data, (int)obs_data_get_int(settings, full_param_name));
        }

        virtual void set_data_from_default() {
            vec4_from_rgba((vec4*) this->data, default_value);
            //*((int*)this->data) = (int)default_value;
        }
};
