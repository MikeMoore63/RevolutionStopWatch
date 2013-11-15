#include <pebble.h>

// removed version 2.0 SDK
// #define MY_UUID { 0x91, 0x13, 0xA8, 0x37, 0x56, 0x92, 0x4C, 0xCA, 0x97, 0x86, 0xB2, 0x8D, 0x08, 0x1C, 0xDE, 0x35 }
// PBL_APP_INFO(MY_UUID,
//             "Revolution Stopwatch", "Mike Moore",
//             1, 2, /* App version */
//             RESOURCE_ID_IMAGE_MENU_ICON,
//             APP_INFO_STANDARD_APP);

// Envisioned as a watchface by Jean-Noël Mattern
// Based on the display of the Freebox Revolution, which was designed by Philippe Starck.

#include "yachtimercontrol.h"   // Add yachtimer control assume one model other controls are available :-)
				// the model is independant of watch control define models and approach
                                // manage insput

//************************* Boiler Plate Stop Watch stuff *****************************************
// The only control a as toggle mode  is available.
// Could have a multi control as well.
YachtTimerControl myControl;

// Resources and position of images come from the view
ModeResource myModes[MODES] = {
           { .mode = WATCHMODE, .resourceid = RESOURCE_ID_IMAGE_WATCH, .adjustnum=0 },
           { .mode = STOPWATCH, .resourceid = RESOURCE_ID_IMAGE_STOPWATCH, .adjustnum=0 },
           { .mode = YACHTIMER, .resourceid = RESOURCE_ID_IMAGE_YACHTTIMER, .adjustnum=0 },
           { .mode = COUNTDOWN, .resourceid = RESOURCE_ID_IMAGE_COUNTDOWN, .adjustnum=0 },
           { .mode = CNTDWNCFG, .resourceid = RESOURCE_ID_IMAGE_CNTDWNCFG, .adjustnum=60L * ASECOND },
};


//************************* Boiler Plate Stop Watch stuff *****************************************


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
Window *window;

Layer *date_container_layer;

#define EMPTY_SLOT -1

typedef struct Slot {
  int           number;
  BitmapLayer  *image_container;
  GBitmap       *image;
  int           state;
} Slot;

// Time
typedef struct TimeSlot {
  Slot              slot;
  int               new_state;
  PropertyAnimation *slide_out_animation;
  PropertyAnimation *slide_in_animation;
  bool              animating;
} TimeSlot;

#define NUMBER_OF_TIME_SLOTS 4
Layer *time_layer;
TimeSlot time_slots[NUMBER_OF_TIME_SLOTS];

// Date
#define NUMBER_OF_DATE_SLOTS 4
Layer *date_layer;
Slot date_slots[NUMBER_OF_DATE_SLOTS];

// Seconds
#define NUMBER_OF_SECOND_SLOTS 2
Layer *seconds_layer;
Slot second_slots[NUMBER_OF_SECOND_SLOTS];

// Day
typedef struct DayItem {
  BitmapLayer  *image_container;
  GBitmap      *image;
  Layer         *layer;
  bool          loaded;
} DayItem;
DayItem day_item;


// General
BitmapLayer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids);
void unload_digit_image_from_slot(Slot *slot);

// Display
void display_time(struct tm *tick_time);
void display_date(struct tm *tick_time);
void display_seconds(struct tm *tick_time);
void display_day(struct tm *tick_time);

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
int main(void);
// void pbl_main(void *params);
void handle_init();
void handle_second_tick(struct tm *time, TimeUnits units_changed);
void handle_deinit();


// General
BitmapLayer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids) {
  if (digit_value < 0 || digit_value > 9) {
    return NULL;
  }

  if (slot->state != EMPTY_SLOT) {
    return NULL;
  }

  slot->state = digit_value;

  slot->image = gbitmap_create_with_resource(digit_resource_ids[digit_value]);
  slot->image_container = bitmap_layer_create(slot->image->bounds);
  bitmap_layer_set_bitmap(slot->image_container, slot->image);
  layer_set_frame((Layer *)slot->image_container, frame);
  layer_add_child(parent_layer, (Layer *)slot->image_container);

  return slot->image_container;
}

