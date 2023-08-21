/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>

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

#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <list>
#include <map>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpacerItem>
#include <QHBoxLayout>
#include <QVBoxLayout>
#define QT_UTF8(str) QString::fromUtf8(str, -1)

#include <util/platform.h>
#include "version.h"
#include "string_util.hpp"
#include "list_files.hpp"

#define LOG_OFFSET_DB 6.0f
#define LOG_RANGE_DB 96.0f

#define try_gs_effect_set_val(param, data, data_size) if (param) { gs_effect_set_val(param, data, data_size); }
#define try_gs_effect_set_texture(param, val) if (param) { gs_effect_set_texture(param, val); }
#define try_gs_effect_set_texture_srgb(param, val) if (param) { gs_effect_set_texture_srgb(param, val); }

#define do_log(level, format, ...) \
    blog(level, "[shadertastic] " format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)

#ifdef DEV_MODE
    #define debug(format, ...) info(format, ##__VA_ARGS__)
#else
    #define debug(format, ...)
#endif

#include "shader/shader.hpp"
#include "shader/shaders_library.hpp"
#include "parameters/parameter.hpp"
#include "parameters/parameter_bool.hpp"
#include "parameters/parameter_double.hpp"
#include "parameters/parameter_int.hpp"
#include "parameters/parameter_list_int.hpp"
#include "parameters/parameter_unknown.hpp"
#include "parameters/parameter_image.hpp"
#include "parameters/parameter_source.hpp"
#include "parameters/parameter_factory.hpp"
#include "effect.hpp"
#include "shadertastic.hpp"
//----------------------------------------------------------------------------------------------------------------------

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("xurei")
OBS_MODULE_USE_DEFAULT_LOCALE(
    #ifdef DEV_MODE
      "shadertastic-dev"
    #else
      "shadertastic"
    #endif
, "en-US")
//----------------------------------------------------------------------------------------------------------------------

shadertastic_settings_t shadertastic_settings;
//----------------------------------------------------------------------------------------------------------------------

obs_data_t * load_settings() {
    char *file = obs_module_config_path("settings.json");
    char *path_abs = os_get_abs_path_ptr(file);
    debug("Settings path: %s", path_abs);

    obs_data_t *settings = obs_data_create_from_json_file(path_abs);

    if (!settings) {
        info("Settings not found. Creating default settings in %s ...", file);
        os_mkdirs(obs_module_config_path(""));
        // Create default settings
        settings = obs_data_create();
        if (obs_data_save_json(settings, file)) {
            info("Settings saved to %s", file);
        }
        else {
            warn("Failed to save settings to file.");
        }
    }
    else {
        blog(LOG_INFO, "Settings loaded successfully");
    }
    bfree(file);
    bfree(path_abs);
    return settings;
}

void apply_settings(obs_data_t *settings) {
    if (shadertastic_settings.effects_path != NULL) {
        delete shadertastic_settings.effects_path;
    }
    const char *effects_path_str = obs_data_get_string(settings, SETTING_EFFECTS_PATH);
    if (effects_path_str != NULL) {
        shadertastic_settings.effects_path = new std::string(effects_path_str);
    }
    else {
        shadertastic_settings.effects_path = NULL;
    }
}

void save_settings(obs_data_t *settings) {
    char *configPath = obs_module_config_path("settings.json");
    debug("%s", obs_data_get_json(settings));

    if (obs_data_save_json(settings, configPath)) {
        blog(LOG_INFO, "Settings saved to %s", configPath);
    }
    else {
        blog(LOG_WARNING, "Failed to save settings to file.");
    }

    if (configPath != NULL) {
        bfree(configPath);
    }
}
//----------------------------------------------------------------------------------------------------------------------

