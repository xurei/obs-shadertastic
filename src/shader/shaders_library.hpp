class shaders_library_t {
    private:
        std::map<std::string, effect_shader *> shaders;
        effect_shader fallback_shader;

    public:
        void load() {
            char *fallback_shader_path = obs_module_file("effects/fallback_effect.hlsl");
            debug("fallback_shader_path %s", fallback_shader_path);
            fallback_shader.load42(fallback_shader_path);
            bfree(fallback_shader_path);
        }

        effect_shader * get(std::string path) {
            debug("shaders_library.get %s", path.c_str());
            auto it = shaders.find(path);
            if (it == shaders.end()) {
                char *shader_path = this->get_shader_path(path);
                debug("shaders_library.get 2 %s", shader_path);
                if (shader_path == NULL) {
                    return NULL;
                }
                else {
                    effect_shader *new_shader = new effect_shader();
                    new_shader->load42(shader_path);
                    if (new_shader->effect == NULL) {
                        shaders.emplace(path, &fallback_shader);
                    }
                    else {
                        shaders.emplace(path, new_shader);
                    }
                    if (shader_path != NULL) {
                        bfree(shader_path);
                    }
                    return this->get(path);
                }
            }
            else {
                return it->second;
            }
        }

        char * get_shader_path(std::string path) {
            return obs_module_file((std::string("effects/") + path + "/main.hlsl").c_str());
        }

        void unload() {
            for (auto & [_, shader]: this->shaders) {
                if (shader != &fallback_shader) {
                    shader->release42();
                    delete shader;
                }
                fallback_shader.release42();
            }
        }

        void reload(std::string path) {
            effect_shader *shader = this->get(path);
            char *shader_path = this->get_shader_path(path);
            if (shader != NULL && shader_path != NULL) {
                shader->load42(shader_path);
                bfree(shader_path);
            }
        }
};

shaders_library_t shaders_library;
