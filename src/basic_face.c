#include <pebble.h>

// the main app window
static Window *window;

// info bar images
static GBitmap *g_imgCharging = NULL;

// info bar data
static int g_iBatteryCharging = 0;

// views
#define MAIN_VIEW 0
#define TIMER_VIEW 1
#define ANALOG_VIEW 2
#define NUM_VIEWS 3
static int g_iView = MAIN_VIEW;

// main view layers
static TextLayer *g_pTimeLayer = NULL;
static TextLayer *g_pDateLayer = NULL;

// data for display of main view
static char g_szTime[16] = { 0 };
static char g_szDate[16] = { 0 };

// timer view layers
static TextLayer *g_pTimerLayer = NULL;

// data for display of timer view
#define TIMER_RESET 0
#define TIMER_RUNNING 1
#define TIMER_IDLE 2
static int g_iTimerState = TIMER_RESET;
static time_t g_tTimerStart = 0; 
static char g_szTimer[16] = { 0 };

// used to hide previous view when switching between views
static void close_view() {
  switch (g_iView) {
  case MAIN_VIEW:
    layer_set_hidden(text_layer_get_layer(g_pTimeLayer), true);
    layer_set_hidden(text_layer_get_layer(g_pDateLayer), true);
    break;
  case TIMER_VIEW:
    layer_set_hidden(text_layer_get_layer(g_pTimerLayer), true);
    break;
  }
}

// used to show new view when switching between views
static void open_view() {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Switching to view: %d", g_iView);

  switch (g_iView) {
  case MAIN_VIEW:
    layer_set_hidden(text_layer_get_layer(g_pTimeLayer), false);
    layer_set_hidden(text_layer_get_layer(g_pDateLayer), false);
    break;
  case TIMER_VIEW:
    layer_set_hidden(text_layer_get_layer(g_pTimerLayer), false);
    break;
  }
}

// update timer view when timer is visible and running
static void update_timer() {
  if ((g_iView == TIMER_VIEW) && (g_iTimerState == TIMER_RUNNING)) {
    time_t tTimerEnd;
    time(&tTimerEnd);

    const double dTimerSeconds = difftime(tTimerEnd, g_tTimerStart);

    const int iHours = dTimerSeconds / (60 * 60);
    const int iMinutes = (dTimerSeconds / 60) - (iHours * 60);
    const int iSeconds = dTimerSeconds - (iHours * 60 * 60) - (iMinutes * 60);

    snprintf(g_szTimer, 16, "%02d:%02d:%02d", iHours, iMinutes, iSeconds);

    text_layer_set_text(g_pTimerLayer, g_szTimer);
  }
}

// toggle between timer states
static void timer_toggle() {
  switch (g_iTimerState) {
  case TIMER_RESET:
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Timer Started");
    time(&g_tTimerStart);
    g_iTimerState = TIMER_RUNNING;
    break;
  case TIMER_RUNNING:
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Timer Stopped");
    update_timer();
    g_iTimerState = TIMER_IDLE;
    break;
  case TIMER_IDLE:
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Timer Reset");
    text_layer_set_text(g_pTimerLayer, "00:00:00");
    g_iTimerState = TIMER_RESET;
    break;
  }
}

// handler for select button
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (g_iView == TIMER_VIEW)
    timer_toggle();
}

// handler for up button
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  close_view();

  g_iView--;
  if (g_iView < 0)
    g_iView = NUM_VIEWS - 1;

  open_view();
}

// handler for down button
static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  close_view();

  g_iView++;
  if (g_iView >= NUM_VIEWS)
    g_iView = 0;

  open_view();
}

// setup button handlers
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

// update time display
static void update_time(struct tm *tick_time) {
  // update time
  if (clock_is_24h_style())
    strftime(g_szTime, 16, "%H:%M:%S", tick_time);
  else
    strftime(g_szTime, 16, "%I:%M:%S %p", tick_time);

  text_layer_set_text(g_pTimeLayer, g_szTime);

  // update date
  strftime(g_szDate, 16, "%B %d", tick_time);

  text_layer_set_text(g_pDateLayer, g_szDate);
}

// called from time service
static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
  update_timer();
}

// called from the battery charge state service
static void handle_battery_state(BatteryChargeState charge_state) {
  g_iBatteryCharging = charge_state.is_charging;

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Battery State Change.  Charging: %d", g_iBatteryCharging);

}

// draws info bar items
static void draw_info_bar(Layer* layer, GContext* ctx) {
  if (g_iBatteryCharging) {
    GRect bounds = layer_get_bounds(layer);
    graphics_draw_bitmap_in_rect(ctx, g_imgCharging, (GRect) { .origin = { bounds.size.w - 42, 0 }, .size = { 42, 28 } });
  }
}

// draws background of main view
static void draw_main_clock(Layer* layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);

  graphics_fill_rect(ctx, 
		     (GRect) { 
		       .origin = { 0, 54}, 
			 .size = { bounds.size.w , 2} 
		     }, 
		     0, GCornersAll);

  graphics_fill_rect(ctx, 
		     (GRect) { 
		       .origin = { 0, 118}, 
			 .size = { bounds.size.w , 2} 
		     }, 
		     0, GCornersAll);
}

// helper functions to translate between polar and cartesian coordinates
static uint16_t polar_to_x(int32_t theta, uint16_t r) {
  return r * sin_lookup(theta) / TRIG_MAX_RATIO;
}

