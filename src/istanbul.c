#include <pebble.h>

#define ANTIALIASING true

#define ACTIVE_UNIT MINUTE_UNIT

#define HAND_MARGIN  14
#define DOTS_MARGIN  4

#if defined(PBL_ROUND)
  #define FINAL_RADIUS 90
#else
  #define FINAL_RADIUS 70
#endif

#define COLOR_HAND GColorRed
#define COLOR_DOTS GColorWhite
#define COLOR_ANIM GColorGreen

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer, *s_seconds_layer;
static Animation *s_second_animation, *s_backto_animation;

static GPoint s_center, s_active_point,
              s_dots[360], s_pixels_major[360], s_pixels_minor[360];

static Time s_last_time;

static int s_radius = FINAL_RADIUS,
           s_active_index = 0,
           s_stop_index = 0,
           s_initial = 0;

static char s_date_text[10];
static bool s_animated = false;

static int normalize(AnimationProgress dist_normalized, int min, int max) {

  float diff = (float)(max - min);
  return (int)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * diff + min);

}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  (void)stopped;
  (void)context;
  (void)anim;

  if (stopped && !s_animated) {
    s_stop_index = s_initial;
    animation_schedule(s_backto_animation);
    s_animated = true;
  }
}


static void second_update(Animation *anim, AnimationProgress dist_normalized) {
  (void)anim;

  int index = normalize(dist_normalized, s_stop_index, 359);

  if (!s_animated) {
    index = s_initial + index;
  }

  if (index > 359) {
    index -= 359;
  }
  else if (index == 359) {
    index = 0;
  }

  int shadow = index - 2;

  shadow = (shadow < 0) ? 0 : shadow;

  s_active_point = s_dots[index];
  s_active_index = index;

  layer_mark_dirty(s_seconds_layer);
}

static Animation* animate_second() {

  Animation *anim = animation_create();

  animation_set_duration(anim, 60000);
  animation_set_curve(anim, AnimationCurveLinear);

  AnimationImplementation implementation = {
    .update = second_update
  };

  animation_set_implementation(anim, &implementation);
  animation_set_handlers(anim, (AnimationHandlers) {
    .stopped = animation_stopped
  }, NULL);

  return anim;

}


static void tick_handler(struct tm *tick_time, TimeUnits timeChanged) {
  (void)timeChanged;

  s_last_time.hours = tick_time->tm_hour;
  s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
  s_last_time.minutes = tick_time->tm_min;

  strftime(s_date_text, sizeof(s_date_text), "%a %d", tick_time);

  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }

}

static void update_second(Layer *layer, GContext *ctx) {
  (void)layer;

  graphics_context_set_stroke_width(ctx, 4);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, s_center, s_active_point);

  graphics_context_set_stroke_width(ctx, 2);
  graphics_context_set_stroke_color(ctx, COLOR_HAND);
  graphics_draw_line(ctx, s_center, s_active_point);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, s_center, 5);
  graphics_context_set_fill_color(ctx, COLOR_HAND);
  graphics_fill_circle(ctx, s_center, 3);
}

static void update_canvas(Layer *layer, GContext *ctx) {

  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  for (int i = 0; i < 360; i+=6) {
    if (i % 30 == 0 || i == 0) {
      graphics_context_set_stroke_width(ctx, 2);
      graphics_context_set_stroke_color(ctx, GColorWhite);
    } else {
      graphics_context_set_stroke_width(ctx, 1);
      graphics_context_set_stroke_color(ctx, GColorLightGray);
    }
    if (i > s_initial && i < s_active_index) {
      graphics_context_set_stroke_color(ctx, COLOR_ANIM);
    }
    graphics_draw_line(ctx, s_pixels_minor[i], s_pixels_major[i]);
  }

  if (!s_animated) {
    if (s_initial % 30 == 0) {
      graphics_context_set_stroke_width(ctx, 2);
    }
    graphics_context_set_stroke_color(ctx, COLOR_ANIM);
    graphics_draw_line(ctx, s_pixels_minor[s_initial], s_pixels_major[s_initial]);
  }

  graphics_context_set_stroke_width(ctx, 2);
  // graphics_draw_circle(ctx, s_center, s_radius);

  // GRect // On the right corner
  //   m_frame = GRect(bounds.size.w - 40, s_center.y - 5, 40, 11),
  //   t_frame = GRect(bounds.size.w - 40, s_center.y - 9, 40, 14);

  // GRect // On the right perfectly aligned
  //   m_frame = GRect(s_center.x + 18, s_center.y - 5, 40, 11),
  //   t_frame = GRect(s_center.x + 18, s_center.y - 9, 40, 14);

  GRect // On the bottom center
    m_frame = GRect(s_center.x - 20, s_center.y + 26, 40, 11),
    t_frame = GRect(s_center.x - 20, s_center.y + 22, 40, 14);

  graphics_context_set_text_color(ctx, GColorBlack);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, m_frame, 1, GCornersAll);

  graphics_draw_text(ctx,
    s_date_text,
    fonts_get_system_font(FONT_KEY_GOTHIC_14),
    t_frame,
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL
  );

  Time mode_time = s_last_time;

  // Adjust for minutes through the hour
  float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
  float hour_angle;

  hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
  hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);

  GPoint hour_hand = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle)
       * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle)
       * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.y,
  };

  graphics_context_set_stroke_width(ctx, 4);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, s_center, s_dots[mode_time.minutes * 6]);

  graphics_context_set_stroke_width(ctx, 2);
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, s_center, s_dots[mode_time.minutes * 6]);

  graphics_context_set_stroke_width(ctx, 6);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, s_center, hour_hand);

  graphics_context_set_stroke_width(ctx, 3);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, s_center, hour_hand);

}

static void window_load(Window *window) {

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_center = grect_center_point(&bounds);

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, update_canvas);
  layer_add_child(window_layer, s_canvas_layer);

  s_seconds_layer = layer_create(bounds);
  layer_set_update_proc(s_seconds_layer, update_second);
  layer_add_child(s_canvas_layer, s_seconds_layer);

}

static void window_unload(Window *window) {
  (void)window;

  tick_timer_service_unsubscribe();

  animation_destroy(s_second_animation);
  animation_destroy(s_backto_animation);

  layer_destroy(s_seconds_layer);
  layer_destroy(s_canvas_layer);
}

static void init() {

  time_t t = time(NULL);
  struct tm *time_now = localtime(&t);
  tick_handler(time_now, ACTIVE_UNIT);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(ACTIVE_UNIT, tick_handler);

  for (int i = 0; i < 360; ++i) {
    s_dots[i] = (GPoint) {
      .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * i / 360)
         * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
      .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * i / 360)
         * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
    };
    s_pixels_minor[i] = (GPoint) {
      .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * i / 360)
         * (int32_t)(s_radius - DOTS_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
      .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * i / 360)
         * (int32_t)(s_radius - DOTS_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
    };
    s_pixels_major[i] = (GPoint) {
      .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * i / 360)
         * (int32_t)s_radius / TRIG_MAX_RATIO) + s_center.x,
      .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * i / 360)
         * (int32_t)s_radius / TRIG_MAX_RATIO) + s_center.y,
    };
  }

  s_initial = time_now->tm_sec * 6;
  s_active_index = s_initial;
  s_active_point = s_dots[s_initial];

  s_second_animation = animate_second();
  animation_schedule(s_second_animation);

  s_backto_animation = animation_clone(s_second_animation);
  animation_set_duration(s_backto_animation, 3000);
  animation_set_curve(s_backto_animation, AnimationCurveEaseInOut);

}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}

