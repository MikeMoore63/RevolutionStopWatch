#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0x91, 0x13, 0xA8, 0x37, 0x56, 0x92, 0x4C, 0xCA, 0x97, 0x86, 0xB2, 0x8D, 0x08, 0x1C, 0xDE, 0x35 }
PBL_APP_INFO(MY_UUID,
             "Revolution Stopwatch", "Mike Moore",
             1, 0, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_STANDARD_APP);

// Envisioned as a watchface by Jean-Noël Mattern
// Based on the display of the Freebox Revolution, which was designed by Philippe Starck.

#include "yachtimermodel.h"   // Add yachtimer methods and constants

//************************* Boiler Plate Stop Watch stuff *****************************************
YachtTimer myYachtTimer;
int startappmode=WATCHMODE;
int modetick=0;
int forceall=true;

#define BUTTON_LAP BUTTON_ID_DOWN
#define BUTTON_RUN BUTTON_ID_SELECT
#define BUTTON_RESET BUTTON_ID_UP
#define TIMER_UPDATE 1
#define MODES 5 // Number of watch types stopwatch, coutdown, yachttimer, watch
#define TICKREMOVE 5
#define CNTDWNCFG 99
#define MAX_TIME  (ASECOND * 60 * 60 * 24)

int ticks=0;


BmpContainer modeImages[MODES];

struct modresource {
        int mode;
        int resourceid;
} mapModeImage[MODES] = {
           { WATCHMODE, RESOURCE_ID_IMAGE_WATCH },
           { STOPWATCH, RESOURCE_ID_IMAGE_STOPWATCH },
           { YACHTIMER, RESOURCE_ID_IMAGE_YACHTTIMER },
           { COUNTDOWN, RESOURCE_ID_IMAGE_COUNTDOWN },
           { CNTDWNCFG, RESOURCE_ID_IMAGE_CNTDWNCFG },
};
// allow us to emulate units changed
PblTm theLastTime;

// The documentation claims this is defined, but it is not.
// Define it here for now.
#ifndef APP_TIMER_INVALID_HANDLE
    #define APP_TIMER_INVALID_HANDLE 0xDEADBEEF
#endif

// Actually keeping track of time
static AppTimerHandle update_timer = APP_TIMER_INVALID_HANDLE;
static int ticklen=0;

void toggle_stopwatch_handler(ClickRecognizerRef recognizer, Window *window);
void toggle_mode(ClickRecognizerRef recognizer, Window *window);
void reset_stopwatch_handler(ClickRecognizerRef recognizer, Window *window);
void stop_stopwatch();
void start_stopwatch();
void config_provider(ClickConfig **config, Window *window);
// Hook to ticks
void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie);
void config_watch(int appmode,int increment);
void update_hand_positions();

// Custom vibration pattern
const VibePattern start_pattern = {
  .durations = (uint32_t []) {100, 300, 300, 300, 100, 300},
  .num_segments = 6
};


// Settings
#define USE_AMERICAN_DATE_FORMAT      false
#define TIME_SLOT_ANIMATION_DURATION  500

// Magic numbers
#define SCREEN_WIDTH        144
#define SCREEN_HEIGHT       168

#define TIME_IMAGE_WIDTH    70
#define TIME_IMAGE_HEIGHT   70

#define DATE_IMAGE_WIDTH    20
#define DATE_IMAGE_HEIGHT   20

#define SECOND_IMAGE_WIDTH  10
#define SECOND_IMAGE_HEIGHT 10

#define DAY_IMAGE_WIDTH     20
#define DAY_IMAGE_HEIGHT    10

#define MARGIN              1
#define TIME_SLOT_SPACE     2
#define DATE_PART_SPACE     4


