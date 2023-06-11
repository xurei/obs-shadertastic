#define try_gs_effect_set_int(param, val) if (param) { gs_effect_set_int(param, val); }

class effect_parameter_int : public effect_parameter {
    public:
        effect_parameter_int(gs_eparam_t *shader_param, obs_data_t *metadata)
        :effect_parameter(sizeof(int), shader_param, metadata) {
            this->set_defaults();
        }
        virtual ~effect_parameter_int() {

        }

        virtual void set_defaults() {
            obs_data_set_default_bool(metadata, "slider", false);
            obs_data_set_default_int(metadata, "min", 0);
            obs_data_set_default_int(metadata, "max", 100);
            obs_data_set_default_int(metadata, "step", 1);
            obs_data_set_default_int(metadata, "default", 50);
        }

        virtual void set_default(obs_data_t *settings, const char *full_param_name) {
            obs_data_set_default_int(settings, full_param_name, obs_data_get_int(metadata, "default"));
        }

        virtual void render_property_ui(const char *full_param_name, obs_properties_t *props) {
            const char *param_label = obs_data_get_string(metadata, "label");
            bool is_slider = obs_data_get_bool(metadata, "slider");
            int param_min = (int)obs_data_get_int(metadata, "min");
            int param_max = (int)obs_data_get_int(metadata, "max");
            int param_step = (int)obs_data_get_int(metadata, "step");
            if (is_slider) {
                obs_properties_add_float_slider(props, full_param_name, param_label, param_min, param_max, param_step);
            }
            else {
                obs_properties_add_float(props, full_param_name, param_label, param_min, param_max, param_step);
            }
        }

        virtual void set_data_from_settings(obs_data_t *settings, const char *full_param_name) {
            *((int*)this->data) = (int)obs_data_get_int(settings, full_param_name);
            //debug("%s = %d", full_param_name, *((int*)this->data));
        }

        virtual void set_data_from_default(obs_data_t *metadata) {
            *((int*)this->data) = (int)obs_data_get_int(metadata, "default");
        }
};
