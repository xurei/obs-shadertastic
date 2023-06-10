enum effect_param_datatype {
    PARAM_DATATYPE_UNKNOWN,
    PARAM_DATATYPE_INT,
    PARAM_DATATYPE_DOUBLE,
    PARAM_DATATYPE_LIST_INT,
};

class effect_parameter_factory {
    public:
        effect_parameter *create(std::string &effect_name, gs_eparam_t *shader_param, obs_data_t *param_metadata) {
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
                switch(datatype) {
                    case PARAM_DATATYPE_INT: {
                        return new effect_parameter_int(shader_param, param_metadata);
                    }
                    case PARAM_DATATYPE_DOUBLE: {
                        return new effect_parameter_double(shader_param, param_metadata);
                    }
                    case PARAM_DATATYPE_LIST_INT: {
                        return new effect_parameter_list_int(shader_param, param_metadata);
                    }
                    case PARAM_DATATYPE_UNKNOWN: {
                        return new effect_parameter_unknown(shader_param, param_metadata);
                    }
                }
                return new effect_parameter_unknown(shader_param, param_metadata);
            }
        }

    private:
        static effect_param_datatype effect_parse_datatype(const char *datatype_str) {
            if (strcmp(datatype_str, "float") == 0 || strcmp(datatype_str, "double") == 0) {
                return PARAM_DATATYPE_DOUBLE;
            }
            else if (strcmp(datatype_str, "int") == 0) {
                return PARAM_DATATYPE_INT;
            }
            else if (strcmp(datatype_str, "list_int") == 0) {
                return PARAM_DATATYPE_LIST_INT;
            }
            else {
                return PARAM_DATATYPE_UNKNOWN;
            }
        }
};

effect_parameter_factory parameter_factory;