// Images
#define NUMBER_OF_TIME_IMAGES 10
const int TIME_IMAGE_RESOURCE_IDS[NUMBER_OF_TIME_IMAGES] = {
  RESOURCE_ID_IMAGE_TIME_0, 
  RESOURCE_ID_IMAGE_TIME_1, RESOURCE_ID_IMAGE_TIME_2, RESOURCE_ID_IMAGE_TIME_3, 
  RESOURCE_ID_IMAGE_TIME_4, RESOURCE_ID_IMAGE_TIME_5, RESOURCE_ID_IMAGE_TIME_6, 
  RESOURCE_ID_IMAGE_TIME_7, RESOURCE_ID_IMAGE_TIME_8, RESOURCE_ID_IMAGE_TIME_9
};

#define NUMBER_OF_DATE_IMAGES 10
const int DATE_IMAGE_RESOURCE_IDS[NUMBER_OF_DATE_IMAGES] = {
  RESOURCE_ID_IMAGE_DATE_0, 
  RESOURCE_ID_IMAGE_DATE_1, RESOURCE_ID_IMAGE_DATE_2, RESOURCE_ID_IMAGE_DATE_3, 
  RESOURCE_ID_IMAGE_DATE_4, RESOURCE_ID_IMAGE_DATE_5, RESOURCE_ID_IMAGE_DATE_6, 
  RESOURCE_ID_IMAGE_DATE_7, RESOURCE_ID_IMAGE_DATE_8, RESOURCE_ID_IMAGE_DATE_9
};

#define NUMBER_OF_SECOND_IMAGES 10
const int SECOND_IMAGE_RESOURCE_IDS[NUMBER_OF_SECOND_IMAGES] = {
  RESOURCE_ID_IMAGE_SECOND_0, 
  RESOURCE_ID_IMAGE_SECOND_1, RESOURCE_ID_IMAGE_SECOND_2, RESOURCE_ID_IMAGE_SECOND_3, 
  RESOURCE_ID_IMAGE_SECOND_4, RESOURCE_ID_IMAGE_SECOND_5, RESOURCE_ID_IMAGE_SECOND_6, 
  RESOURCE_ID_IMAGE_SECOND_7, RESOURCE_ID_IMAGE_SECOND_8, RESOURCE_ID_IMAGE_SECOND_9
};

#define NUMBER_OF_DAY_IMAGES 7
const int DAY_IMAGE_RESOURCE_IDS[NUMBER_OF_DAY_IMAGES] = {
  RESOURCE_ID_IMAGE_DAY_0, RESOURCE_ID_IMAGE_DAY_1, RESOURCE_ID_IMAGE_DAY_2, 
  RESOURCE_ID_IMAGE_DAY_3, RESOURCE_ID_IMAGE_DAY_4, RESOURCE_ID_IMAGE_DAY_5, 
  RESOURCE_ID_IMAGE_DAY_6
};


// Main
Window window;
//***********************************************************************************************
AppContextRef app;
//***********************************************************************************************

Layer date_container_layer;

#define EMPTY_SLOT -1

typedef struct Slot {
  int           number;
  BmpContainer  image_container;
  int           state;
} Slot;

// Time
typedef struct TimeSlot {
  Slot              slot;
  int               new_state;
  PropertyAnimation slide_out_animation;
  PropertyAnimation slide_in_animation;
  bool              animating;
} TimeSlot;

#define NUMBER_OF_TIME_SLOTS 4
Layer time_layer;
TimeSlot time_slots[NUMBER_OF_TIME_SLOTS];

// Date
#define NUMBER_OF_DATE_SLOTS 4
Layer date_layer;
Slot date_slots[NUMBER_OF_DATE_SLOTS];

// Seconds
#define NUMBER_OF_SECOND_SLOTS 2
Layer seconds_layer;
Slot second_slots[NUMBER_OF_SECOND_SLOTS];

// Day
typedef struct DayItem {
  BmpContainer  image_container;
  Layer         layer;
  bool          loaded;
} DayItem;
DayItem day_item;


// General
BmpContainer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids);
void unload_digit_image_from_slot(Slot *slot);

// Display
void display_time(PblTm *tick_time);
void display_date(PblTm *tick_time);
void display_seconds(PblTm *tick_time);
void display_day(PblTm *tick_time);

