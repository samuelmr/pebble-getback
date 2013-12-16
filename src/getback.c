#include <pebble.h>

static Window *window;
static TextLayer *text_layer;
double latitude_now;
double longitude_now;
double latitude_stored;
double longitude_stored;
int accuracy;
int distance;
int heading;
double divider = 1000000;

static const uint32_t LAT1_KEY = 1;
static const uint32_t LON1_KEY = 2;
static const uint32_t LAT2_KEY = 3;
static const uint32_t LON2_KEY = 4;
static const uint32_t HEAD_KEY = 5;
static const uint32_t DIST_KEY = 6;

void ftoa(char* str, double val, int precision) {
  //  start with positive/negative
  if (val < 0) {
    *(str++) = '-';
    val = -val;
  }
  //  integer value
  snprintf(str, 12, "%d", (int) val);
  str += strlen(str);
  val -= (int) val;
  //  decimals
  if ((precision > 0) && (val >= .00001)) {
    //  add period
    *(str++) = '.';
    //  loop through precision
    for (int i = 0;  i < precision;  i++)
      if (val > 0) {
        val *= 10;
        *(str++) = '0' + (int) (val + ((i == precision - 1) ? .5 : 0));
        val -= (int) val;
      } else
        break;
  }
  //  terminate
  *str = '\0';
}

void show_current_position(void) {
  text_layer_set_text(text_layer, "Dist updated!");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Body text updated");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Position: %d, %d", (int) latitude_now, (int) longitude_now);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Heading: %d, Distance: %d", heading, distance);
}

void store_current_position(void) {
  latitude_stored = latitude_now;
  longitude_stored = longitude_now;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Stored: %d, %d", (int) latitude_stored, (int) longitude_stored);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
/*
  double lat1 = 60.1;
  double lon1 = 23.1;
  double lat2 = 50.2;
  double lon2 = 25.6;
  int angle = 0;
  double sine = sin(angle);
  sine = sin(lat1)*sin(lat2) + cos(lat1)*cos(lat2) * cos(lon2-lon1);
  double test = acos(sine);
  char body_text[20];
  snprintf(body_text, 20, "%d (%d) %d", (int) test, angle, (int) sine);
  text_layer_set_text(text_layer, body_text);
*/
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  store_current_position();
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    return;
  }
  dict_write_int32(iter, LAT2_KEY, latitude_now * divider);
  dict_write_int32(iter, LON2_KEY, longitude_now * divider);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Built message for phone!");
  app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Send stored position to phone!");
  // show_current_position();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  show_current_position();
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
  Tuple *lat1_tuple = dict_find(iter, LAT1_KEY);
  latitude_now = lat1_tuple->value->int32/divider;
  Tuple *lon1_tuple = dict_find(iter, LON1_KEY);
  longitude_now = lon1_tuple->value->int32/divider;
  Tuple *head_tuple = dict_find(iter, HEAD_KEY);
  if (head_tuple) {
    heading = head_tuple->value->int8;
  }
  Tuple *dist_tuple = dict_find(iter, DIST_KEY);
  if (dist_tuple) {
    distance = dist_tuple->value->int8;
  }
  show_current_position();
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
  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD	));
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
  text_layer_set_text(text_layer, "Press a button");
/*
  show_current_position();
*/
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
}

static void init(void) {
  latitude_stored = persist_exists(LAT2_KEY) ? persist_read_int(LAT2_KEY) : 0;
  longitude_stored = persist_exists(LON2_KEY) ? persist_read_int(LON2_KEY) : 0;
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
  persist_write_int(LAT2_KEY, latitude_stored);
  persist_write_int(LON2_KEY, longitude_stored);
  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
