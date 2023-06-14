#define try_gs_effect_set_int(param, val) if (param) { gs_effect_set_int(param, val); }

class effect_parameter_int : public effect_parameter {
    private:
        int default_value;
        bool is_slider;
        int param_min;
        int param_max;
        int param_step;

    public:
        effect_parameter_int(gs_eparam_t *shader_param) : effect_parameter(sizeof(int), shader_param) {
        }

        virtual void set_defaults(obs_data_t *metadata) {
            obs_data_set_default_bool(metadata, "slider", false);
            obs_data_set_default_int(metadata, "min", 0);
            obs_data_set_default_int(metadata, "max", 100);
            obs_data_set_default_int(metadata, "step", 1);
            obs_data_set_default_int(metadata, "default", 50);

            default_value = (int)obs_data_get_int(metadata, "default");
            is_slider = obs_data_get_bool(metadata, "slider");
            param_min = (int)obs_data_get_int(metadata, "min");
            param_max = (int)obs_data_get_int(metadata, "max");
            param_step = (int)obs_data_get_int(metadata, "step");
        }

        virtual void set_default(obs_data_t *settings, const char *full_param_name) {
            obs_data_set_default_int(settings, full_param_name, default_value);
        }

        virtual void render_property_ui(const char *full_param_name, obs_properties_t *props) {
            if (is_slider) {
                obs_properties_add_int_slider(props, full_param_name, label.c_str(), param_min, param_max, param_step);
            }
            else {
                obs_properties_add_int(props, full_param_name, label.c_str(), param_min, param_max, param_step);
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