// Time
void display_time_value(int value, int row_number);
void update_time_slot(TimeSlot *time_slot, int digit_value);
GRect frame_for_time_slot(TimeSlot *time_slot);
void slide_in_digit_image_into_time_slot(TimeSlot *time_slot, int digit_value);
void time_slot_slide_in_animation_stopped(Animation *slide_in_animation, bool finished, void *context);
void slide_out_digit_image_from_time_slot(TimeSlot *time_slot);
void time_slot_slide_out_animation_stopped(Animation *slide_out_animation, bool finished, void *context);

// Date
void display_date_value(int value, int part_number);
void update_date_slot(Slot *date_slot, int digit_value);

// Seconds
void update_second_slot(Slot *second_slot, int digit_value);

// Handlers
void pbl_main(void *params);
void handle_init(AppContextRef ctx);
void handle_second_tick(AppContextRef ctx, PebbleTickEvent *event);
void handle_deinit(AppContextRef ctx);

//Stop watch handling stuff *********************************************************************

void config_watch(int appmode,int increment)
{
    int adjustnum = 0;

    // even if running allow minute changes
    switch(mapModeImage[modetick].mode)
        {
        // Ok so we want to lower countdown config
        // Down in increments of 1 minute
        case CNTDWNCFG:
                adjustnum=ASECOND * 60;
		ticks=0;
                break;

        }


        /* for non adjust appmodes does nothing as adjustnum is 0 */
        time_t new_time=0;

        /* if running adjust running time otherwise adjust config time */
        if(yachtimer_isRunning(&myYachtTimer))
        {
                new_time =  yachtimer_getElapsed(&myYachtTimer) + (increment * adjustnum );
                if(new_time > MAX_TIME) new_time = yachtimer_getElapsed(&myYachtTimer);
                yachtimer_setElapsed(&myYachtTimer, new_time);
        }
        else
        {
                new_time =  yachtimer_getConfigTime(&myYachtTimer) + (increment * adjustnum );
                // Cannot sert below 0
                // Can set above max display time
                // so keep it displayable
                if(new_time > MAX_TIME) new_time = MAX_TIME;
                yachtimer_setConfigTime(&myYachtTimer, new_time);

        }
    


}
void start_stopwatch() {
    yachtimer_start(&myYachtTimer);

    // default start mode
    startappmode = yachtimer_getMode(&myYachtTimer);;
    update_hand_positions();


}
// Toggle stopwatch timer mode
void toggle_mode(ClickRecognizerRef recognizer, Window *window) {

          // Can only set to first three
          modetick = (modetick == MODES) ?0:(modetick+1);
          yachtimer_setMode(&myYachtTimer,mapModeImage[modetick].mode);

          for (int i=0;i<MODES;i++)
          {
                layer_set_hidden( &modeImages[i].layer.layer, ((mapModeImage[modetick].mode == mapModeImage[i].mode)?false:true));
          }
          ticks = 0;

          update_hand_positions();
}
void stop_stopwatch() {

    yachtimer_stop(&myYachtTimer);
    update_hand_positions();
}
void toggle_stopwatch_handler(ClickRecognizerRef recognizer, Window *window) {
    switch(mapModeImage[modetick].mode)
    {
        case YACHTIMER:
        case STOPWATCH:
        case COUNTDOWN:
            if(yachtimer_isRunning(&myYachtTimer)) {
                stop_stopwatch();
            } else {
                start_stopwatch();
            }
            break;
        default:
            config_watch(mapModeImage[modetick].mode,-1);
    }
    update_hand_positions();
}
void reset_stopwatch_handler(ClickRecognizerRef recognizer, Window *window) {

    switch(mapModeImage[modetick].mode)
    {
        case STOPWATCH:
        case YACHTIMER:
        case COUNTDOWN:
            yachtimer_reset(&myYachtTimer);
            if(yachtimer_isRunning(&myYachtTimer))
            {
                 stop_stopwatch();
                 start_stopwatch();
            }
            else
            {
                stop_stopwatch();
            }

            break;
        default:
            ;
            // if not in config mode won't do anything which makes this easy
            config_watch(mapModeImage[modetick].mode,1);
    }
    // Force redisplay
    update_hand_positions();
}
void update_hand_positions()
{
	handle_timer(app,update_timer,TIMER_UPDATE);
}
//***********************************************************************************************

