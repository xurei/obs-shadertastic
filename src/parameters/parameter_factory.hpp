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

class effect_parameter_factory {
    public:
        effect_parameter *create(const std::string &effect_name, gs_eparam_t *shader_param, obs_data_t *param_metadata) {
            const char *param_name = obs_data_get_string(param_metadata, "name");
            const char *data_type = obs_data_get_string(param_metadata, "type");
            if (param_name == NULL || strcmp(param_name, "") == 0) {
                do_log(LOG_WARNING, "Missing name for a parameter in effect %s", effect_name.c_str());
                return NULL;
            }
            else if (data_type == NULL || strcmp(data_type, "") == 0) {
                do_log(LOG_WARNING, "Missing data type for a parameter %s in effect %s", param_name, effect_name.c_str());
                return NULL;
            }
            else {
                effect_param_datatype datatype = effect_parse_datatype(data_type);
                effect_parameter *out;
                switch(datatype) {
                    case PARAM_DATATYPE_BOOL: {
                        out = new effect_parameter_bool(shader_param);
                        break;
                    }
                    case PARAM_DATATYPE_INT: {
                        out = new effect_parameter_int(shader_param);
                        break;
                    }
                    case PARAM_DATATYPE_DOUBLE: {
                        out = new effect_parameter_double(shader_param);
                        break;
                    }
                    case PARAM_DATATYPE_LIST_INT: {
                        out = new effect_parameter_list_int(shader_param);
                        break;
                    }
                    case PARAM_DATATYPE_IMAGE: {
                        out = new effect_parameter_image(shader_param);
                        break;
                    }
                    case PARAM_DATATYPE_SOURCE: {
                        out = new effect_parameter_source(shader_param);
                        break;
                    }
                    case PARAM_DATATYPE_AUDIOLEVEL: {
                        out = new effect_parameter_audiolevel(shader_param);
                        break;
                    }
                    case PARAM_DATATYPE_UNKNOWN: default: {
                        out = new effect_parameter_unknown(shader_param);
                        break;
                    }
                }
                out->load_common_fields(param_metadata);
                out->set_defaults(param_metadata);
                return out;
            }
        }

    private:
        static effect_param_datatype effect_parse_datatype(const char *datatype_str) {
            if (strcmp(datatype_str, "float") == 0 || strcmp(datatype_str, "double") == 0) {
                return PARAM_DATATYPE_DOUBLE;
            }
            else if (strcmp(datatype_str, "bool") == 0) {
                return PARAM_DATATYPE_BOOL;
            }
            else if (strcmp(datatype_str, "int") == 0) {
                return PARAM_DATATYPE_INT;
            }
            else if (strcmp(datatype_str, "list_int") == 0) {
                return PARAM_DATATYPE_LIST_INT;
            }
            else if (strcmp(datatype_str, "image") == 0) {
                return PARAM_DATATYPE_IMAGE;
            }
            else if (strcmp(datatype_str, "source") == 0) {
                return PARAM_DATATYPE_SOURCE;
            }
            else if (strcmp(datatype_str, "audiolevel") == 0) {
                return PARAM_DATATYPE_AUDIOLEVEL;
            }
            else {
                return PARAM_DATATYPE_UNKNOWN;
            }
        }
};

effect_parameter_factory parameter_factory;
