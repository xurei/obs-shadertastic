/******************************************************************************
    Copyright (C) 2023 by xurei <xureilab@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

static bool effect_parameter_audiolevel_add(void *data, obs_source_t *source) {
    std::list<std::string> *sources_list = (std::list<std::string>*)(data);

    uint32_t flags = obs_source_get_output_flags(source);
    obs_source_type type = obs_source_get_type(source);

    if ((flags & OBS_SOURCE_AUDIO) && ((type == OBS_SOURCE_TYPE_INPUT))) {
        const char *name = obs_source_get_name(source);
        if (name != nullptr) {
            sources_list->push_back(std::string(name));
        }
    }
    return true;
}

class effect_parameter_audiolevel : public effect_parameter {
    private:
        obs_weak_source_t *source = nullptr;
        obs_volmeter_t *obs_volmeter;
        float smoothing = 0.5;
        unsigned long last_callback_update = 0;

    public:
        explicit effect_parameter_audiolevel(gs_eparam_t *shader_param):
            effect_parameter(sizeof(float), shader_param),
            obs_volmeter(obs_volmeter_create(OBS_FADER_LOG))
        {
            obs_volmeter_add_callback(this->obs_volmeter, effect_parameter_audiolevel::callback, this);
            obs_volmeter_set_peak_meter_type(this->obs_volmeter, SAMPLE_PEAK_METER);
            *((float*)this->data) = -100.0;
        }

        ~effect_parameter_audiolevel() override {
            if (this->source != nullptr) {
                obs_weak_source_release(this->source);
                this->source = nullptr;
            }
            obs_volmeter_detach_source(obs_volmeter);
            obs_volmeter_remove_callback(this->obs_volmeter, effect_parameter_audiolevel::callback, this);
            obs_volmeter_destroy(this->obs_volmeter);
        }

        effect_param_datatype type() override {
            return PARAM_DATATYPE_AUDIOLEVEL;
        }

        void set_defaults(obs_data_t *metadata) override {
            UNUSED_PARAMETER(metadata);
        }

        void set_default(obs_data_t *settings, const char *full_param_name) override {
            obs_data_set_default_double(settings, full_param_name, 0.5);
        }

        void render_property_ui(const char *full_param_name, obs_properties_t *props) override {
            obs_property_t *p = obs_properties_add_list(props, full_param_name, label.c_str(), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
            std::list<std::string> sources_list;
            obs_enum_sources(effect_parameter_audiolevel_add, &sources_list);
            sources_list.sort(compare_nocase);
            for (const std::string &str: sources_list) {
                obs_property_list_add_string(p, str.c_str(), str.c_str());
            }
            auto prop = obs_properties_add_float_slider(props, (std::string(full_param_name) + "__smoothing").c_str(), "∟ Smoothing", 0.0, 0.99, 0.01);
            if (!description.empty()) {
                obs_property_set_long_description(prop, obs_module_text(description.c_str()));
            }
        }

        void set_data_from_default() override {
            // TODO check that we should not decrement the showing counter here ?
            this->source = nullptr;
        }

        void set_data_from_settings(obs_data_t *settings, const char *full_param_name) override {
            if (this->source != nullptr) {
                this->hide();

                obs_volmeter_detach_source(obs_volmeter);

                // TODO release earlier ?
                obs_weak_source_release(this->source);
                this->source = nullptr;
            }

            obs_source_t *ref_source = obs_get_source_by_name(obs_data_get_string(settings, full_param_name));
            if (ref_source != nullptr) {
                obs_volmeter_attach_source(obs_volmeter, ref_source);
                this->source = obs_source_get_weak_source(ref_source);
                this->show();
                obs_source_release(ref_source);
            }

            this->smoothing = (float)obs_data_get_double(settings, (std::string(full_param_name) + "__smoothing").c_str());
        }

        void try_gs_set_val() override {
            unsigned long now = get_time_ms();

            // If no callback has been received for more than 200ms, assume no sound
            if (now - last_callback_update > 200) {
                *((float*)this->data) = -100.0;
            }

            try_gs_effect_set_val(name.c_str(), shader_param, data, data_size);
        }

        static void callback(void *data,
          const float magnitude[MAX_AUDIO_CHANNELS],
          const float peak[MAX_AUDIO_CHANNELS],
          const float inputPeak[MAX_AUDIO_CHANNELS]) {
            UNUSED_PARAMETER(magnitude);
            UNUSED_PARAMETER(peak);
            UNUSED_PARAMETER(inputPeak);
            effect_parameter_audiolevel *param = static_cast<effect_parameter_audiolevel *>(data);

            float prev_val = *((float*)param->data);

            float clamped_peak = magnitude[0];

            if (clamped_peak < -100.0) {
                clamped_peak = -100.0;
            }
            else if (clamped_peak > 0.0) {
                clamped_peak = 0.0;
            }

            param->last_callback_update = get_time_ms();

            //debug("AUDIO LEVEL CALLBACK %lu %f", get_time_ms(), clamped_peak);

            float val = prev_val < clamped_peak ? clamped_peak : (float)(prev_val * param->smoothing + clamped_peak * (1.0 - param->smoothing));

            *((float*)param->data) = (float) val;
        }
};


