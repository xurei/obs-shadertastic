#define try_gs_effect_set_float(param, val) if (param) { gs_effect_set_float(param, val); }

class effect_parameter_double : public effect_parameter {
    public:
        effect_parameter_double(gs_eparam_t *shader_param, obs_data_t *metadata)
        :effect_parameter(sizeof(float), shader_param, metadata) {
            this->set_defaults();
        }
        virtual ~effect_parameter_double() {

        }

        virtual void set_defaults() {
            obs_data_set_default_bool(metadata, "slider", false);
            obs_data_set_default_double(metadata, "min", 0.0);
            obs_data_set_default_double(metadata, "max", 100.0);
            obs_data_set_default_double(metadata, "step", 0.1);
            obs_data_set_default_double(metadata, "default", 50.0);
        }

        virtual void set_default(obs_data_t *settings, const char *full_param_name) {
            obs_data_set_default_double(settings, full_param_name, obs_data_get_double(metadata, "default"));
        }

        virtual void render_property_ui(const char *full_param_name, obs_properties_t *props) {
            const char *param_label = obs_data_get_string(metadata, "label");
            bool is_slider = obs_data_get_bool(metadata, "slider");
            double param_min = obs_data_get_double(metadata, "min");
            double param_max = obs_data_get_double(metadata, "max");
            double param_step = obs_data_get_double(metadata, "step");
            if (is_slider) {
                obs_properties_add_float_slider(props, full_param_name, param_label, param_min, param_max, param_step);
            }
            else {
                obs_properties_add_float(props, full_param_name, param_label, param_min, param_max, param_step);
            }
        }

        virtual void set_data_from_settings(obs_data_t *settings, const char *full_param_name) {
            *((float*)this->data) = (float)obs_data_get_double(settings, full_param_name);
            //debug("%s = %f", full_param_name, *((float*)this->data));
        }

        virtual void set_data_from_default(obs_data_t *metadata) {
            *((float*)this->data) = (float)obs_data_get_double(metadata, "default");
        }
};
