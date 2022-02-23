/*
 *  Copyright © 2017-2022 Wellington Wallace
 *
 *  This file is part of EasyEffects.
 *
 *  EasyEffects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  EasyEffects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with EasyEffects.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "presets_manager.hpp"

PresetsManager::PresetsManager()
    : user_config_dir(g_get_user_config_dir()),
      user_presets_dir(user_config_dir + "/easyeffects/"),
      user_input_dir(user_config_dir + "/easyeffects/input"),
      user_output_dir(user_config_dir + "/easyeffects/output"),
      autoload_input_dir(user_config_dir + "/easyeffects/autoload/input"),
      autoload_output_dir(user_config_dir + "/easyeffects/autoload/output"),
      settings(g_settings_new("com.github.wwmm.easyeffects")),
      soe_settings(g_settings_new("com.github.wwmm.easyeffects.streamoutputs")),
      sie_settings(g_settings_new("com.github.wwmm.easyeffects.streaminputs")),
      autogain(std::make_unique<AutoGainPreset>()),
      bass_enhancer(std::make_unique<BassEnhancerPreset>()),
      bass_loudness(std::make_unique<BassLoudnessPreset>()),
      compressor(std::make_unique<CompressorPreset>()),
      convolver(std::make_unique<ConvolverPreset>()),
      crossfeed(std::make_unique<CrossfeedPreset>()),
      crystalizer(std::make_unique<CrystalizerPreset>()),
      deesser(std::make_unique<DeesserPreset>()),
      delay(std::make_unique<DelayPreset>()),
      echo_canceller(std::make_unique<EchoCancellerPreset>()),
      equalizer(std::make_unique<EqualizerPreset>()),
      exciter(std::make_unique<ExciterPreset>()),
      filter(std::make_unique<FilterPreset>()),
      gate(std::make_unique<GatePreset>()),
      limiter(std::make_unique<LimiterPreset>()),
      loudness(std::make_unique<LoudnessPreset>()),
      maximizer(std::make_unique<MaximizerPreset>()),
      multiband_compressor(std::make_unique<MultibandCompressorPreset>()),
      multiband_gate(std::make_unique<MultibandGatePreset>()),
      pitch(std::make_unique<PitchPreset>()),
      reverb(std::make_unique<ReverbPreset>()),
      rnnoise(std::make_unique<RNNoisePreset>()),
      stereo_tools(std::make_unique<StereoToolsPreset>()) {
  // system presets directories provided by Glib

  for (auto scd = g_get_system_config_dirs(); *scd != nullptr; scd++) {
    system_input_dir.emplace_back(std::string(*scd) +
                                  "/easyeffects/input");  // it should be fine to use emplace_back in these cases
    system_output_dir.emplace_back(std::string(*scd) + "/easyeffects/output");
  }

  // add "/etc" to system config folders array and remove duplicates
  system_input_dir.emplace_back("/etc/easyeffects/input");
  system_output_dir.emplace_back("/etc/easyeffects/output");
  std::sort(system_input_dir.begin(), system_input_dir.end());
  std::sort(system_output_dir.begin(), system_output_dir.end());
  system_input_dir.erase(std::unique(system_input_dir.begin(), system_input_dir.end()), system_input_dir.end());
  system_output_dir.erase(std::unique(system_output_dir.begin(), system_output_dir.end()), system_output_dir.end());

  for (const auto& scd : system_input_dir) {
    util::debug(log_tag + "system input presets directory: \"" + scd.string() + "\"; ");
  }
  for (const auto& scd : system_output_dir) {
    util::debug(log_tag + "system output presets directory: \"" + scd.string() + "\"; ");
  }

  // user presets directories

  create_user_directory(user_presets_dir);
  create_user_directory(user_input_dir);
  create_user_directory(user_output_dir);
  create_user_directory(autoload_input_dir);
  create_user_directory(autoload_output_dir);

  auto* gfile = g_file_new_for_path(user_output_dir.c_str());

  user_output_monitor = g_file_monitor_directory(gfile, G_FILE_MONITOR_NONE, nullptr, nullptr);

  g_signal_connect(user_output_monitor, "changed",
                   G_CALLBACK(+[](GFileMonitor* monitor, GFile* file, GFile* other_file, GFileMonitorEvent event_type,
                                  gpointer user_data) {
                     auto self = static_cast<PresetsManager*>(user_data);

                     switch (event_type) {
                       case G_FILE_MONITOR_EVENT_CREATED: {
                         const auto preset_name = util::remove_filename_extension(g_file_get_basename(file));

                         self->user_output_preset_created.emit(preset_name);

                         break;
                       }
                       case G_FILE_MONITOR_EVENT_DELETED: {
                         const auto preset_name = util::remove_filename_extension(g_file_get_basename(file));

                         self->user_output_preset_removed.emit(preset_name);

                         break;
                       }
                       default:
                         break;
                     }
                   }),
                   this);

  g_object_unref(gfile);

  gfile = g_file_new_for_path(user_input_dir.c_str());

  user_input_monitor = g_file_monitor_directory(gfile, G_FILE_MONITOR_NONE, nullptr, nullptr);

  g_signal_connect(user_input_monitor, "changed",
                   G_CALLBACK(+[](GFileMonitor* monitor, GFile* file, GFile* other_file, GFileMonitorEvent event_type,
                                  gpointer user_data) {
                     auto self = static_cast<PresetsManager*>(user_data);

                     switch (event_type) {
                       case G_FILE_MONITOR_EVENT_CREATED: {
                         const auto preset_name = util::remove_filename_extension(g_file_get_basename(file));

                         self->user_input_preset_created.emit(preset_name);

                         break;
                       }
                       case G_FILE_MONITOR_EVENT_DELETED: {
                         const auto preset_name = util::remove_filename_extension(g_file_get_basename(file));

                         self->user_input_preset_removed.emit(preset_name);

                         break;
                       }
                       default:
                         break;
                     }
                   }),
                   this);

  g_object_unref(gfile);

  gfile = g_file_new_for_path(autoload_input_dir.c_str());

  autoload_input_monitor = g_file_monitor_directory(gfile, G_FILE_MONITOR_NONE, nullptr, nullptr);

  g_signal_connect(autoload_input_monitor, "changed",
                   G_CALLBACK(+[](GFileMonitor* monitor, GFile* file, GFile* other_file, GFileMonitorEvent event_type,
                                  gpointer user_data) {
                     auto self = static_cast<PresetsManager*>(user_data);

                     if (event_type == G_FILE_MONITOR_EVENT_CREATED || event_type == G_FILE_MONITOR_EVENT_DELETED) {
                       const auto profiles = self->get_autoload_profiles(PresetType::input);

                       self->autoload_input_profiles_changed.emit(profiles);
                     }
                   }),
                   this);

  g_object_unref(gfile);

  gfile = g_file_new_for_path(autoload_output_dir.c_str());

  autoload_output_monitor = g_file_monitor_directory(gfile, G_FILE_MONITOR_NONE, nullptr, nullptr);

  g_signal_connect(autoload_output_monitor, "changed",
                   G_CALLBACK(+[](GFileMonitor* monitor, GFile* file, GFile* other_file, GFileMonitorEvent event_type,
                                  gpointer user_data) {
                     auto self = static_cast<PresetsManager*>(user_data);

                     if (event_type == G_FILE_MONITOR_EVENT_CREATED || event_type == G_FILE_MONITOR_EVENT_DELETED) {
                       const auto profiles = self->get_autoload_profiles(PresetType::output);

                       self->autoload_output_profiles_changed.emit(profiles);
                     }
                   }),
                   this);

  g_object_unref(gfile);
}

PresetsManager::~PresetsManager() {
  g_file_monitor_cancel(user_output_monitor);
  g_file_monitor_cancel(user_input_monitor);
  g_file_monitor_cancel(autoload_input_monitor);
  g_file_monitor_cancel(autoload_output_monitor);

  g_object_unref(user_output_monitor);
  g_object_unref(user_input_monitor);
  g_object_unref(autoload_input_monitor);
  g_object_unref(autoload_output_monitor);
  g_object_unref(settings);
  g_object_unref(sie_settings);
  g_object_unref(soe_settings);

  util::debug(log_tag + "destroyed");
}

void PresetsManager::create_user_directory(const std::filesystem::path& path) {
  if (!std::filesystem::is_directory(path)) {
    if (std::filesystem::create_directories(path)) {
      util::debug(log_tag + "user presets directory created: " + path.string());
    } else {
      util::warning(log_tag + "failed to create user presets directory: " + path.string());
    }

  } else {
    util::debug(log_tag + "user presets directory already exists: " + path.string());
  }
}

auto PresetsManager::get_names(const PresetType& preset_type) -> std::vector<std::string> {
  std::filesystem::directory_iterator it;
  std::vector<std::string> names;

  // system directories search
  std::vector<std::filesystem::path> sys_dirs;

  switch (preset_type) {
    case PresetType::output:
      sys_dirs.insert(sys_dirs.end(), system_output_dir.begin(), system_output_dir.end());
      break;
    case PresetType::input:
      sys_dirs.insert(sys_dirs.end(), system_input_dir.begin(), system_input_dir.end());
      break;
  }

  for (const auto& dir : sys_dirs) {
    if (std::filesystem::exists(dir)) {
      it = std::filesystem::directory_iterator{dir};

      const auto vn = search_names(it);
      names.insert(names.end(), vn.begin(), vn.end());
    }
  }

  // user directory search
  const auto user_dir = (preset_type == PresetType::output) ? user_output_dir : user_input_dir;
  it = std::filesystem::directory_iterator{user_dir};

  const auto vn = search_names(it);
  names.insert(names.end(), vn.begin(), vn.end());

  // removing duplicates
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());

  return names;
}

auto PresetsManager::search_names(std::filesystem::directory_iterator& it) -> std::vector<std::string> {
  std::vector<std::string> names;

  try {
    while (it != std::filesystem::directory_iterator{}) {
      if (std::filesystem::is_regular_file(it->status())) {
        if (it->path().extension().c_str() == json_ext) {
          names.push_back(it->path().stem().c_str());
        }
      }

      ++it;
    }
  } catch (const std::exception& e) {
    util::warning(e.what());
  }

  return names;
}

void PresetsManager::add(const PresetType& preset_type, const std::string& name) {
  for (const auto& p : get_names(preset_type)) {
    if (p == name) {
      return;
    }
  }

  save_preset_file(preset_type, name);
}

void PresetsManager::save_blocklist(const PresetType& preset_type, nlohmann::json& json) {
  std::vector<std::string> blocklist;

  switch (preset_type) {
    case PresetType::output: {
      const auto list = util::gchar_array_to_vector(g_settings_get_strv(soe_settings, "blocklist"));

      for (const auto& l : list) {
        blocklist.push_back(l);
      }

      json["output"]["blocklist"] = blocklist;

      break;
    }
    case PresetType::input: {
      const auto list = util::gchar_array_to_vector(g_settings_get_strv(sie_settings, "blocklist"));

      for (const auto& l : list) {
        blocklist.push_back(l);
      }

      json["input"]["blocklist"] = blocklist;

      break;
    }
  }
}

void PresetsManager::load_blocklist(const PresetType& preset_type, const nlohmann::json& json) {
  std::vector<std::string> blocklist;

  switch (preset_type) {
    case PresetType::input: {
      try {
        auto list = json.at("input").at("blocklist").get<std::vector<std::string>>();

        g_settings_set_strv(sie_settings, "blocklist", util::make_gchar_pointer_vector(list).data());
      } catch (const nlohmann::json::exception& e) {
        g_settings_reset(sie_settings, "blocklist");

        util::warning(log_tag + e.what());
      }

      break;
    }
    case PresetType::output: {
      try {
        auto list = json.at("output").at("blocklist").get<std::vector<std::string>>();

        g_settings_set_strv(soe_settings, "blocklist", util::make_gchar_pointer_vector(list).data());
      } catch (const nlohmann::json::exception& e) {
        g_settings_reset(soe_settings, "blocklist");

        util::warning(log_tag + e.what());
      }

      break;
    }
  }
}

void PresetsManager::save_preset_file(const PresetType& preset_type, const std::string& name) {
  nlohmann::json json;

  std::filesystem::path output_file;

  save_blocklist(preset_type, json);

  switch (preset_type) {
    case PresetType::output: {
      const auto plugins = util::gchar_array_to_vector(g_settings_get_strv(soe_settings, "plugins"));

      std::vector<std::string> list;

      list.reserve(plugins.size());

      for (const auto& p : plugins) {
        list.push_back(p);
      }

      json["output"]["plugins_order"] = list;

      write_plugins_preset(preset_type, plugins, json);

      output_file = user_output_dir / std::filesystem::path{name.c_str() + json_ext};

      break;
    }
    case PresetType::input: {
      const auto plugins = util::gchar_array_to_vector(g_settings_get_strv(sie_settings, "plugins"));

      std::vector<std::string> list;

      list.reserve(plugins.size());

      for (const auto& p : plugins) {
        list.push_back(p);
      }

      json["input"]["plugins_order"] = list;

      write_plugins_preset(preset_type, plugins, json);

      output_file = user_input_dir / std::filesystem::path{name.c_str() + json_ext};

      break;
    }
  }

  std::ofstream o(output_file.c_str());

  o << std::setw(4) << json << std::endl;

  // std::cout << std::setw(4) << json << std::endl;

  util::debug(log_tag + "saved preset: " + output_file.string());
}

void PresetsManager::write_plugins_preset(const PresetType& preset_type,
                                          const std::vector<std::string>& plugins,
                                          nlohmann::json& json) {
  for (const auto& name : plugins) {
    if (name == plugin_name::autogain) {
      autogain->write(preset_type, json);
    } else if (name == plugin_name::bass_enhancer) {
      bass_enhancer->write(preset_type, json);
    } else if (name == plugin_name::bass_loudness) {
      bass_loudness->write(preset_type, json);
    } else if (name == plugin_name::compressor) {
      compressor->write(preset_type, json);
    } else if (name == plugin_name::convolver) {
      convolver->write(preset_type, json);
    } else if (name == plugin_name::crossfeed) {
      crossfeed->write(preset_type, json);
    } else if (name == plugin_name::crystalizer) {
      crystalizer->write(preset_type, json);
    } else if (name == plugin_name::deesser) {
      deesser->write(preset_type, json);
    } else if (name == plugin_name::delay) {
      delay->write(preset_type, json);
    } else if (name == plugin_name::echo_canceller) {
      echo_canceller->write(preset_type, json);
    } else if (name == plugin_name::equalizer) {
      equalizer->write(preset_type, json);
    } else if (name == plugin_name::exciter) {
      exciter->write(preset_type, json);
    } else if (name == plugin_name::filter) {
      filter->write(preset_type, json);
    } else if (name == plugin_name::gate) {
      gate->write(preset_type, json);
    } else if (name == plugin_name::limiter) {
      limiter->write(preset_type, json);
    } else if (name == plugin_name::loudness) {
      loudness->write(preset_type, json);
    } else if (name == plugin_name::maximizer) {
      maximizer->write(preset_type, json);
    } else if (name == plugin_name::multiband_compressor) {
      multiband_compressor->write(preset_type, json);
    } else if (name == plugin_name::multiband_gate) {
      multiband_gate->write(preset_type, json);
    } else if (name == plugin_name::pitch) {
      pitch->write(preset_type, json);
    } else if (name == plugin_name::reverb) {
      reverb->write(preset_type, json);
    } else if (name == plugin_name::rnnoise) {
      rnnoise->write(preset_type, json);
    } else if (name == plugin_name::stereo_tools) {
      stereo_tools->write(preset_type, json);
    }
  }
}

void PresetsManager::remove(const PresetType& preset_type, const std::string& name) {
  std::filesystem::path preset_file;

  const auto user_dir = (preset_type == PresetType::output) ? user_output_dir : user_input_dir;

  preset_file = user_dir / std::filesystem::path{name.c_str() + json_ext};

  if (std::filesystem::exists(preset_file)) {
    std::filesystem::remove(preset_file);

    util::debug(log_tag + "removed preset: " + preset_file.string());
  }
}

void PresetsManager::load_preset_file(const PresetType& preset_type, const std::string& name) {
  nlohmann::json json;

  std::vector<std::string> plugins;

  std::vector<std::filesystem::path> conf_dirs;

  std::filesystem::path input_file;

  auto preset_found = false;

  switch (preset_type) {
    case PresetType::output: {
      conf_dirs.push_back(user_output_dir);

      conf_dirs.insert(conf_dirs.end(), system_output_dir.begin(), system_output_dir.end());

      for (const auto& dir : conf_dirs) {
        input_file = dir / std::filesystem::path{name + json_ext};

        if (std::filesystem::exists(input_file)) {
          preset_found = true;

          break;
        }
      }

      if (preset_found) {
        try {
          std::ifstream is(input_file);

          is >> json;

          for (const auto& p : json.at("output").at("plugins_order").get<std::vector<std::string>>()) {
            for (const auto& v : plugin_name::list) {
              if (v == p) {
                plugins.push_back(p);

                break;
              }
            }
          }

        } catch (const nlohmann::json::exception& e) {
          plugins.clear();

          util::warning(log_tag + e.what());
        }

        g_settings_set_strv(soe_settings, "plugins", util::make_gchar_pointer_vector(plugins).data());
      } else {
        util::debug(log_tag + "can't find the preset " + name + " on the filesystem");

        return;
      }

      break;
    }
    case PresetType::input: {
      conf_dirs.push_back(user_input_dir);

      conf_dirs.insert(conf_dirs.end(), system_input_dir.begin(), system_input_dir.end());

      for (const auto& dir : conf_dirs) {
        input_file = dir / std::filesystem::path{name + json_ext};

        if (std::filesystem::exists(input_file)) {
          preset_found = true;

          break;
        }
      }

      if (preset_found) {
        try {
          std::ifstream is(input_file);

          is >> json;

          for (const auto& p : json.at("input").at("plugins_order").get<std::vector<std::string>>()) {
            for (const auto& v : plugin_name::list) {
              if (v == p) {
                plugins.push_back(p);

                break;
              }
            }
          }

        } catch (const nlohmann::json::exception& e) {
          plugins.clear();

          util::warning(log_tag + e.what());
        }

        g_settings_set_strv(sie_settings, "plugins", util::make_gchar_pointer_vector(plugins).data());
      } else {
        util::debug(log_tag + "can't find the preset " + name + " on the filesystem");

        return;
      }

      break;
    }
  }

  load_blocklist(preset_type, json);

  read_plugins_preset(preset_type, plugins, json);

  util::debug(log_tag + "loaded preset: " + input_file.string());
}

void PresetsManager::read_plugins_preset(const PresetType& preset_type,
                                         const std::vector<std::string>& plugins,
                                         const nlohmann::json& json) {
  for (const auto& name : plugins) {
    if (name == plugin_name::autogain) {
      autogain->read(preset_type, json);
    } else if (name == plugin_name::bass_enhancer) {
      bass_enhancer->read(preset_type, json);
    } else if (name == plugin_name::bass_loudness) {
      bass_loudness->read(preset_type, json);
    } else if (name == plugin_name::compressor) {
      compressor->read(preset_type, json);
    } else if (name == plugin_name::convolver) {
      convolver->read(preset_type, json);
    } else if (name == plugin_name::crossfeed) {
      crossfeed->read(preset_type, json);
    } else if (name == plugin_name::crystalizer) {
      crystalizer->read(preset_type, json);
    } else if (name == plugin_name::deesser) {
      deesser->read(preset_type, json);
    } else if (name == plugin_name::delay) {
      delay->read(preset_type, json);
    } else if (name == plugin_name::echo_canceller) {
      echo_canceller->read(preset_type, json);
    } else if (name == plugin_name::equalizer) {
      equalizer->read(preset_type, json);
    } else if (name == plugin_name::exciter) {
      exciter->read(preset_type, json);
    } else if (name == plugin_name::filter) {
      filter->read(preset_type, json);
    } else if (name == plugin_name::gate) {
      gate->read(preset_type, json);
    } else if (name == plugin_name::limiter) {
      limiter->read(preset_type, json);
    } else if (name == plugin_name::loudness) {
      loudness->read(preset_type, json);
    } else if (name == plugin_name::maximizer) {
      maximizer->read(preset_type, json);
    } else if (name == plugin_name::multiband_compressor) {
      multiband_compressor->read(preset_type, json);
    } else if (name == plugin_name::multiband_gate) {
      multiband_gate->read(preset_type, json);
    } else if (name == plugin_name::pitch) {
      pitch->read(preset_type, json);
    } else if (name == plugin_name::reverb) {
      reverb->read(preset_type, json);
    } else if (name == plugin_name::rnnoise) {
      rnnoise->read(preset_type, json);
    } else if (name == plugin_name::stereo_tools) {
      stereo_tools->read(preset_type, json);
    }
  }
}

void PresetsManager::import(const PresetType& preset_type, const std::string& file_path) {
  std::filesystem::path p{file_path};

  if (std::filesystem::is_regular_file(p)) {
    if (p.extension().c_str() == json_ext) {
      std::filesystem::path out_path;

      const auto user_dir = (preset_type == PresetType::output) ? user_output_dir : user_input_dir;

      out_path = user_dir / p.filename();

      std::filesystem::copy_file(p, out_path, std::filesystem::copy_options::overwrite_existing);

      util::debug(log_tag + "imported preset to: " + out_path.string());
    }
  } else {
    util::warning(log_tag + p.string() + " is not a file!");
  }
}

void PresetsManager::add_autoload(const PresetType& preset_type,
                                  const std::string& preset_name,
                                  const std::string& device_name,
                                  const std::string& device_description,
                                  const std::string& device_profile) {
  nlohmann::json json;

  std::filesystem::path output_file;

  switch (preset_type) {
    case PresetType::output:
      output_file = autoload_output_dir / std::filesystem::path{device_name + ":" + device_profile + json_ext};
      break;
    case PresetType::input:
      output_file = autoload_input_dir / std::filesystem::path{device_name + ":" + device_profile + json_ext};
      break;
  }

  std::ofstream o(output_file.c_str());

  json["device"] = device_name;
  json["device-description"] = device_description;
  json["device-profile"] = device_profile;
  json["preset-name"] = preset_name;

  o << std::setw(4) << json << std::endl;

  util::debug(log_tag + "added autoload preset file: " + output_file.string());
}

void PresetsManager::remove_autoload(const PresetType& preset_type,
                                     const std::string& preset_name,
                                     const std::string& device_name,
                                     const std::string& device_profile) {
  std::filesystem::path input_file;

  switch (preset_type) {
    case PresetType::output:
      input_file = autoload_output_dir / std::filesystem::path{device_name + ":" + device_profile + json_ext};
      break;
    case PresetType::input:
      input_file = autoload_input_dir / std::filesystem::path{device_name + ":" + device_profile + json_ext};
      break;
  }

  if (std::filesystem::is_regular_file(input_file)) {
    nlohmann::json json;

    std::ifstream is(input_file);

    is >> json;

    if (preset_name == json.value("preset-name", "") && device_profile == json.value("device-profile", "")) {
      std::filesystem::remove(input_file);

      util::debug(log_tag + "removed autoload: " + input_file.string());
    }
  }
}

auto PresetsManager::find_autoload(const PresetType& preset_type,
                                   const std::string& device_name,
                                   const std::string& device_profile) -> std::string {
  std::filesystem::path input_file;

  switch (preset_type) {
    case PresetType::output:
      input_file = autoload_output_dir / std::filesystem::path{device_name + ":" + device_profile + json_ext};
      break;
    case PresetType::input:
      input_file = autoload_input_dir / std::filesystem::path{device_name + ":" + device_profile + json_ext};
      break;
  }

  if (std::filesystem::is_regular_file(input_file)) {
    nlohmann::json json;

    std::ifstream is(input_file);

    is >> json;

    return json.value("preset-name", "");
  }

  return "";
}

void PresetsManager::autoload(const PresetType& preset_type,
                              const std::string& device_name,
                              const std::string& device_profile) {
  auto name = find_autoload(preset_type, device_name, device_profile);

  if (name.empty()) {
    name = find_autoload(preset_type, util::any_device, "");
  }

  if (!name.empty()) {
    util::debug(log_tag + "autoloading preset " + name + " for device " + device_name);

    load_preset_file(preset_type, name);

    switch (preset_type) {
      case PresetType::output:
        g_settings_set_string(settings, "last-used-output-preset", name.c_str());
        break;
      case PresetType::input:
        g_settings_set_string(settings, "last-used-input-preset", name.c_str());
        break;
    }
  }
}

auto PresetsManager::get_autoload_profiles(const PresetType& preset_type) -> std::vector<nlohmann::json> {
  std::filesystem::path autoload_dir;
  std::vector<nlohmann::json> list;

  switch (preset_type) {
    case PresetType::output:
      autoload_dir = autoload_output_dir;

      break;
    case PresetType::input:
      autoload_dir = autoload_input_dir;
      break;
  }

  auto it = std::filesystem::directory_iterator{autoload_dir};

  try {
    while (it != std::filesystem::directory_iterator{}) {
      if (std::filesystem::is_regular_file(it->status())) {
        if (it->path().extension().c_str() == json_ext) {
          nlohmann::json json;

          std::ifstream is(autoload_dir / it->path());

          is >> json;

          list.push_back(json);
        }
      }

      ++it;
    }

    return list;
  } catch (const std::exception& e) {
    util::warning(log_tag + e.what());

    return list;
  }
}

auto PresetsManager::preset_file_exists(const PresetType& preset_type, const std::string& name) -> bool {
  std::filesystem::path input_file;
  std::vector<std::filesystem::path> conf_dirs;

  switch (preset_type) {
    case PresetType::output: {
      conf_dirs.push_back(user_output_dir);

      conf_dirs.insert(conf_dirs.end(), system_output_dir.begin(), system_output_dir.end());

      for (const auto& dir : conf_dirs) {
        input_file = dir / std::filesystem::path{name.c_str() + json_ext};

        if (std::filesystem::exists(input_file)) {
          return true;
        }
      }

      break;
    }
    case PresetType::input: {
      conf_dirs.push_back(user_input_dir);

      conf_dirs.insert(conf_dirs.end(), system_input_dir.begin(), system_input_dir.end());

      for (const auto& dir : conf_dirs) {
        input_file = dir / std::filesystem::path{name.c_str() + json_ext};

        if (std::filesystem::exists(input_file)) {
          return true;
        }
      }

      break;
    }
  }

  return false;
}
