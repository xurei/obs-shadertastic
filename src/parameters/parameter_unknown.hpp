class effect_parameter_unknown : public effect_parameter {
    public:
        effect_parameter_unknown(gs_eparam_t *shader_param, obs_data_t *metadata)
        :effect_parameter(sizeof(int), shader_param, metadata) {
            this->set_defaults();
        }
        virtual ~effect_parameter_unknown() {

        }

        virtual void set_defaults() {
        }

        virtual void set_default(obs_data_t *settings, const char *full_param_name) {
            UNUSED_PARAMETER(settings);
            UNUSED_PARAMETER(full_param_name);
        }

        virtual void render_property_ui(const char *full_param_name, obs_properties_t *props) {
            char message[1024];
            sprintf(message, "Unknown type for %s", full_param_name);
            obs_properties_add_text(props, "", message, OBS_TEXT_INFO);
        }

        virtual void set_data_from_settings(obs_data_t *settings, const char *full_param_name) {
            UNUSED_PARAMETER(settings);
            UNUSED_PARAMETER(full_param_name);
            *((int*)this->data) = 0;
        }

        virtual void set_data_from_default(obs_data_t *metadata) {
            UNUSED_PARAMETER(metadata);
            *((int*)this->data) = 0;
        }
};
