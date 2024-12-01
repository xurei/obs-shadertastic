#include <memory>

class shaders_library_t {
    private:
        std::map<std::string, std::shared_ptr<effect_shader>> shaders;
        std::shared_ptr<effect_shader> fallback_shader;

        std::shared_ptr<effect_shader> load_shader_file(const std::string &path) {
            std::string shader_path = this->get_shader_path(path);
            debug("shaders_library.load_shader_file %s", shader_path.c_str());
            effect_shader *new_shader = new effect_shader();
            new_shader->load(shader_path.c_str());
            if (new_shader->effect == nullptr) {
                // Effect loading failed. Using the fallback effect to show ERR on the source
                delete new_shader;
                auto emplaced = shaders.emplace(path, fallback_shader);
                return emplaced.first->second;
            }
            else {
                auto emplaced = shaders.emplace(path, new_shader);
                return emplaced.first->second;
            }
        }

    public:
        void load() {
            char *fallback_shader_path = obs_module_file("effects/fallback_effect.hlsl");
            debug("fallback_shader_path %s", fallback_shader_path);
            fallback_shader = std::make_shared<effect_shader>();
            fallback_shader->load(fallback_shader_path);
            bfree(fallback_shader_path);
        }

        std::shared_ptr<effect_shader> get(const std::string &path) {
            debug("shaders_library.get %s", path.c_str());
            auto it = shaders.find(path);
            if (it == shaders.end()) {
                return this->load_shader_file(path);
            }
            else {
                return it->second;
            }
        }

        std::string get_shader_path(const std::string &path) {
            return (path + "/main.hlsl");
        }

        void reload(const std::string &path) {
            debug("Reloading Shader '%s'...", path.c_str());
            std::shared_ptr<effect_shader> shader_to_delete = this->get(path);
            shaders.erase(path);
            this->load_shader_file(path);
            debug("Reloading Shader DONE '%s'", path.c_str());
        }
};

shaders_library_t shaders_library;
