class effect_parameter_list_int : public effect_parameter {
    public:
        effect_parameter_list_int(gs_eparam_t *shader_param, obs_data_t *metadata)
        :effect_parameter(sizeof(int), shader_param, metadata) {
            this->set_defaults();
        }
        virtual ~effect_parameter_list_int() {

        }

        virtual void set_defaults() {
            obs_data_set_default_array(metadata, "values", obs_data_array_create());
            obs_data_set_default_array(metadata, "default", 0);
        }

        virtual void set_default(obs_data_t *settings, const char *full_param_name) {
            obs_data_set_default_int(settings, full_param_name, obs_data_get_int(metadata, "default"));
        }

        virtual void render_property_ui(const char *full_param_name, obs_properties_t *props) {
            const char *param_label = obs_data_get_string(metadata, "label");
            obs_property_t *list_ui = obs_properties_add_list(
                props, full_param_name, param_label,
                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT
            );
            obs_data_array_t *array = obs_data_get_array(metadata, "values");
            size_t array_count = obs_data_array_count(array);
            for (size_t i=0; i<array_count; ++i) {
                obs_data_t *item = obs_data_array_item(array, i);
                obs_property_list_add_int(list_ui, obs_data_get_string(item, "label"), obs_data_get_int(item, "value"));
                obs_data_release(item);
            }
            obs_data_array_release(array);
        }

        virtual void set_data_from_settings(obs_data_t *settings, const char *full_param_name) {
            *((int*)this->data) = (int)obs_data_get_int(settings, full_param_name);
            //debug("%s = %d", full_param_name, *((int*)this->data));
        }

        virtual void set_data_from_default(obs_data_t *metadata) {
            *((int*)this->data) = (int)obs_data_get_int(metadata, "default");
        }
};