// General
BmpContainer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids) {
  if (digit_value < 0 || digit_value > 9) {
    return NULL;
  }

  if (slot->state != EMPTY_SLOT) {
    return NULL;
  }

  slot->state = digit_value;

  BmpContainer *image_container = &slot->image_container;

  bmp_init_container(digit_resource_ids[digit_value], image_container);
  layer_set_frame(&image_container->layer.layer, frame);
  layer_add_child(parent_layer, &image_container->layer.layer);

  return image_container;
}

void unload_digit_image_from_slot(Slot *slot) {
  if (slot->state == EMPTY_SLOT) {
    return;
  }

  BmpContainer *image_container = &slot->image_container;

  layer_remove_from_parent(&image_container->layer.layer);
  bmp_deinit_container(image_container);

  slot->state = EMPTY_SLOT;
}

// Display
void display_time(PblTm *tick_time) {
  int hour = tick_time->tm_hour;

  if (!clock_is_24h_style()) {
    hour = hour % 12;
    if (hour == 0) {
      hour = 12;
    }
  }

  display_time_value(hour,              0);
  display_time_value(tick_time->tm_min, 1);
}

void display_date(PblTm *tick_time) {
  int day   = tick_time->tm_mday;
  int month = tick_time->tm_mon + 1;

#if USE_AMERICAN_DATE_FORMAT
  display_date_value(month, 0);
  display_date_value(day,   1);
#else
  display_date_value(day,   0);
  display_date_value(month, 1);
#endif
}

void display_seconds(PblTm *tick_time) {
  int seconds = tick_time->tm_sec;

  seconds = seconds % 100; // Maximum of two digits per row.

  for (int second_slot_number = 1; second_slot_number >= 0; second_slot_number--) {
    Slot *second_slot = &second_slots[second_slot_number];

    update_second_slot(second_slot, seconds % 10);
    
    seconds = seconds / 10;
  }
}

void display_day(PblTm *tick_time) {
  BmpContainer *image_container = &day_item.image_container;

  if (day_item.loaded) {
    layer_remove_from_parent(&image_container->layer.layer);
    bmp_deinit_container(image_container);
  }

  bmp_init_container(DAY_IMAGE_RESOURCE_IDS[tick_time->tm_wday], image_container);
  layer_add_child(&day_item.layer, &image_container->layer.layer);

  day_item.loaded = true;
}

// Time
void display_time_value(int value, int row_number) {
  value = value % 100; // Maximum of two digits per row.

  for (int column_number = 1; column_number >= 0; column_number--) {
    int time_slot_number = (row_number * 2) + column_number;

    TimeSlot *time_slot = &time_slots[time_slot_number];

    update_time_slot(time_slot, value % 10);

    value = value / 10;
  }
}

void update_time_slot(TimeSlot *time_slot, int digit_value) {
  if (time_slot->slot.state == digit_value) {
    return;
  }

  if (time_slot->animating) {
    // Otherwise we'll crash when the animation is replaced by a new animation before we're finished.
    return;
  }

  time_slot->animating = true;

  PropertyAnimation *animation;
  if (time_slot->slot.state == EMPTY_SLOT) {
    slide_in_digit_image_into_time_slot(time_slot, digit_value);
    animation = &time_slot->slide_in_animation;
  }
  else {
    time_slot->new_state = digit_value;

    slide_out_digit_image_from_time_slot(time_slot);
    animation = &time_slot->slide_out_animation;

    animation_set_handlers(&animation->animation, (AnimationHandlers){
      .stopped = (AnimationStoppedHandler)time_slot_slide_out_animation_stopped
    }, (void *)time_slot);
  }

  animation_schedule(&animation->animation);
}

GRect frame_for_time_slot(TimeSlot *time_slot) {
  int x = MARGIN + (time_slot->slot.number % 2) * (TIME_IMAGE_WIDTH + TIME_SLOT_SPACE);
  int y = MARGIN + (time_slot->slot.number / 2) * (TIME_IMAGE_HEIGHT + TIME_SLOT_SPACE);

  return GRect(x, y, TIME_IMAGE_WIDTH, TIME_IMAGE_HEIGHT);
}

