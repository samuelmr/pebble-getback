#include <pebble.h>

static Window *window;
static TextLayer *dist_layer;
static TextLayer *unit_layer;
static TextLayer *hint_layer;
static Layer *head_layer;
static TextLayer *n_layer;
static TextLayer *e_layer;
static TextLayer *s_layer;
static TextLayer *w_layer;
static TextLayer *calib_layer;
int32_t distance = 0;
int16_t heading = 0;
int16_t orientation = 0;
static const uint32_t CMD_KEY = 0x1;
static const uint32_t HEAD_KEY = 0x2;
static const uint32_t DIST_KEY = 0x3;
static const uint32_t UNITS_KEY = 0x4;
static const char *set_cmd = "set";
static const char *quit_cmd = "quit";
static GPath *head_path;
static GRect hint_layer_size;
static const double YARD_LENGTH = 0.9144;
static const double YARDS_IN_MILE = 1760;
GPoint center;

const GPathInfo HEAD_PATH_POINTS = {
  3,
  (GPoint []) {
    {0, -77},
    {-11, -48},
    {11,  -48},
  }
};

static void reset_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Reset");
  if (hint_layer) {
    text_layer_destroy(hint_layer);
    hint_layer = NULL;
  }
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Can not send command to phone!");
    return;
  }
  dict_write_cstring(iter, CMD_KEY, set_cmd);
  const uint32_t final_size = dict_write_end(iter);
  app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent command '%s' to phone! (%d bytes)", set_cmd, (int) final_size);
}

