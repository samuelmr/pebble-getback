#include <pebble.h>

static Window *window;
static TextLayer *text_layer;
char distance[9] = "";
char heading[9] = "";
static const uint32_t CMD_KEY = 1;
static const uint32_t HEAD_KEY = 2;
static const uint32_t DIST_KEY = 3;
static const char *set_cmd = "set";
static const char *clear_cmd = "clear";

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Tuplet cmd_tuple = TupletCString(CMD_KEY, set_cmd);
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    return;
  }
  dict_write_int32(iter, HEAD_KEY, 8);
  dict_write_cstring(iter, CMD_KEY, set_cmd);
  // dict_write_tuplet(iter, &cmd_tuple);
  app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent command '%s'to phone!", set_cmd);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Tuplet cmd_tuple = TupletCString(CMD_KEY, clear_cmd);
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    return;
  }
  dict_write_cstring(iter, CMD_KEY, clear_cmd);
  // dict_write_tuplet(iter, &cmd_tuple);
  app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent command '%s'to phone!", clear_cmd);
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
    strcpy(heading, head_tuple->value->cstring);
  }
  Tuple *dist_tuple = dict_find(iter, DIST_KEY);
  if (dist_tuple) {
    strcpy(distance, dist_tuple->value->cstring);
  }
  static char body_text[9];
  snprintf(body_text, sizeof(body_text), "%s\n%s", heading, distance);
  text_layer_set_text(text_layer, body_text);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Body text updated");
}

void in_dropped_handler(AppMessageResult reason, void *context) {
   // incoming message dropped
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Could not handle message from watch: %d", reason);
}
 
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  text_layer = text_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
  text_layer_set_text(text_layer, "UP: set\nDOWN: clear.");
  // text_layer_set_text(text_layer, "HELLO!");
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
}

static void init(void) {
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);
  const uint32_t inbound_size = 128;
  const uint32_t outbound_size = 8;
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