void slide_in_digit_image_into_time_slot(TimeSlot *time_slot, int digit_value) {
  GRect to_frame = frame_for_time_slot(time_slot);

  int from_x = to_frame.origin.x;
  int from_y = to_frame.origin.y;
  switch (time_slot->slot.number) {
    case 0:
      from_x -= TIME_IMAGE_WIDTH + MARGIN;
      break;
    case 1:
      from_y -= TIME_IMAGE_HEIGHT + MARGIN;
      break;
    case 2:
      from_y += TIME_IMAGE_HEIGHT + MARGIN;
      break;
    case 3:
      from_x += TIME_IMAGE_WIDTH + MARGIN;
      break;
  }
  GRect from_frame = GRect(from_x, from_y, TIME_IMAGE_WIDTH, TIME_IMAGE_HEIGHT);

  BmpContainer *image_container = load_digit_image_into_slot(&time_slot->slot, digit_value, &time_layer, from_frame, TIME_IMAGE_RESOURCE_IDS);

  PropertyAnimation *animation = &time_slot->slide_in_animation;
  property_animation_init_layer_frame(animation, &image_container->layer.layer, &from_frame, &to_frame);
  animation_set_duration( &animation->animation, TIME_SLOT_ANIMATION_DURATION);
  animation_set_curve(    &animation->animation, AnimationCurveLinear);
  animation_set_handlers( &animation->animation, (AnimationHandlers){
    .stopped = (AnimationStoppedHandler)time_slot_slide_in_animation_stopped
  }, (void *)time_slot);
}

void time_slot_slide_in_animation_stopped(Animation *slide_in_animation, bool finished, void *context) {
  TimeSlot *time_slot = (TimeSlot *)context;

  time_slot->animating = false;
}

void slide_out_digit_image_from_time_slot(TimeSlot *time_slot) {
  GRect from_frame = frame_for_time_slot(time_slot);

  int to_x = from_frame.origin.x;
  int to_y = from_frame.origin.y;
  switch (time_slot->slot.number) {
    case 0:
      to_y -= TIME_IMAGE_HEIGHT + MARGIN;
      break;
    case 1:
      to_x += TIME_IMAGE_WIDTH + MARGIN;
      break;
    case 2:
      to_x -= TIME_IMAGE_WIDTH + MARGIN;
      break;
    case 3:
      to_y += TIME_IMAGE_HEIGHT + MARGIN;
      break;
  }
  GRect to_frame = GRect(to_x, to_y, TIME_IMAGE_WIDTH, TIME_IMAGE_HEIGHT);

  BmpContainer *image_container = &time_slot->slot.image_container;

  PropertyAnimation *animation = &time_slot->slide_out_animation;
  property_animation_init_layer_frame(animation, &image_container->layer.layer, &from_frame, &to_frame);
  animation_set_duration( &animation->animation, TIME_SLOT_ANIMATION_DURATION);
  animation_set_curve(    &animation->animation, AnimationCurveLinear);

  // Make sure to unload the digit image from the slot when the animation has finished!
}

void time_slot_slide_out_animation_stopped(Animation *slide_out_animation, bool finished, void *context) {
  TimeSlot *time_slot = (TimeSlot *)context;

  unload_digit_image_from_slot(&time_slot->slot);

  slide_in_digit_image_into_time_slot(time_slot, time_slot->new_state);
  animation_schedule(&time_slot->slide_in_animation.animation);

  time_slot->new_state = EMPTY_SLOT;
}

// Date
void display_date_value(int value, int part_number) {
  value = value % 100; // Maximum of two digits per row.

  for (int column_number = 1; column_number >= 0; column_number--) {
    int date_slot_number = (part_number * 2) + column_number;

    Slot *date_slot = &date_slots[date_slot_number];

    update_date_slot(date_slot, value % 10);

    value = value / 10;
  }
}

