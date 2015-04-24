#include "pebble.h"
Window *main_window;
Layer *root_layer;

GBitmap *font8_bmp, *sprites_bmp, *sprites_mask_bmp;
uint8_t *font8;
uint16_t *sprites, *sprites_mask;
uint8_t map[11*10];
// ------------------------------------------------------------------------------------------------------------------------------------------------ //
void load_images() {
  font8_bmp        = gbitmap_create_with_resource(RESOURCE_ID_FONT8);
  sprites_bmp      = gbitmap_create_with_resource(RESOURCE_ID_SPRITES);
  sprites_mask_bmp = gbitmap_create_with_resource(RESOURCE_ID_SPRITES_MASK);
  //font8 = gbitmap_get_data(font8_bmp);
  font8        = font8_bmp->addr;
  sprites      = sprites_bmp->addr;
  sprites_mask = sprites_mask_bmp->addr;
}

void destroy_images() {
  gbitmap_destroy(font8_bmp);
  gbitmap_destroy(sprites_bmp);
  gbitmap_destroy(sprites_mask_bmp);
}

void draw_block16(uint16_t *screen, int16_t start_x, int16_t start_y, uint8_t spr) {
  uint32_t left  = (start_x <   0) ?   0 - start_x : 0;
  uint32_t right = (start_x > 128) ? (start_x > 143) ? 0 : 144 - start_x : 16;
  
    for(uint32_t y=0; y<16; y++) {
      uint16_t y_addr = (start_y + y) * 10;
      uint16_t row = sprites[(spr&1) + ((spr&254)*16) + (y*2)];
      
      for(uint32_t x=left; x<right; x++) {
        screen[y_addr + ((start_x+x) >> 4)] &= ~(1 << ((start_x+x)&15)); // Black Background (comment out for clear)
        screen[y_addr + ((start_x+x) >> 4)] |=  (((row>>x)&1) << ((start_x+x)&15)); // White Pixel
      }
    }
}

void draw_sprite16(uint16_t *screen, int16_t start_x, int16_t start_y, uint8_t spr) {
  uint32_t left  = (start_x <   0) ?   0 - start_x : 0;
  uint32_t right = (start_x > 128) ? 144 - start_x : 16;
  if(start_x>143) right=0;
  
    for(uint32_t y=0; y<16; y++) {
      uint16_t y_addr = (start_y + y) * 10;
      uint16_t row = sprites[(spr&1) + ((spr&254)*16) + (y*2)];
      uint16_t mask_row = sprites_mask[(spr&1) + ((spr&254)*16) + (y*2)];
      
      for(uint32_t x=left; x<right; x++) {
        screen[y_addr + ((start_x+x) >> 4)] &= ~( ((mask_row>>x)&1) << ((start_x+x)&15)); // Black Background (comment out for clear)
        screen[y_addr + ((start_x+x) >> 4)] |=  ( (((mask_row>>x)&1) & ((row>>x)&1)) << ((start_x+x)&15)); // White Pixel
      }
    }
}


void draw_font8_fast(uint8_t *screen, int16_t start_x, int16_t start_y, uint8_t chr) {
  start_y = (start_y*20) + start_x;
  uint8_t *row = font8 + (chr&3) + ((chr&252)*8);
  for(uint32_t y=0; y<8; y++, start_y+=20, row+=4)
    screen[start_y] = *row;
}

void draw_font8_text(uint8_t *screen, int16_t x, int16_t y, char *str) {
  uint8_t strpos=0;
  while(str[strpos]>0) {
    draw_font8_fast(screen, x, y, str[strpos]);
    x++; strpos++;
  }
}

//--------------------------------------------------------------------------------------------------------------------------//

uint8_t offset=0;
uint8_t runner_frame=0;
uint8_t runner_x = 10;
uint8_t runner_y = 31;//168-31;
int16_t countdown=327*8;
uint8_t coins = 9;
uint32_t score = 237980;
int16_t yvel=0;
bool jumping=false;

