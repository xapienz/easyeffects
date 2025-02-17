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

#include "bass_loudness_ui.hpp"

namespace ui::bass_loudness_box {

using namespace std::string_literals;

auto constexpr log_tag = "bass_loudness_box: ";

struct Data {
 public:
  ~Data() { util::debug(log_tag + "data struct destroyed"s); }

  std::shared_ptr<BassLoudness> bass_loudness;

  std::vector<sigc::connection> connections;

  std::vector<gulong> gconnections;
};

struct _BassLoudnessBox {
  GtkBox parent_instance;

  GtkScale *input_gain, *output_gain;

  GtkLevelBar *input_level_left, *input_level_right, *output_level_left, *output_level_right;

  GtkLabel *input_level_left_label, *input_level_right_label, *output_level_left_label, *output_level_right_label;

  GtkToggleButton* bypass;

  GtkSpinButton *loudness, *output, *link;

  GSettings* settings;

  Data* data;
};

G_DEFINE_TYPE(BassLoudnessBox, bass_loudness_box, GTK_TYPE_BOX)

void on_bypass(BassLoudnessBox* self, GtkToggleButton* btn) {
  self->data->bass_loudness->bypass = gtk_toggle_button_get_active(btn);
}

void on_reset(BassLoudnessBox* self, GtkButton* btn) {
  gtk_toggle_button_set_active(self->bypass, 0);

  util::reset_all_keys(self->settings);
}

void setup(BassLoudnessBox* self, std::shared_ptr<BassLoudness> bass_loudness, const std::string& schema_path) {
  self->data->bass_loudness = bass_loudness;

  self->settings = g_settings_new_with_path("com.github.wwmm.easyeffects.bassloudness", schema_path.c_str());

  bass_loudness->post_messages = true;
  bass_loudness->bypass = false;

  self->data->connections.push_back(bass_loudness->input_level.connect([=](const float& left, const float& right) {
    update_level(self->input_level_left, self->input_level_left_label, self->input_level_right,
                 self->input_level_right_label, left, right);
  }));

  self->data->connections.push_back(bass_loudness->output_level.connect([=](const float& left, const float& right) {
    update_level(self->output_level_left, self->output_level_left_label, self->output_level_right,
                 self->output_level_right_label, left, right);
  }));

  gsettings_bind_widgets<"input-gain", "output-gain">(self->settings, self->input_gain, self->output_gain);

  gsettings_bind_widgets<"loudness", "output", "link">(self->settings, self->loudness, self->output, self->link);
}

void dispose(GObject* object) {
  auto* self = EE_BASS_LOUDNESS_BOX(object);

  self->data->bass_loudness->bypass = false;

  for (auto& c : self->data->connections) {
    c.disconnect();
  }

  for (auto& handler_id : self->data->gconnections) {
    g_signal_handler_disconnect(self->settings, handler_id);
  }

  self->data->connections.clear();
  self->data->gconnections.clear();

  g_object_unref(self->settings);

  util::debug(log_tag + "disposed"s);

  G_OBJECT_CLASS(bass_loudness_box_parent_class)->dispose(object);
}

void finalize(GObject* object) {
  auto* self = EE_BASS_LOUDNESS_BOX(object);

  delete self->data;

  util::debug(log_tag + "finalize"s);

  G_OBJECT_CLASS(bass_loudness_box_parent_class)->finalize(object);
}

void bass_loudness_box_class_init(BassLoudnessBoxClass* klass) {
  auto* object_class = G_OBJECT_CLASS(klass);
  auto* widget_class = GTK_WIDGET_CLASS(klass);

  object_class->dispose = dispose;
  object_class->finalize = finalize;

  gtk_widget_class_set_template_from_resource(widget_class, "/com/github/wwmm/easyeffects/ui/bass_loudness.ui");

  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, input_gain);
  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, output_gain);
  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, input_level_left);
  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, input_level_right);
  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, output_level_left);
  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, output_level_right);
  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, input_level_left_label);
  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, input_level_right_label);
  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, output_level_left_label);
  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, output_level_right_label);

  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, bypass);

  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, loudness);
  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, output);
  gtk_widget_class_bind_template_child(widget_class, BassLoudnessBox, link);

  gtk_widget_class_bind_template_callback(widget_class, on_bypass);
  gtk_widget_class_bind_template_callback(widget_class, on_reset);
}

void bass_loudness_box_init(BassLoudnessBox* self) {
  gtk_widget_init_template(GTK_WIDGET(self));

  self->data = new Data();

  prepare_spinbuttons<"dB">(self->loudness, self->output, self->link);

  prepare_scales<"dB">(self->input_gain, self->output_gain);
}

auto create() -> BassLoudnessBox* {
  return static_cast<BassLoudnessBox*>(g_object_new(EE_TYPE_BASS_LOUDNESS_BOX, nullptr));
}

}  // namespace ui::bass_loudness_box