void update_date_slot(Slot *date_slot, int digit_value) {
  if (date_slot->state == digit_value) {
    return;
  }

  int x = date_slot->number * (DATE_IMAGE_WIDTH + MARGIN);
  if (date_slot->number >= 2) {
    x += 3; // 3 extra pixels of space between the day and month
  }
  GRect frame =  GRect(x, 0, DATE_IMAGE_WIDTH, DATE_IMAGE_HEIGHT);

  unload_digit_image_from_slot(date_slot);
  load_digit_image_into_slot(date_slot, digit_value, &date_layer, frame, DATE_IMAGE_RESOURCE_IDS);
}

// Seconds
void update_second_slot(Slot *second_slot, int digit_value) {
  if (second_slot->state == digit_value) {
    return;
  }

  GRect frame = GRect(
    second_slot->number * (SECOND_IMAGE_WIDTH + MARGIN), 
    0, 
    SECOND_IMAGE_WIDTH, 
    SECOND_IMAGE_HEIGHT
  );

  unload_digit_image_from_slot(second_slot);
  load_digit_image_into_slot(second_slot, digit_value, &seconds_layer, frame, SECOND_IMAGE_RESOURCE_IDS);
}

// Handlers
void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler   = &handle_init,
    .deinit_handler = &handle_deinit,
//*********************************************************************************
    .timer_handler = &handle_timer  // Put timer in instead of tick_handler
//*********************************************************************************
//    .tick_info = {
//      .tick_handler = &handle_second_tick,
//      .tick_units   = SECOND_UNIT
//    }
//*********************************************************************************
  };

  app_event_loop(params, &handlers);
}
//*********************************************************************************
void config_provider(ClickConfig **config, Window *window) {
    config[BUTTON_RUN]->click.handler = (ClickHandler)toggle_stopwatch_handler;
    config[BUTTON_LAP]->long_click.handler = (ClickHandler) toggle_mode;
    config[BUTTON_RESET]->click.handler = (ClickHandler)reset_stopwatch_handler;
//    config[BUTTON_LAP]->click.handler = (ClickHandler)lap_time_handler;
//    config[BUTTON_LAP]->long_click.handler = (ClickHandler)handle_display_lap_times;
//    config[BUTTON_LAP]->long_click.delay_ms = 700;
    (void)window;
}
//*********************************************************************************

void handle_init(AppContextRef ctx) {
//*********************************************************************************
  app = ctx;  // needed to add this as not passed in click provider
//*********************************************************************************
  window_init(&window, "Revolution");
  window_stack_push(&window, true /* Animated */);
//*********************************************************************************
  window_set_fullscreen(&window, true); // if app need to do this.
  // Arrange for user input.
  window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);