static uint16_t polar_to_y(int32_t theta, uint16_t r) {
  return r * -cos_lookup(theta) / TRIG_MAX_RATIO;
}

// remap values that assume (0,0) is the center of the screen
static uint16_t remap_x_to_origin(GRect* pBounds, uint16_t x) {
  return (pBounds->size.w / 2) + x;
}

static uint16_t remap_y_to_origin(GRect* pBounds, uint16_t y) {
  return (pBounds->size.h / 2) + y;
}

// draw analog clock
static void draw_analog_clock(Layer* layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);

  // draw hours
  const uint16_t r_base = (bounds.size.w / 2) - 8;
  for (int i = 0; i < 12; ++i) {
    const int32_t theta = TRIG_MAX_ANGLE * (i / 12.0);

    const uint16_t px = remap_x_to_origin(&bounds, polar_to_x(theta, r_base));
    const uint16_t py = remap_y_to_origin(&bounds, polar_to_y(theta, r_base));

    graphics_fill_circle(ctx, (GPoint) { .x = px, .y = py }, 4);
  }

  // remap origin
  const uint16_t ox = remap_x_to_origin(&bounds, 0);
  const uint16_t oy = remap_y_to_origin(&bounds, 0);

  // draw center point
  graphics_fill_circle(ctx, (GPoint) { .x = ox, .y = oy }, 1);

  // current time
  time_t tt;
  time(&tt);
  struct tm* curr_time = localtime(&tt);

  // draw hour hand
  const int32_t hour_theta = TRIG_MAX_ANGLE * (((curr_time->tm_hour % 12) * 60) + curr_time->tm_min) / (12 * 60);
  const uint16_t hr = (bounds.size.w / 4) - 4;
  const uint16_t hx = remap_x_to_origin(&bounds, polar_to_x(hour_theta, hr));
  const uint16_t hy = remap_y_to_origin(&bounds, polar_to_y(hour_theta, hr));

  graphics_draw_line(ctx, 
		     (GPoint) { .x = ox, .y = oy }, 
		     (GPoint) { .x = hx, .y = hy });

  // draw minute hand
  const int32_t minute_theta = TRIG_MAX_ANGLE * curr_time->tm_min / 60;
  const uint16_t mr = (bounds.size.w / 2) - 16;
  const uint16_t mx = remap_x_to_origin(&bounds, polar_to_x(minute_theta, mr));
  const uint16_t my = remap_y_to_origin(&bounds, polar_to_y(minute_theta, mr));

  graphics_draw_line(ctx, 
		     (GPoint) { .x = ox, .y = oy }, 
		     (GPoint) { .x = mx, .y = my });
}

// layer drawing handler
static void face_draw(Layer* layer, GContext* ctx) {
  draw_info_bar(layer, ctx);

  switch (g_iView) {
  case MAIN_VIEW:
    draw_main_clock(layer, ctx);
    break;
  case ANALOG_VIEW:
    draw_analog_clock(layer, ctx);
    break;
  }
}

// called when may app window is loaded
static void window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Window Load");

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  layer_set_update_proc(window_layer, face_draw);

  // main view setup 

  g_pTimeLayer = text_layer_create((GRect) { 
      .origin = { 0, 62 }, 
	.size = { bounds.size.w, 30 } 
    });
  text_layer_set_background_color(g_pTimeLayer, GColorBlack);
  text_layer_set_text_color(g_pTimeLayer, GColorWhite);
  text_layer_set_text(g_pTimeLayer, "Time");
  text_layer_set_text_alignment(g_pTimeLayer, GTextAlignmentCenter);
  text_layer_set_font(g_pTimeLayer, 
		      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(g_pTimeLayer));

  g_pDateLayer = text_layer_create((GRect) { 
      .origin = { 0, 94 }, 
	.size = { bounds.size.w, 18 } 
    });
  text_layer_set_text_alignment(g_pDateLayer, GTextAlignmentCenter);
  text_layer_set_background_color(g_pDateLayer, GColorBlack);
  text_layer_set_text_color(g_pDateLayer, GColorWhite);
  text_layer_set_text(g_pDateLayer, "The Date");
  layer_add_child(window_layer, text_layer_get_layer(g_pDateLayer));

  // timer view setup
  g_pTimerLayer = text_layer_create((GRect) { 
      .origin = { 0, 72 }, 
	.size = { bounds.size.w, 30 } 
    });
  text_layer_set_background_color(g_pTimerLayer, GColorBlack);
  text_layer_set_text_color(g_pTimerLayer, GColorWhite);
  text_layer_set_text(g_pTimerLayer, "00:00:00");
  text_layer_set_text_alignment(g_pTimerLayer, GTextAlignmentCenter);
  text_layer_set_font(g_pTimerLayer, 
		      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(g_pTimerLayer));
  layer_set_hidden(text_layer_get_layer(g_pTimerLayer), true);
  
  // start getting updates on second tick
  tick_timer_service_subscribe(SECOND_UNIT, handle_tick);

  // start getting updates on battery start
  battery_state_service_subscribe(handle_battery_state);
}

// called when app winow is hidden
static void window_unload(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Window Unload");

  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();

  // clean up all layers
  text_layer_destroy(g_pTimeLayer);
  text_layer_destroy(g_pDateLayer);
  text_layer_destroy(g_pTimerLayer);
}

// called when app is created
static void init(void) {
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);

  g_imgCharging = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING);
}

// called when app is destroyed
static void deinit(void) {
  window_destroy(window);
}

// main function
int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();

  return 0;
}