static void hint_handler(ClickRecognizerRef recognizer, void *context) {
  if (hint_layer) {
    text_layer_destroy(hint_layer);
    hint_layer = NULL;
  }
  else {
    Layer *window_layer = window_get_root_layer(window);
    hint_layer = text_layer_create(hint_layer_size);
    text_layer_set_font(hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(hint_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(hint_layer));
    text_layer_set_text(hint_layer, "Press and hold to set target to current position.");
  }  
}  

void out_sent_handler(DictionaryIterator *sent, void *context) {
   // outgoing message was delivered
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message accepted by phone!");
  Layer *window_layer = window_get_root_layer(window);
  hint_layer = text_layer_create(hint_layer_size);
  text_layer_set_font(hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(hint_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(hint_layer));
  text_layer_set_text(hint_layer, "Target set. Press again to continue.");
  vibes_short_pulse();
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
   // outgoing message failed
  APP_LOG(APP_LOG_LEVEL_WARNING, "Message rejected by phone: %d", reason);
  if (reason == APP_MSG_SEND_TIMEOUT) {
    Layer *window_layer = window_get_root_layer(window);
    hint_layer = text_layer_create(hint_layer_size);
    text_layer_set_font(hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(hint_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(hint_layer));
    text_layer_set_text(hint_layer, "Cannot set target. Restart the app.");
  }
}

void in_received_handler(DictionaryIterator *iter, void *context) {
  // incoming message received
  static char units[9];
  Tuple *head_tuple = dict_find(iter, HEAD_KEY);
  if (head_tuple) {
    heading = head_tuple->value->int16;
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "Updated heading to %d", (int) heading);
    layer_mark_dirty(head_layer);
  }
  Tuple *units_tuple = dict_find(iter, UNITS_KEY);
  if (units_tuple) {
    strcpy(units, units_tuple->value->cstring);
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "Units: %s", units);
  }
  Tuple *dist_tuple = dict_find(iter, DIST_KEY);
  if (dist_tuple) {
    distance = dist_tuple->value->int32;
    if (strcmp(units, "imperial") == 0) {
      text_layer_set_text(unit_layer, "yd");
      distance = distance / YARD_LENGTH;
    }
    else {
      text_layer_set_text(unit_layer, "m");
    }
    if (distance > 2900) {
      if (strcmp(units, "imperial") == 0) {
        distance = (int) (distance / YARDS_IN_MILE);
        text_layer_set_text(unit_layer, "mi");
      }
      else {
        distance = (int) (distance / 1000);
        text_layer_set_text(unit_layer, "km");
      }
    }
  }
  static char dist_text[9];
  snprintf(dist_text, sizeof(dist_text), "%d", (int) distance);
  text_layer_set_text(dist_layer, dist_text);
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Distance updated: %d %s", (int) distance, text_layer_get_text(unit_layer));
}

void compass_heading_handler(CompassHeadingData heading_data){
  static char valid_buf[2];
  switch (heading_data.compass_status) {
    case CompassStatusDataInvalid:
      snprintf(valid_buf, sizeof(valid_buf), "%s", "C");
      return;
    case CompassStatusCalibrating:
      snprintf(valid_buf, sizeof(valid_buf), "%s", "F");
      break;
    case CompassStatusCalibrated:
      snprintf(valid_buf, sizeof(valid_buf), "%s", "");
  }
  orientation = heading_data.true_heading;
  layer_mark_dirty(head_layer);
  text_layer_set_text(calib_layer, valid_buf);
  int32_t nx = center.x + 63 * sin_lookup(orientation)/TRIG_MAX_RATIO;
  int32_t ny = center.y - 63 * cos_lookup(orientation)/TRIG_MAX_RATIO;
  layer_set_frame((Layer *) n_layer, GRect(nx - 9, ny - 9, 18, 18));
  int32_t ex = center.x + 63 * sin_lookup(orientation + TRIG_MAX_ANGLE/4)/TRIG_MAX_RATIO;
  int32_t ey = center.y - 63 * cos_lookup(orientation + TRIG_MAX_ANGLE/4)/TRIG_MAX_RATIO;
  layer_set_frame((Layer *) e_layer, GRect(ex - 9, ey - 9, 18, 18));
  int32_t sx = center.x + 63 * sin_lookup(orientation + TRIG_MAX_ANGLE/2)/TRIG_MAX_RATIO;
  int32_t sy = center.y - 63 * cos_lookup(orientation + TRIG_MAX_ANGLE/2)/TRIG_MAX_RATIO;
  layer_set_frame((Layer *) s_layer, GRect(sx - 9, sy - 9, 18, 18));
  int32_t wx = center.x + 63 * sin_lookup(orientation + TRIG_MAX_ANGLE*3/4)/TRIG_MAX_RATIO;
  int32_t wy = center.y - 63 * cos_lookup(orientation + TRIG_MAX_ANGLE*3/4)/TRIG_MAX_RATIO;
  layer_set_frame((Layer *) w_layer, GRect(wx - 9, wy - 9, 18, 18));
}

static void head_layer_update_callback(Layer *layer, GContext *ctx) {
  gpath_rotate_to(head_path, (TRIG_MAX_ANGLE / 360) * (heading + TRIGANGLE_TO_DEG(orientation)));
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, 77);
  graphics_context_set_fill_color(ctx, GColorWhite);
  gpath_draw_filled(ctx, head_path);
  graphics_fill_circle(ctx, center, 49);
  graphics_context_set_fill_color(ctx, GColorBlack);
}