//*********************************************************************************
  window_set_background_color(&window, GColorBlack);

  resource_init_current_app(&APP_RESOURCES);


  // Time slots
  for (int i = 0; i < NUMBER_OF_TIME_SLOTS; i++) {
    TimeSlot *time_slot = &time_slots[i];
    time_slot->slot.number  = i;
    time_slot->slot.state   = EMPTY_SLOT;
    time_slot->new_state    = EMPTY_SLOT;
    time_slot->animating    = false;
  }

  // Date slots
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    Slot *date_slot = &date_slots[i];
    date_slot->number = i;
    date_slot->state  = EMPTY_SLOT;
  }

  // Second slots
  for (int i = 0; i < NUMBER_OF_SECOND_SLOTS; i++) {
    Slot *second_slot = &second_slots[i];
    second_slot->number = i;
    second_slot->state  = EMPTY_SLOT;
  }

  // Day slot
  day_item.loaded = false;


  // Root layer
  Layer *root_layer = window_get_root_layer(&window);

  // Time
  layer_init(&time_layer, GRect(0, 0, SCREEN_WIDTH, SCREEN_WIDTH));
  layer_set_clips(&time_layer, true);
  layer_add_child(root_layer, &time_layer);

  // Date container
  int date_container_height = SCREEN_HEIGHT - SCREEN_WIDTH;

  layer_init(&date_container_layer, GRect(0, SCREEN_WIDTH, SCREEN_WIDTH, date_container_height));
  layer_add_child(root_layer, &date_container_layer);

  // Day
  GRect day_layer_frame = GRect(
    MARGIN, 
    date_container_height - DAY_IMAGE_HEIGHT - MARGIN, 
    DAY_IMAGE_WIDTH, 
    DAY_IMAGE_HEIGHT
  );
  layer_init(&day_item.layer, day_layer_frame);
  layer_add_child(&date_container_layer, &day_item.layer);

  // Date
  GRect date_layer_frame = GRectZero;
  date_layer_frame.size.w   = DATE_IMAGE_WIDTH + MARGIN + DATE_IMAGE_WIDTH + DATE_PART_SPACE + DATE_IMAGE_WIDTH + MARGIN + DATE_IMAGE_WIDTH;
  date_layer_frame.size.h   = DATE_IMAGE_HEIGHT;
  date_layer_frame.origin.x = (SCREEN_WIDTH - date_layer_frame.size.w) / 2;
  date_layer_frame.origin.y = date_container_height - DATE_IMAGE_HEIGHT - MARGIN;

  layer_init(&date_layer, date_layer_frame);
  layer_add_child(&date_container_layer, &date_layer);

  // Seconds
  GRect seconds_layer_frame = GRect(
    SCREEN_WIDTH - SECOND_IMAGE_WIDTH - MARGIN - SECOND_IMAGE_WIDTH - MARGIN, 
    date_container_height - SECOND_IMAGE_HEIGHT - MARGIN, 
    SECOND_IMAGE_WIDTH + MARGIN + SECOND_IMAGE_WIDTH, 
    SECOND_IMAGE_HEIGHT
  );
  layer_init(&seconds_layer, seconds_layer_frame);
  layer_add_child(&date_container_layer, &seconds_layer);

//*********************************************************************************
// Mode icons
  for (int i=0;i<MODES;i++)
  {
        bmp_init_container(mapModeImage[i].resourceid,&modeImages[i]);
        // layer_set_frame(&modeImages[i].layer.layer, GRect(0,0,13,23));
        layer_set_frame(&modeImages[i].layer.layer, GRect((144 - 12)/2,((144 - 16)/2),12,16));
        layer_set_hidden( &modeImages[i].layer.layer, true);
        layer_add_child(&window.layer,&modeImages[i].layer.layer);
  }
  ticks = 0;
  // Set up a layer for the second hand
 yachtimer_init(&myYachtTimer,mapModeImage[modetick].mode);
 yachtimer_setConfigTime(&myYachtTimer, ASECOND * 60 * 10);
 yachtimer_tick(&myYachtTimer,0);
//*********************************************************************************

  // Display
//*********************************************************************************
  stop_stopwatch();
  PblTm *tick_time;
  tick_time=yachtimer_getPblDisplayTime(&myYachtTimer);
  memcpy(&theLastTime, tick_time,sizeof(PblTm));
  tick_time = yachtimer_getPblLastTime(&myYachtTimer);
  theLastTime.tm_yday = tick_time->tm_yday;
  theLastTime.tm_mon = tick_time->tm_mon;
  theLastTime.tm_year = tick_time->tm_year;
//*********************************************************************************

  display_time(tick_time);
  display_day(tick_time);
  display_date(tick_time);
  display_seconds(tick_time);
}