// ------------------------------------------------------------------------ //
//  Button Functions
// ------------------------------------------------------------------------ //
static bool up_button_depressed = false; // Whether Pebble's   Up   button is held
static bool dn_button_depressed = false; // Whether Pebble's  Down  button is held
static bool sl_button_depressed = false; // Whether Pebble's Select button is held
void up_push_in_handler(ClickRecognizerRef recognizer, void *context) {up_button_depressed = true;}
void up_release_handler(ClickRecognizerRef recognizer, void *context) {up_button_depressed = false;}
void dn_push_in_handler(ClickRecognizerRef recognizer, void *context) {dn_button_depressed = true;}
void dn_release_handler(ClickRecognizerRef recognizer, void *context) {dn_button_depressed = false;}
void sl_push_in_handler(ClickRecognizerRef recognizer, void *context) {sl_button_depressed = true;}
void sl_release_handler(ClickRecognizerRef recognizer, void *context) {sl_button_depressed = false;}
void click_config_provider(void *context) {
  window_raw_click_subscribe(BUTTON_ID_UP, up_push_in_handler, up_release_handler, context);
  window_raw_click_subscribe(BUTTON_ID_DOWN, dn_push_in_handler, dn_release_handler, context);
  window_raw_click_subscribe(BUTTON_ID_SELECT, sl_push_in_handler, sl_release_handler, context);
}

// ------------------------------------------------------------------------ //
//  Timer Functions
// ------------------------------------------------------------------------ //
#define UPDATE_MS 50 // Refresh rate in milliseconds
static void timer_callback(void *data) {
  offset = (offset+5)&15;
  runner_frame = (runner_frame+1) % 3;
  countdown -= 1;
  if(countdown==0) countdown=400*8;
  if(up_button_depressed) coins=(coins+1)%100;
  if(sl_button_depressed) score+=100;
  
  if(yvel<0 && (runner_y+yvel<=31)) {
    runner_y=31;
    yvel=0;
    jumping=false;
  };
  runner_y += yvel;
  yvel-=6;

  if(dn_button_depressed) {
    if(jumping==false)
      {yvel=15; jumping=true;}
    else
      yvel+=4;
  }

  
  layer_mark_dirty(root_layer);                    // Schedule redraw of screen
  app_timer_register(UPDATE_MS, timer_callback, NULL); // Schedule a callback
}


// void up_single_click_handler(ClickRecognizerRef recognizer, void *context) {}
// void select_single_click_handler(ClickRecognizerRef recognizer, void *context) {}
// void down_single_click_handler(ClickRecognizerRef recognizer, void *context) {}
// void click_config_provider(void *context) {
//   //window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
//   window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, up_single_click_handler);
//   window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
//   window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
//   //window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, down_single_click_handler);
// }
  

void root_layer_update(Layer *me, GContext *ctx) {
  GBitmap* framebuffer = graphics_capture_frame_buffer(ctx);
  if(framebuffer) {
//     uint8_t* screen = gbitmap_get_data(framebuffer);
//     uint8_t*  screen8  = framebuffer->addr;
//     uint16_t* screen16 = framebuffer->addr;
    #define screen8   ((uint8_t*)framebuffer->addr)
    #define screen16 ((uint16_t*)framebuffer->addr)
    #define screen32 ((uint32_t*)framebuffer->addr)
    
    for(int16_t i=(18*5); i<168*5; i+=5)
      screen32[i] = ~0;
    
    for(uint8_t i=0; i<10; i++)
      draw_block16(screen16, i*16 - offset, 168-16, 1);

    draw_sprite16(screen16, runner_x, 168-runner_y, jumping ? 8 : runner_frame<<1);

    draw_font8_text(screen8, 0, 0, "MARIO         TIME\0");
    static char text[40];  //Buffer to hold text
    snprintf(text, sizeof(text), "%06ld  \x04*%02d   %03d", score%1000000, coins, countdown>>3);
    text[20]=0;
    draw_font8_text(screen8, 0, 8, text);
//     draw_font8_text(screen8, 0, 8, "000000  \x04*00   327\0");

    graphics_release_frame_buffer(ctx, framebuffer);
  }
  
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0,17), GPoint(143,17));
  
}

// ------------------------------------------------------------------------ //
//  Main Functions
// ------------------------------------------------------------------------ //
static void main_window_load(Window *window) {
  root_layer = window_get_root_layer(window);
  layer_set_update_proc(root_layer, root_layer_update);
}

static void init(void) {
  main_window = window_create();
  window_set_window_handlers(main_window, (WindowHandlers) {
    .load = main_window_load,
  });
  window_set_click_config_provider(main_window, click_config_provider);
  window_set_fullscreen(main_window, true);

  load_images();
  
  window_stack_push(main_window, true); // Display window
  app_timer_register(UPDATE_MS, timer_callback, NULL); // Schedule a callback
}
  
static void deinit(void) {
  destroy_images();
  window_destroy(main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}