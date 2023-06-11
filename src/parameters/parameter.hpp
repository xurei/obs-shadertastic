class effect_parameter {
    public:
        effect_parameter(size_t data_size, gs_eparam_t *shader_param, obs_data_t *metadata) {
            this->data_size = data_size;
            this->shader_param = shader_param;
            this->metadata = metadata;
            this->data = bmalloc(data_size);
        }

        virtual ~effect_parameter() {
            if (metadata != NULL) {
                obs_data_release(metadata);
                metadata = NULL;
            }
            bfree(this->data);
            #ifndef DEV_MODE
                // FIXME This seems to make the windows version crash. Needs to be inspected
                //if (data != NULL) {
                //    bfree(data);
                //    data = NULL;
                //}
            #endif
        }

        virtual void set_defaults() = 0;

        virtual void set_default(obs_data_t *settings, const char *full_param_name) = 0;

        virtual void render_property_ui(const char *effect_name, obs_properties_t *props) = 0;

        virtual void set_data_from_settings(obs_data_t *settings, const char *full_param_name) = 0;

        virtual void set_data_from_default(obs_data_t *metadata) = 0;

        std::string get_full_param_name(const char *effect_name) {
            std::string effect_name_str = std::string(effect_name);
            return get_full_param_name(effect_name_str);
        }
        std::string get_full_param_name(std::string effect_name) {
            const char *param_name = obs_data_get_string(metadata, "name");
            return std::string(effect_name) + '.' + param_name;
        }

        gs_eparam_t * get_shader_param() {
            return shader_param;
        }
        obs_data_t * get_metadata() {
            return metadata;
        }
        size_t get_data_size() {
            return data_size;
        }
        inline void * get_data() {
            return data;
        }

    protected:
        void *data;
        gs_eparam_t *shader_param;
        obs_data_t *metadata;
        size_t data_size;
};