void unload_digit_image_from_slot(Slot *slot) {
  if (slot->state == EMPTY_SLOT) {
    return;
  }


  layer_remove_from_parent((Layer *)slot->image_container);
  bitmap_layer_destroy(slot->image_container);
  gbitmap_destroy(slot->image);

  slot->state = EMPTY_SLOT;
}

// Display
void display_time(struct tm *tick_time) {
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

void display_date(struct tm *tick_time) {
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

void display_seconds(struct tm *tick_time) {
  int seconds = tick_time->tm_sec;

  seconds = seconds % 100; // Maximum of two digits per row.

  for (int second_slot_number = 1; second_slot_number >= 0; second_slot_number--) {
    Slot *second_slot = &second_slots[second_slot_number];

    update_second_slot(second_slot, seconds % 10);
    
    seconds = seconds / 10;
  }
}

void display_day(struct tm *tick_time) {

  if (day_item.loaded) {
    layer_remove_from_parent((Layer *)day_item.image_container);
    bitmap_layer_destroy(day_item.image_container);
    gbitmap_destroy(day_item.image);
  }

  day_item.image = gbitmap_create_with_resource(  DAY_IMAGE_RESOURCE_IDS[tick_time->tm_wday]);
  day_item.image_container = bitmap_layer_create(day_item.image->bounds);
  bitmap_layer_set_bitmap(day_item.image_container,day_item.image);
  layer_add_child(day_item.layer, (Layer *)day_item.image_container);

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
    animation = time_slot->slide_in_animation;
  }
  else {
    time_slot->new_state = digit_value;

    slide_out_digit_image_from_time_slot(time_slot);
    animation = time_slot->slide_out_animation;

    animation_set_handlers((Animation *)animation, (AnimationHandlers){
      .stopped = (AnimationStoppedHandler)time_slot_slide_out_animation_stopped
    }, (void *)time_slot);
  }

  animation_schedule((Animation *)animation);
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

  BitmapLayer *image_container = load_digit_image_into_slot(&time_slot->slot, digit_value, time_layer, from_frame, TIME_IMAGE_RESOURCE_IDS);


  time_slot->slide_in_animation = property_animation_create_layer_frame( (Layer *) image_container, &from_frame, &to_frame);
  animation_set_duration( (Animation *)time_slot->slide_in_animation , TIME_SLOT_ANIMATION_DURATION);
  animation_set_curve(   (Animation *) time_slot->slide_in_animation , AnimationCurveLinear);
  animation_set_handlers( (Animation *) time_slot->slide_in_animation,  (AnimationHandlers){
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

  BitmapLayer *image_container = time_slot->slot.image_container;

  time_slot->slide_out_animation = property_animation_create_layer_frame((Layer *)image_container,&from_frame, &to_frame);
  Animation *animation = (Animation *)time_slot->slide_out_animation;
  animation_set_duration( animation, TIME_SLOT_ANIMATION_DURATION);
  animation_set_curve(  animation, AnimationCurveLinear);

  // Make sure to unload the digit image from the slot when the animation has finished!
}

void time_slot_slide_out_animation_stopped(Animation *slide_out_animation, bool finished, void *context) {
  TimeSlot *time_slot = (TimeSlot *)context;

  unload_digit_image_from_slot(&time_slot->slot);

  slide_in_digit_image_into_time_slot(time_slot, time_slot->new_state);
  animation_schedule((Animation *)time_slot->slide_in_animation);

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
  load_digit_image_into_slot(date_slot, digit_value, date_layer, frame, DATE_IMAGE_RESOURCE_IDS);
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
  load_digit_image_into_slot(second_slot, digit_value, seconds_layer, frame, SECOND_IMAGE_RESOURCE_IDS);
}

int main(void)
{
	handle_init();
	app_event_loop();
        handle_deinit();
}

//// Handlers
// void pbl_main(void *params) {
//  PebbleAppHandlers handlers = {
//    .init_handler   = &handle_init,
//    .deinit_handler = &handle_deinit,
////*********************************************************************************
//    .timer_handler = &yachtimercontrol_handle_timer  // Put timer in instead of tick_handler
////*********************************************************************************
////    .tick_info = {
////      .tick_handler = &handle_second_tick,
////      .tick_units   = SECOND_UNIT
////    }
////*********************************************************************************
//  };
//
//  app_event_loop(params, &handlers);
//}

void handle_init() {
  window = window_create();
//*********************************************************************************
  window_set_fullscreen(window, true); // if app need to do this.
//*********************************************************************************
  window_stack_push(window, true /* Animated */);

  window_set_background_color(window, GColorBlack);



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
  Layer *root_layer = window_get_root_layer(window);

  // Time
  time_layer = layer_create(GRect(0, 0, SCREEN_WIDTH, SCREEN_WIDTH));
  layer_set_clips(time_layer, true);
  layer_add_child(root_layer, time_layer);

  // Date container
  int date_container_height = SCREEN_HEIGHT - SCREEN_WIDTH;

  date_container_layer = layer_create(GRect(0, SCREEN_WIDTH, SCREEN_WIDTH, date_container_height));
  layer_add_child(root_layer, date_container_layer);

  // Day
  GRect day_layer_frame = GRect(
    MARGIN, 
    date_container_height - DAY_IMAGE_HEIGHT - MARGIN, 
    DAY_IMAGE_WIDTH, 
    DAY_IMAGE_HEIGHT
  );
  day_item.layer = layer_create(day_layer_frame);
  layer_add_child(date_container_layer, day_item.layer);

  // Date
  GRect date_layer_frame = GRectZero;
  date_layer_frame.size.w   = DATE_IMAGE_WIDTH + MARGIN + DATE_IMAGE_WIDTH + DATE_PART_SPACE + DATE_IMAGE_WIDTH + MARGIN + DATE_IMAGE_WIDTH;
  date_layer_frame.size.h   = DATE_IMAGE_HEIGHT;
  date_layer_frame.origin.x = (SCREEN_WIDTH - date_layer_frame.size.w) / 2;
  date_layer_frame.origin.y = date_container_height - DATE_IMAGE_HEIGHT - MARGIN;

  date_layer = layer_create( date_layer_frame);
  layer_add_child(date_container_layer, date_layer);

  // Seconds
  GRect seconds_layer_frame = GRect(
    SCREEN_WIDTH - SECOND_IMAGE_WIDTH - MARGIN - SECOND_IMAGE_WIDTH - MARGIN, 
    date_container_height - SECOND_IMAGE_HEIGHT - MARGIN, 
    SECOND_IMAGE_WIDTH + MARGIN + SECOND_IMAGE_WIDTH, 
    SECOND_IMAGE_HEIGHT
  );
  seconds_layer = layer_create( seconds_layer_frame);
  layer_add_child(date_container_layer, seconds_layer);

//************************************* set up the control *******************************
  GRect modeIndicatorPos = GRect(
	((144 - 12)/2),
	((144 - 16)/2),
	12,
	16);
  yachtimercontrol_init(&myControl,
			window, 
			myModes, 
			MODES,
			modeIndicatorPos,
			handle_second_tick);
//************************************* set up the control *******************************


  // Display

//************************************* the control makes this happen  *******************************
 /*   struct tm *tick_time = yachtimer_getPblDisplayTime(yachtimercontrol_getModel(&myControl));
   display_time(tick_time);
   display_day(tick_time);
   display_date(tick_time);
   display_seconds(tick_time); */
}

void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  display_seconds(tick_time);

  if ((units_changed & SECOND_UNIT) == SECOND_UNIT) {
    display_time(tick_time);
  }

  if ((units_changed & DAY_UNIT) == DAY_UNIT) {
    display_day(tick_time);
    display_date(tick_time);
  }
}

void handle_deinit() {
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
    bitmap_layer_destroy(day_item.image_container);
    gbitmap_destroy(day_item.image);
  }
   //************************************* unset up the control *******************************
  yachtimercontrol_deinit(&myControl);
   //************************************* unset up the control *******************************
}
