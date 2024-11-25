class params_list {
    private:
    std::list<effect_parameter *> params;
    std::map<std::string, effect_parameter *> params_map;

    public:
    params_list() { }
    params_list(const params_list &source) {
        this->params.insert(this->params.begin(), source.params.begin(), source.params.end());
        this->params_map.insert(source.params_map.begin(), source.params_map.end());
    }

    void clear() {
        this->params.clear();
        this->params_map.clear();
    }

    effect_parameter * get(std::string param_name) {
        auto it = params_map.find(param_name);
        if (it == params_map.end()) {
            return nullptr;
        }
        else {
            return it->second;
        }
    }

    void put(std::string param_name, effect_parameter *param) {
        this->params_map[param_name] = param;
        this->params.push_back(param);
    }

    auto begin() {
        return params.begin();
    }
    auto end() {
        return params.end();
    }
};