//*********************************************************************************
// Wrap normal timer mode with stopwatch handling
void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie ) {
  (void)ctx;
  PebbleTickEvent theEvent;
  PblTm *theTime;
   
   if(cookie == TIMER_UPDATE)
   {
          yachtimer_tick(&myYachtTimer,ASECOND);
          ticklen = yachtimer_getTick(&myYachtTimer);
	  theEvent.tick_time = yachtimer_getPblDisplayTime(&myYachtTimer);
          theTime = yachtimer_getPblLastTime(&myYachtTimer);

	  // Work out time changed
          // In all modes do hors minutes and seconds
          // In non-watch modes have day, date, day of week etc follow clock 
          theEvent.units_changed = 0;
          theEvent.units_changed |= (theLastTime.tm_sec != theEvent.tick_time->tm_sec)?SECOND_UNIT:theEvent.units_changed;
          theEvent.units_changed |= (theLastTime.tm_min != theEvent.tick_time->tm_min)?MINUTE_UNIT:theEvent.units_changed;
          theEvent.units_changed |= (theLastTime.tm_hour != theEvent.tick_time->tm_hour)?HOUR_UNIT:theEvent.units_changed;
	  if(yachtimer_getMode(&myYachtTimer) == WATCHMODE)
	  {
		  theEvent.units_changed |= (theLastTime.tm_yday != theEvent.tick_time->tm_yday)?DAY_UNIT:theEvent.units_changed;
		  theEvent.units_changed |= (theLastTime.tm_mon != theEvent.tick_time->tm_mon)?MONTH_UNIT:theEvent.units_changed;
		  theEvent.units_changed |= (theLastTime.tm_year != theEvent.tick_time->tm_year)?YEAR_UNIT:theEvent.units_changed;
	   }
	   else
	   {
		
		  theEvent.units_changed |= (theLastTime.tm_yday != theTime->tm_yday)?DAY_UNIT:theEvent.units_changed;
		  theEvent.units_changed |= (theLastTime.tm_mon != theTime->tm_mon)?MONTH_UNIT:theEvent.units_changed;
		  theEvent.units_changed |= (theLastTime.tm_year != theTime->tm_year)?YEAR_UNIT:theEvent.units_changed;
	   }
           memcpy(&theLastTime,theEvent.tick_time,sizeof(PblTm));

	  if(yachtimer_getMode(&myYachtTimer) != WATCHMODE)
	  {
		  theLastTime.tm_yday = theTime->tm_yday;
		  theLastTime.tm_mon = theTime->tm_mon;
		  theLastTime.tm_year = theTime->tm_year;
	  }
	  if(ticks <= TICKREMOVE)
	  {
		theEvent.units_changed = SECOND_UNIT|MINUTE_UNIT|HOUR_UNIT|DAY_UNIT|MONTH_UNIT|YEAR_UNIT;
	  }

	  // Emulate every second tick
          if(update_timer != APP_TIMER_INVALID_HANDLE) {
              if(app_timer_cancel_event(app, update_timer)) {
                  update_timer = APP_TIMER_INVALID_HANDLE;
              }
          }
          // All second only stopwatches need a second rather than using ticklen
          update_timer = app_timer_send_event(ctx, ASECOND, TIMER_UPDATE);
          ticks++;
          if(ticks >= TICKREMOVE)
          {
                for(int i=0;i<MODES;i++)
                {
                        layer_set_hidden( &modeImages[i].layer.layer, true);
                }
          }
          theTimeEventType event = yachtimer_triggerEvent(&myYachtTimer);

        if(event == MinorTime) vibes_double_pulse();
        if(event == MajorTime) vibes_enqueue_custom_pattern(start_pattern);
	handle_second_tick(ctx,&theEvent);
   }
}
//*********************************************************************************
void handle_second_tick(AppContextRef ctx, PebbleTickEvent *event) {
  display_seconds(event->tick_time);

  if ((event->units_changed & SECOND_UNIT) == SECOND_UNIT) {
    display_time(event->tick_time);
  }

  if ((event->units_changed & DAY_UNIT) == DAY_UNIT) {
    display_day(event->tick_time);
    display_date(event->tick_time);
  }
}

void handle_deinit(AppContextRef ctx) {
  for (int i = 0; i < NUMBER_OF_TIME_SLOTS; i++) {
    unload_digit_image_from_slot(&time_slots[i].slot);
  }
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    unload_digit_image_from_slot(&date_slots[i]);
  }
  for (int i = 0; i < NUMBER_OF_SECOND_SLOTS; i++) {
    unload_digit_image_from_slot(&second_slots[i]);
  }

  if (day_item.loaded) {
    bmp_deinit_container(&day_item.image_container);
  }
  for(int i=0;i<MODES;i++)
        bmp_deinit_container(&modeImages[i]);
}
