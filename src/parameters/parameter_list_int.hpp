struct effect_parameter_list_int_value {
    std::string label;
    int value;
};

class effect_parameter_list_int : public effect_parameter {
    private:
        int default_value;
        std::vector<effect_parameter_list_int_value> values;

    public:
        effect_parameter_list_int(gs_eparam_t *shader_param) : effect_parameter(sizeof(int), shader_param) {
        }

        virtual void set_defaults(obs_data_t *metadata) {
            obs_data_set_default_array(metadata, "values", obs_data_array_create());
            obs_data_set_default_array(metadata, "default", 0);

            default_value = (int)obs_data_get_int(metadata, "default");
            obs_data_array_t *array = obs_data_get_array(metadata, "values");
            size_t array_count = obs_data_array_count(array);
            values.resize(array_count);
            for (size_t i=0; i<array_count; ++i) {
                obs_data_t *item = obs_data_array_item(array, i);
                values[i].label = std::string(obs_data_get_string(item, "label"));
                values[i].value = (int)obs_data_get_int(item, "value");
                obs_data_release(item);
            }
            obs_data_array_release(array);
        }

        virtual void set_default(obs_data_t *settings, const char *full_param_name) {
            obs_data_set_default_int(settings, full_param_name, default_value);
        }

        virtual void render_property_ui(const char *full_param_name, obs_properties_t *props) {
            obs_property_t *list_ui = obs_properties_add_list(
                props, full_param_name, label.c_str(),
                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT
            );
            for (size_t i=0; i < values.size(); ++i) {
                obs_property_list_add_int(list_ui, values[i].label.c_str(), values[i].value);
            }
        }

        virtual void set_data_from_settings(obs_data_t *settings, const char *full_param_name) {
            *((int*)this->data) = (int)obs_data_get_int(settings, full_param_name);
            //debug("%s = %d", full_param_name, *((int*)this->data));
        }

        virtual void set_data_from_default() {
            *((int*)this->data) = (int)default_value;
        }
};
