class shaders_library_t {
    private:
        std::map<std::string, effect_shader *> shaders;
        effect_shader fallback_shader;

    public:
        void load() {
            char *fallback_shader_path = obs_module_file("effects/fallback_effect.hlsl");
            debug("fallback_shader_path %s", fallback_shader_path);
            fallback_shader.load(fallback_shader_path);
            bfree(fallback_shader_path);
        }

        effect_shader * get(std::string path) {
            debug("shaders_library.get %s", path.c_str());
            auto it = shaders.find(path);
            if (it == shaders.end()) {
                std::string shader_path = this->get_shader_path(path);
                debug("shaders_library.get 2 %s", shader_path.c_str());
                effect_shader *new_shader = new effect_shader();
                new_shader->load(shader_path.c_str());
                if (new_shader->effect == NULL) {
                    shaders.emplace(path, &fallback_shader);
                }
                else {
                    shaders.emplace(path, new_shader);
                }
                return this->get(path);
            }
            else {
                return it->second;
            }
        }

        std::string get_shader_path(std::string path) {
            return (path + "/main.hlsl");
        }

        void unload() {
            for (auto & [_, shader]: this->shaders) {
                if (shader != &fallback_shader) {
                    shader->release();
                    delete shader;
                }
            }
            fallback_shader.release();
        }

        void reload(std::string path) {
            std::string shader_path = this->get_shader_path(path);
            effect_shader *shader = this->get(path);
            if (shader == &fallback_shader) {
                shaders.erase(path);
                shader = this->get(path);
            }
            if (shader != NULL && shader != &fallback_shader) {
                shader->load(shader_path.c_str());
            }
        }
};

shaders_library_t shaders_library;