void in_dropped_handler(AppMessageResult reason, void *context) {
   // incoming message dropped
  APP_LOG(APP_LOG_LEVEL_WARNING, "Could not handle message from watch: %d", reason);
}
 
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, hint_handler);
  window_single_click_subscribe(BUTTON_ID_UP, hint_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, hint_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, reset_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_UP, 0, reset_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 0, reset_handler, NULL);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  center = grect_center_point(&bounds);

  head_layer = layer_create(bounds);
  layer_set_update_proc(head_layer, head_layer_update_callback);
  head_path = gpath_create(&HEAD_PATH_POINTS);
  gpath_move_to(head_path, grect_center_point(&bounds));  
  layer_add_child(window_layer, head_layer);

  n_layer = text_layer_create(GRect(center.x-9, center.y-71, 18, 18));
  text_layer_set_background_color(n_layer, GColorClear);
  text_layer_set_text_color(n_layer, GColorWhite);
  text_layer_set_font(n_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(n_layer, GTextAlignmentCenter);
  text_layer_set_text(n_layer, "N");
  layer_add_child(head_layer, (Layer *) n_layer);

  e_layer = text_layer_create(GRect(center.x+53, center.y-10, 18, 18));
  text_layer_set_background_color(e_layer, GColorClear);
  text_layer_set_text_color(e_layer, GColorWhite);
  text_layer_set_font(e_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(e_layer, GTextAlignmentCenter);
  text_layer_set_text(e_layer, "E");
  layer_add_child(head_layer, (Layer *) e_layer);

  s_layer = text_layer_create(GRect(center.x-9, center.y+57, 18, 18));
  text_layer_set_background_color(s_layer, GColorClear);
  text_layer_set_text_color(s_layer, GColorWhite);
  text_layer_set_font(s_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_layer, GTextAlignmentCenter);
  text_layer_set_text(s_layer, "S");
  layer_add_child(head_layer, (Layer *) s_layer);

  w_layer = text_layer_create(GRect(center.x-71, center.y-9, 18, 18));
  text_layer_set_background_color(w_layer, GColorClear);
  text_layer_set_text_color(w_layer, GColorWhite);
  text_layer_set_font(w_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(w_layer, GTextAlignmentCenter);
  text_layer_set_text(w_layer, "W");
  layer_add_child(head_layer, text_layer_get_layer(w_layer));

  dist_layer = text_layer_create(GRect(29, 50, 86, 40));
  text_layer_set_background_color(dist_layer, GColorClear);
  text_layer_set_font(dist_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
  text_layer_set_text_alignment(dist_layer, GTextAlignmentCenter);
  text_layer_set_text(dist_layer, "");
  layer_add_child(window_layer, text_layer_get_layer(dist_layer));
  
  unit_layer = text_layer_create(GRect(50, 84, 44, 30));
  text_layer_set_background_color(unit_layer, GColorClear);
  text_layer_set_font(unit_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(unit_layer, GTextAlignmentCenter);
  text_layer_set_text(unit_layer, "");
  layer_add_child(window_layer, text_layer_get_layer(unit_layer));

  calib_layer = text_layer_create(GRect(129, 129, 14, 14));
  text_layer_set_background_color(calib_layer, GColorClear);
  text_layer_set_font(calib_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(calib_layer, GTextAlignmentCenter);
  text_layer_set_text(calib_layer, "C");
  layer_add_child(window_layer, text_layer_get_layer(calib_layer));

}

static void window_unload(Window *window) {
  layer_destroy(head_layer);
  text_layer_destroy(calib_layer);
  text_layer_destroy(unit_layer);
  text_layer_destroy(dist_layer);
  text_layer_destroy(w_layer);
  text_layer_destroy(s_layer);
  text_layer_destroy(e_layer);
  text_layer_destroy(n_layer);
  if (hint_layer) {
    text_layer_destroy(hint_layer);
  }
}

static void init(void) {
  // update screen only when heading changes at least 3 degrees
  compass_service_set_heading_filter(TRIG_MAX_ANGLE*3/360);
  compass_service_subscribe(&compass_heading_handler);
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);
  hint_layer_size = GRect(10, 30, 124, 84);
  const uint32_t inbound_size = APP_MESSAGE_INBOX_SIZE_MINIMUM;
  const uint32_t outbound_size = APP_MESSAGE_OUTBOX_SIZE_MINIMUM;
  app_message_open(inbound_size, outbound_size);
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  compass_service_unsubscribe();
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Can not send quit command to phone!");
    return;
  }
  dict_write_cstring(iter, CMD_KEY, quit_cmd);
  const uint32_t final_size = dict_write_end(iter);
  app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent command '%s' to phone! (%d bytes)", quit_cmd, (int) final_size);

  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