void load_effects(shadertastic_common *s, obs_data_t *settings, const std::string effects_dir) {
    std::vector<std::string> dirs = list_directories(effects_dir.c_str());

    for (const auto &dir : dirs) {
        std::string effect_path = effects_dir + "/" + dir;
        if (shadertastic_effect_t::is_effect(effect_path)) {
            debug("Effect %s", effect_path.c_str());
            shadertastic_effect_t effect(dir, effect_path);
            effect.load();
            if (effect.main_shader != NULL) {
                s->effects->insert(shadertastic_effects_map_t::value_type(dir, effect));

                // Defaults must be set here and not in the transition_defaults() function.
                // as the effects are not loaded yet in transition_defaults()
                for (auto param: effect.effect_params) {
                    std::string full_param_name = param->get_full_param_name(effect.name.c_str());
                    param->set_default(settings, full_param_name.c_str());
                }
            }
            else {
                debug ("NOT LOADING FILTER %s", dir.c_str());
            }
        }
    }
}
//----------------------------------------------------------------------------------------------------------------------

#include "shader_filter.hpp"
#include "shader_transition.hpp"
//----------------------------------------------------------------------------------------------------------------------

static void show_settings_dialog() {
    obs_data_t *settings = load_settings();

    // Create the settings dialog
    QDialog *dialog = new QDialog();
    dialog->setWindowTitle("Shadertastic");
    dialog->setFixedSize(dialog->sizeHint());
    QVBoxLayout *mainLayout = new QVBoxLayout(dialog);
    mainLayout->setContentsMargins(20, 12, 20, 12);
    QFormLayout *formLayout = new QFormLayout();

    // External folder input
    QHBoxLayout* layout = new QHBoxLayout();
    QLabel *effectsLabel = new QLabel("Additional effects path");
    formLayout->addRow(effectsLabel);
    QLineEdit *filePathLineEdit = new QLineEdit();
    filePathLineEdit->setReadOnly(true);
    filePathLineEdit->setText(QT_UTF8(obs_data_get_string(settings, SETTING_EFFECTS_PATH)));
    layout->addWidget(filePathLineEdit);
    QPushButton *filePickerButton = new QPushButton("Select folder...");
    QObject::connect(filePickerButton, &QPushButton::clicked, [=](){
        QString selectedFolder = QFileDialog::getExistingDirectory();
        if (!selectedFolder.isEmpty()) {
            const char *selectedFolderStr = selectedFolder.toUtf8().constData();
            debug("%s = %s", SETTING_EFFECTS_PATH, selectedFolderStr);
            obs_data_set_string(settings, SETTING_EFFECTS_PATH, selectedFolderStr);
            filePathLineEdit->setText(selectedFolder);
        }
        else {
            obs_data_set_string(settings, SETTING_EFFECTS_PATH, NULL);
            filePathLineEdit->setText("");
        }
    });
    layout->addWidget(filePickerButton);
    formLayout->addRow(layout);

    formLayout->addRow(new QLabel(
        "Place your own effects in this folder to load them.\n"
        "Two subfolders will be created: 'filters' and 'transitions', one for each type of effect."
    ));
    QLabel *effectsLabelDescription = new QLabel(
        "You can also find more effects in the <a href=\"https://shadertastic.com/library\">Shadertastic library</a>"
    );
    effectsLabelDescription->setOpenExternalLinks(true);
    formLayout->addRow(effectsLabelDescription);

    // OK & Cancel Buttons
    layout = new QHBoxLayout();
    layout->addStretch();
    formLayout->addRow(layout);

    QPushButton *cancelButton = new QPushButton("Cancel");
    QObject::connect(cancelButton, &QPushButton::clicked, dialog, &QDialog::close);
    layout->addWidget(cancelButton);

    QPushButton *saveButton = new QPushButton("Save");
    QObject::connect(saveButton, &QPushButton::clicked, [=]() {
        apply_settings(settings);
        save_settings(settings);
        dialog->close();
    });
    layout->addWidget(saveButton);

    // Separator
    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setLineWidth(0);
    formLayout->addRow(line);

    // About
    layout = new QHBoxLayout();
    layout->addStretch();
    QLabel *aboutLabel = new QLabel("Version " PROJECT_VERSION " by <a href=\"http://about.xurei.io/\">xurei</a>");
    aboutLabel->setOpenExternalLinks(true);
    layout->addWidget(aboutLabel);
    formLayout->addRow(layout);

    mainLayout->addLayout(formLayout);

    QObject::connect(dialog, &QDialog::finished, [=](int) {
        obs_data_release(settings);
    });

    dialog->show();
}
//----------------------------------------------------------------------------------------------------------------------

