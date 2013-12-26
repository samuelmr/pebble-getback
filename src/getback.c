#include <pebble.h>

static Window *window;
static TextLayer *dist_layer;
static TextLayer *unit_layer;
static Layer *head_layer;
int32_t distance = 0;
int16_t heading = 0;
static const uint32_t CMD_KEY = 0x1;
static const uint32_t HEAD_KEY = 0x2;
static const uint32_t DIST_KEY = 0x3;
static const char *set_cmd = "set";
static GPath *head_path;

const GPathInfo HEAD_PATH_POINTS = {
  3,
  (GPoint []) {
    {0, 0},
    {-8, -80}, // 80 = radius + fudge; 8 = 80*tan(6 degrees); 6 degrees per minute;
    {8,  -80},
  }
};

static void click_handler(ClickRecognizerRef recognizer, void *context) {
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

void out_sent_handler(DictionaryIterator *sent, void *context) {
   // outgoing message was delivered
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message accepted by phone!");
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
   // outgoing message failed
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message rejected by phone: %d", reason);
}

void in_received_handler(DictionaryIterator *iter, void *context) {
  // incoming message received
  // Check for fields you expect to receive
  Tuple *head_tuple = dict_find(iter, HEAD_KEY);
  if (head_tuple) {
    heading = head_tuple->value->int16;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Updated heading to %d", (int) heading);
    layer_mark_dirty(head_layer);
  }
  Tuple *dist_tuple = dict_find(iter, DIST_KEY);
  static char unit_text[3] = "m";
  if (dist_tuple) {
    distance = dist_tuple->value->int32;
    if (distance > 2900) {
      distance = (int) (distance / 1000);
      strcpy(unit_text, "km");
    }
  }
  static char dist_text[9];
  snprintf(dist_text, sizeof(dist_text), "%d", (int) distance);
  text_layer_set_text(dist_layer, dist_text);
  text_layer_set_text(unit_layer, unit_text);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Distance updated: %d %s", (int) distance, unit_text);
}

static void head_layer_update_callback(Layer *layer, GContext *ctx) {
  gpath_rotate_to(head_path, (TRIG_MAX_ANGLE / 360) * heading);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Rotated heading layer by %d degrees", heading);
  GRect l_bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&l_bounds);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, 77);
  graphics_context_set_fill_color(ctx, GColorWhite);
  gpath_draw_filled(ctx, head_path);
  graphics_fill_circle(ctx, center, 52);
}

void in_dropped_handler(AppMessageResult reason, void *context) {
   // incoming message dropped
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Could not handle message from watch: %d", reason);
}
 
static void click_config_provider(void *context) {
/*
  window_single_click_subscribe(BUTTON_ID_SELECT, click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, click_handler);
*/
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, click_handler, NULL);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);

  head_layer = layer_create(bounds);
  layer_set_update_proc(head_layer, head_layer_update_callback);
  layer_add_child(window_layer, head_layer);
  head_path = gpath_create(&HEAD_PATH_POINTS);
  gpath_move_to(head_path, grect_center_point(&bounds));

  dist_layer = text_layer_create(GRect(27, 50, 90, 40));
  text_layer_set_font(dist_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
  text_layer_set_text_alignment(dist_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(dist_layer));
  text_layer_set_text(dist_layer, "0");
  
  unit_layer = text_layer_create(GRect(50, 84, 44, 30));
  text_layer_set_font(unit_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(unit_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(unit_layer));
  text_layer_set_text(unit_layer, "m");
}

static void window_unload(Window *window) {
  text_layer_destroy(unit_layer);
  text_layer_destroy(dist_layer);
  layer_destroy(head_layer);
}

static void init(void) {
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);
  const uint32_t inbound_size = 128;
  const uint32_t outbound_size = 128;
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
  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