bool obs_module_load(void) {
    info("loaded version %s", PROJECT_VERSION);
    obs_data_t *settings = load_settings();
    apply_settings(settings);
    obs_data_release(settings);

    struct obs_source_info shadertastic_transition_info = {};
    shadertastic_transition_info.id =
        #ifdef DEV_MODE
          "shadertastic_transition_dev";
        #else
          "shadertastic_transition";
        #endif

    shaders_library.load();

    shadertastic_transition_info.type = OBS_SOURCE_TYPE_TRANSITION;
    shadertastic_transition_info.get_name = shadertastic_transition_get_name;
    shadertastic_transition_info.create = shadertastic_transition_create;
    shadertastic_transition_info.destroy = shadertastic_transition_destroy;
    shadertastic_transition_info.get_properties = shadertastic_transition_properties;
    shadertastic_transition_info.update = shadertastic_transition_update;
    shadertastic_transition_info.video_render = shadertastic_transition_video_render;
    shadertastic_transition_info.load = shadertastic_transition_update;
    shadertastic_transition_info.audio_render = shadertastic_transition_audio_render;
    shadertastic_transition_info.transition_start = shadertastic_transition_start;
    shadertastic_transition_info.transition_stop = shadertastic_transition_stop;
    shadertastic_transition_info.get_defaults2 = shadertastic_transition_defaults;
    //shadertastic_transition_info.video_tick = shadertastic_transition_tick;
    shadertastic_transition_info.video_get_color_space = shadertastic_transition_get_color_space;
    obs_register_source(&shadertastic_transition_info);

    struct obs_source_info shadertastic_filter_info = {};
    shadertastic_filter_info.id = "shadertastic_filter";
    shadertastic_filter_info.type = OBS_SOURCE_TYPE_FILTER;
    shadertastic_filter_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW /*| OBS_SOURCE_COMPOSITE*/;
    shadertastic_filter_info.get_name = shadertastic_filter_get_name;
    shadertastic_filter_info.create = shadertastic_filter_create;
    shadertastic_filter_info.destroy = shadertastic_filter_destroy;
    shadertastic_filter_info.get_properties = shadertastic_filter_properties;
    shadertastic_filter_info.video_tick = shadertastic_filter_tick;
    shadertastic_filter_info.update = shadertastic_filter_update;
    shadertastic_filter_info.get_width = shadertastic_filter_getwidth,
    shadertastic_filter_info.get_height = shadertastic_filter_getheight,
    shadertastic_filter_info.video_render = shadertastic_filter_video_render;
    shadertastic_filter_info.load = shadertastic_filter_update;
    shadertastic_filter_info.show = shadertastic_filter_show;
    shadertastic_filter_info.hide = shadertastic_filter_hide;
    shadertastic_filter_info.get_defaults2 = shadertastic_filter_defaults;
    shadertastic_filter_info.video_get_color_space = shadertastic_filter_get_color_space;
    obs_register_source(&shadertastic_filter_info);

    QAction *action = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(obs_module_text("Shadertastic Settings")));
    QObject::connect(action, &QAction::triggered, [] { show_settings_dialog(); });

    return true;
}
//----------------------------------------------------------------------------------------------------------------------

void obs_module_unload(void) {
    shaders_library.unload();
}
//----------------------------------------------------------------------------------------------------------------------
