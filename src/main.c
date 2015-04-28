#include "pebble.h"
Window *main_window;
Layer *root_layer, *message_layer;
AppTimer *looptimer;

// .h stuff
#define UPDATE_MS 50 // Refresh rate in milliseconds
#define screen8   ((uint8_t*)framebuffer->addr)
#define screen16 ((uint16_t*)framebuffer->addr)
#define screen32 ((uint32_t*)framebuffer->addr)
static void timer_callback(void *data);  // need to define this here so up button can pause

// end .h stuff

GBitmap *font8_bmp, *sprites_bmp, *sprites_mask_bmp;
uint8_t *font8;
uint16_t *sprites, *sprites_mask;
uint8_t map[16*16];

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

// draws without mask
void draw_block16(uint16_t *screen, int16_t start_x, int16_t start_y, uint8_t spr) {
  uint32_t left  = (start_x <   0) ?                     0 - start_x : 0;
  uint32_t right = (start_x > 128) ? (start_x < 144) ? 144 - start_x : 0 : 16;
  
    for(uint32_t y=0; y<16; y++) {
      uint16_t y_addr = (start_y + y) * 10;
      uint16_t row = sprites[(spr&1) + ((spr&254)*16) + (y*2)];
      
      for(uint32_t x=left; x<right; x++) {
        screen[y_addr + ((start_x+x) >> 4)] &= ~(1 << ((start_x+x)&15)); // Black Background (comment out for clear)
        screen[y_addr + ((start_x+x) >> 4)] |=  (((row>>x)&1) << ((start_x+x)&15)); // White Pixel
      }
    }
}

// draws with mask
void draw_sprite16(uint16_t *screen, int16_t start_x, int16_t start_y, uint8_t spr) {
  uint16_t left   = (start_x <      0) ? (start_x >  -16) ?   0 - start_x : 16 :  0;
  uint16_t right  = (start_x > 144-16) ? (start_x <  144) ? 144 - start_x :  0 : 16;
  uint16_t top    = (start_y <      0) ? (start_y >  -16) ?   0 - start_y : 16 :  0;
  uint16_t bottom = (start_y > 168-16) ? (start_y <  168) ? 168 - start_y :  0 : 16;
//   static char text[40];  //Buffer to hold text
//   snprintf(text, sizeof(text), "%d %d - %d %d %d %d", start_x, start_y, left, right, top, bottom);
//   snprintf(text, sizeof(text), "%d - %d %d", start_y, top, bottom);
//   APP_LOG(APP_LOG_LEVEL_INFO, text);

  for(uint16_t y=top; y<bottom; y++) {
    uint16_t y_addr = (start_y + y) * 10;
    uint16_t row = sprites[(spr&1) + ((spr&254)*16) + (y*2)];
    uint16_t mask_row = sprites_mask[(spr&1) + ((spr&254)*16) + (y*2)];
      
    for(uint16_t x=left; x<right; x++) {
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

uint8_t  offset;
uint8_t  runner_frame;
uint8_t  runner_x;
uint8_t  runner_y;
int16_t  countdown;
uint8_t  coins;
uint32_t score;
uint8_t  lives;
//int16_t yvel=0;
uint8_t yvel;
bool paused=false;

// bool jumping=false;
uint8_t jumpmode; // 0=on ground, button let go, ready to jump, 1=jumping button held, 2=falling button let go
uint8_t runmode; // 0=running, 1=stopped (TODO: Mix with JUMPMODE and ALIVE to make PlayerState)
bool    alive;
uint8_t coinanimation;

// ------------------------------------------------------------------------ //
//  Button Functions
// ------------------------------------------------------------------------ //
static bool up_button_depressed = false; // Whether Pebble's   Up   button is held
static bool dn_button_depressed = false; // Whether Pebble's  Down  button is held
static bool sl_button_depressed = false; // Whether Pebble's Select button is held
static bool dn_button_previous  = false; // Whether Pebble's  Down  button is held


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
  
void up_push_in_handler(ClickRecognizerRef recognizer, void *context) {
  up_button_depressed = true;
  paused=!paused;
  if(paused)
    app_timer_cancel(looptimer);
  else
    timer_callback(NULL);
}
void up_release_handler(ClickRecognizerRef recognizer, void *context) {up_button_depressed = false;}
void dn_push_in_handler(ClickRecognizerRef recognizer, void *context) {dn_button_depressed = true;}
void dn_release_handler(ClickRecognizerRef recognizer, void *context) {dn_button_depressed = false;}
void sl_push_in_handler(ClickRecognizerRef recognizer, void *context) {sl_button_depressed = true;
                                                                      if(paused) timer_callback(NULL);  // frame advance
                                                                      }
void sl_release_handler(ClickRecognizerRef recognizer, void *context) {sl_button_depressed = false;}
void click_config_provider(void *context) {
  window_raw_click_subscribe(BUTTON_ID_UP, up_push_in_handler, up_release_handler, context);
  window_raw_click_subscribe(BUTTON_ID_DOWN, dn_push_in_handler, dn_release_handler, context);
  window_raw_click_subscribe(BUTTON_ID_SELECT, sl_push_in_handler, sl_release_handler, context);
}

// ------------------------------------------------------------------------ //
//  Timer Functions
// ------------------------------------------------------------------------ //
static void remove_message(void *data) {
  layer_set_hidden(message_layer, true);
  timer_callback(NULL);
}



static void init_game(void *data) {
  offset=0;
  runner_frame=0;
  runner_x = 30+48;
  runner_y = 150;//31;//168-31;
  countdown=400*8+7; // +7 so it stays on 400 for a full second
  yvel=0;
  jumpmode=0; // 0=on ground, button let go, ready to jump, 1=jumping button held, 2=falling button let go
  runmode=0; // 0=running, 1=stopped (TODO: Mix with JUMPMODE and ALIVE to make PlayerState)
  alive=true;
  coinanimation=0;

  //rand() % 3 == 0 ? 1 : 0;
  for(uint16_t i=0; i<256; i++) map[i] = 0; // Clear map
  for(uint16_t i=0; i<16; i++) map[i] = 1;  // Bottom path
  for(uint16_t i=0; i<16; i++) map[(16*15) + i] = 9;//7  // Hidden ceiling/bottom of pit
  //for(uint16_t i=18; i<16+13; i++) map[i] = 9;//1;  // 2nd Bottom path
  map[16*3]=3;map[16*3+1]=3;
  map[16*6 + 4]=5;map[16*6 + 5]=7;
  
  if(lives>0)
    app_timer_register(3000, remove_message, NULL); // Show message for 3000ms, then start game
  layer_set_hidden(message_layer, false);
  layer_mark_dirty(message_layer);
}



static void death_timer_callback(void *data) {
  runner_y += yvel;  // move y position
  yvel-=2;           // Death Gravity
  if((int8_t)yvel<-10) yvel=246; // terminal velocity (int -10 = uint 246)
  
  layer_mark_dirty(root_layer);      // Schedule redraw of screen
  if(runner_y<240)                                                       // if not below bottom of screen
    looptimer=app_timer_register(UPDATE_MS, death_timer_callback, NULL); // Schedule a callback to continue animation
  else {
    lives--;
    app_timer_register(1000, init_game, NULL); // Start again
  }
}

uint8_t Q1, Q2;
static void timer_callback(void *data) {
  uint8_t occupied1, occupied2;
  // outline:
  //  Move Player
  //  
  
  
  
  // Move player
  offset += 5;                  // move tiles left 5 pixels
  
  if((int8_t)yvel<-10) yvel=246; // terminal velocity (int -10 = uint 246)

  // See if runner hit something to the right
  runmode=0; // mode=running (unless hit a block sideways)
  occupied1=map[(((runner_x+14+offset)>>4)&15) + ((runner_y-13)&240)]; // square occupied by bottom-right area
  occupied2=map[(((runner_x+14+offset)>>4)&15) + ((runner_y- 2)&240)]; // square occupied by upper-right
  if(occupied1==1 || occupied1==3 || occupied1==5 || occupied1==7 || occupied2==1 || occupied2==3 || occupied2==5 || occupied2==7) {
    runmode=1; // not running
    //runner_x -= 6; // -6 instead of -5 cause of catching-up below
    runner_x -= (runner_x+14+offset)&15; // push player to against block (+15 instead of +16 so he's kinda inside it)
    if(runner_x<(48-10)) { // hit left wall
      alive=false;         // dead
      runner_x=48;         // move sprite into view for death animation
    }
  } else {
    score+=3; // points for just running
    //runner_x+=1; if(runner_x>30+48) runner_x=30+48; // catching back up
    runner_x+=1; if(runner_x>50+48) runner_x=50+48; // catching back up
  }

  yvel-=6;  // Gravity
  runner_y += yvel;  // fall
  
  if(yvel>127) { // if falling downward, check if feet hit (>127 means <0)
    //if((runner_y+yvel)&255<15 || runner_y<15) {  // if touching bottom, dead
    if(((runner_y)&255)<=15) {  // if touching bottom, dead
      alive=false;
      //yvel=0;
    } else {
      occupied1=map[(((runner_x+2+offset)>>4)&15) + ((runner_y-16)&240)]; // bottom left area of player
      occupied2=map[(((runner_x+13+offset)>>4)&15) + ((runner_y-16)&240)]; // bottom right area of player
      if(occupied1==1 || occupied1==3 || occupied1==5 || occupied1==7 || occupied2==1 || occupied2==3 || occupied2==5 || occupied2==7) {
        runner_y=((runner_y-16)&240)+16+15; //+16+15: 16 to put on top of block and 15 for top of head
        yvel=0; // standing on top of a block
        jumpmode=0;  // not jumping (running animation)
        // &240 = >>4 (to get map square) * 16 (16 because: addr = y * 16 (the array width))
        //     } else if(getmap((runner_x+8+offset), (runner_y+yvel-15))>0) { // &240 = >>4 (to get map square) * 16 (16 because: addr = y * 16 (the array width))
        // 16+16+16 cause map is 3 left (with 10 center and 3 right = 16 wide)
        //TODO: wide foot print so if 2 or 3px either way is land, then stand
      }
    }
  } else {  // else moving up or 0
    // add [if hitting head] here
    //occupied=(((runner_x+8+offset)>>4)&15) + ((runner_y+yvel)&240);
    occupied1=map[(((runner_x+ 2+offset)>>4)&15) + ((runner_y)&240)];
    occupied2=map[(((runner_x+13+offset)>>4)&15) + ((runner_y)&240)];
    if(occupied1==1 || occupied1==3 || occupied1==5 || occupied1==7 || occupied2==1 || occupied2==3 || occupied2==5 || occupied2==7) {
      runner_y=((runner_y)&240)-1; //+16+15: 16 to put on top of block and 15 for top of head
      yvel=0; // block stops jump
    }
  }

  // See what ran into
  occupied1=(((runner_x+8+offset)>>4)&15) + ((runner_y-8)&240); // center of player
  if(map[occupied1]==9) { // if player occupies same spot as a coin
    map[occupied1]=0;
    coins++;
    score+=100;
    vibes_cancel(); vibes_enqueue_custom_pattern((VibePattern){.durations = (uint32_t []){40}, .num_segments = 1});
  }
  if(coins>99) {
    coins-=100;
    lives++;
    // add object:
    //       type: message (like points or 1up)
    //     sprite: 1-UP
    //   location: player x+8 & y
    // add_object(1up, player.x+8, player.y);
    // change_object(koopatroopa, shell)
  }
  
  
  // Button Checks:  
  // if(up_button_depressed) coins=(coins+1)%100;
  // if(sl_button_depressed) score+=100;
      occupied1=map[(((runner_x+ 2+offset)>>4)&15) + ((runner_y-16)&240)]; // bottom left area of player (15 instead of 16 cause he stands ON the block. 16 now cause, uh, he can use his toes?)
      occupied2=map[(((runner_x+13+offset)>>4)&15) + ((runner_y-16)&240)]; // bottom right area of player
      Q1=occupied1; Q2=occupied2;
  if(dn_button_depressed) {  // if jump button is being pushed
    if(!dn_button_previous) { // if it was JUST pushed (try to jump)
//occupied=
      if(occupied1==1 || occupied1==3 || occupied1==5 || occupied1==7 || occupied2==1 || occupied2==3 || occupied2==5 || occupied2==7) {
        jumpmode=1; // playermode = jumping
        yvel=11+4;    // y velocity of jumping (note: adds 2 below)
      }
    }
    if(jumpmode==1) yvel+=4;  // less gravity if holding button while jumping
  } else {
    if(jumpmode==1) // if not holding button while jumping
      jumpmode=2;   // then now falling
  }
  dn_button_previous = dn_button_depressed;  // used to see if down was just pressed or is being held
  

  // Update Timer and Animations
  countdown -= 1; //if(countdown==0) countdown=400*8;  // timer
  if(countdown<8) alive=false;  // timer runs out! (technically if timer>>3==0)
  coinanimation++;
  runner_frame = (runner_frame+1) % 3;  // next runner frame (whether running or not)
  

  
  // Generate stuff on the right
  map[(16*5) + ((15+(offset>>4)) &15)] = rand()%3==0 ? 0 : ((rand()%4)*2)+1;  // 2 and 13 (L & R) not visible, 3 and 12 partially visible
  map[((15+(offset>>4))&15)]=(rand()%10==0) ? 0 : 1;  // random pit
  if(rand()%10==0) map[(16*1) + ((15+(offset>>4))&15)]=9;  // random coin
  
  
  layer_mark_dirty(root_layer);                    // Schedule redraw of screen
  if(alive) {
    if(!paused)
      looptimer=app_timer_register(UPDATE_MS, timer_callback, NULL); // Schedule a callback
  } else {
    yvel=15;
    looptimer=app_timer_register(500, death_timer_callback, NULL); // Start death animation in 500ms
  }
}

// ------------------------------------------------------------------------ //
//  Layer Drawing Functions
// ------------------------------------------------------------------------ //
void message_layer_update(Layer *me, GContext *ctx) {
  static char text[40];  //Buffer to hold text
  GBitmap* framebuffer = graphics_capture_frame_buffer(ctx);
  if(framebuffer) {
    for(int16_t i=(16*5); i<168*5; i++) screen32[i] = ~0; // blank screen (from status bar down)

    draw_font8_text(screen8, 0, 0, "MARIO         TIME\0");
    snprintf(text, sizeof(text), "%06ld  %c*%02d      ", score%1000000, 4, coins);
    draw_font8_text(screen8, 0, 8, text);

    if(lives>0) {
      snprintf(text, sizeof(text), "LIVES * %d", lives);
      draw_font8_text(screen8, 4, 88, text);
    } else {
      draw_font8_text(screen8, 4, 88, "GAME  OVER");
    }
    
    for(int16_t i=0; i<168*5; i++) screen32[i] = ~screen32[i]; // Invert whole screen (now white on black)
    
    graphics_release_frame_buffer(ctx, framebuffer);
  }
}

void root_layer_update(Layer *me, GContext *ctx) {
  static char text[40];  //Buffer to hold text
  GBitmap* framebuffer = graphics_capture_frame_buffer(ctx);
  if(framebuffer) {
    for(int16_t i=(16*5); i<168*5; i++) screen32[i] = ~0; // blank screen (from status bar down)
    
    for(uint8_t y=0; y<10; y++)
      for(uint8_t x=3; x<13; x++)
         if(map[(y*16) + ((x+(offset>>4))&15)]>0)
          draw_block16(screen16, (x-4)*16 + (16-(offset&15)), 168-(16*(y+1)), map[(y*16) + ((x+(offset>>4))&15)]);
    
    
//     draw_block16(screen16, i*16 - (offset&15), 168-16, 1);
//      draw_sprite16(screen16, runner_x-48, 168-runner_y, jumping ? 8 : runner_frame<<1);
    
    //0,2,4=running, 6 = standing, 8=jumping, 10=dead
     draw_sprite16(screen16, runner_x-48, 168-runner_y, alive ? (jumpmode==0 ? (runmode==0 ? runner_frame<<1 : 6) : 8) : 10);
//     draw_sprite16(screen16, runner_x-48, 100, jumpmode==0 ? runner_frame<<1 : 8);

    if(countdown < 8)
      draw_font8_text(screen8, 5, 88, "TIME UP!");
    
    draw_font8_text(screen8, 0, 0, "MARIO         TIME\0");
    snprintf(text, sizeof(text), "%06ld  %c*%02d   %03d", score%1000000, ((coinanimation>>2)&3)+4, coins, countdown>>3);
    draw_font8_text(screen8, 0, 8, text);
    for(int16_t i=17*5; i<18*5; i++) screen32[i] = 0; // Black horizontal line

//     for(int16_t i=0; i<18*5; i++) screen32[i] = ~screen32[i]; // Invert top section

//     draw_font8_fast(screen8, 0, 32, 48+jumpmode);
    graphics_release_frame_buffer(ctx, framebuffer);
  }

//   graphics_context_set_text_color(ctx, GColorBlack);
//   snprintf(text, sizeof(text), "Timer: %ld", (uint32_t)looptimer);
//   graphics_draw_text(ctx, text, fonts_get_system_font("RESOURCE_ID_GOTHIC_14"), GRect(0, 32, 144, 60), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

  uint8_t occupied1=map[(((runner_x+ 2+offset)>>4)&15) + ((runner_y-16)&240)]; // bottom left area of player (15 instead of 16 cause he stands ON the block. 16 now cause, uh, he can use his toes?)
  uint8_t occupied2=map[(((runner_x+13+offset)>>4)&15) + ((runner_y-16)&240)]; // bottom right area of player (needs to be 13 and not 15 cause kinda inside right block - allow side cliff jumping)

  graphics_context_set_text_color(ctx, GColorBlack);
  snprintf(text, sizeof(text), "xy=(%d, %d) %d\n%d %d\n%d %d", runner_x, runner_y, yvel, occupied1, occupied2, Q1, Q2);
  graphics_draw_text(ctx, text, fonts_get_system_font("RESOURCE_ID_GOTHIC_14"), GRect(0, 18, 144, 60), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

// ------------------------------------------------------------------------ //
//  Main Functions
// ------------------------------------------------------------------------ //
static void main_window_load(Window *window) {
  root_layer = window_get_root_layer(window);
  layer_set_update_proc(root_layer, root_layer_update);
  
  message_layer = layer_create(layer_get_frame(root_layer));
  layer_add_child(root_layer, message_layer);
  layer_set_update_proc(message_layer, message_layer_update);
}

static void init(void) {
  main_window = window_create();
  window_set_window_handlers(main_window, (WindowHandlers) {
    .load = main_window_load,
  });
  window_set_click_config_provider(main_window, click_config_provider);
  window_set_fullscreen(main_window, true);
  
  srand(time(NULL));  // Seed randomizer
  load_images();
  coins = 0;
  score = 0;
  lives = 3;

  window_stack_push(main_window, true); // Display window
  //timer_callback(NULL);
  init_game(NULL);
  //looptimer=app_timer_register(UPDATE_MS, timer_callback, NULL); // Schedule a callback
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
/*
Disappearing Blocks
Falling Blocks
Stars
Fireballs
Enemies
Pipes do nothing (except plants)
Staircase
Pits
Waaay later: Underground level with easy ceiling but some holes and lifts


(Offset>>4 + x)&15 + y*16 = block on screen
F----------------
E----------------
D----------------
C----------------
B----------------
A----------------
9---oooooooooo---
8---oooooooooo---
7---oooooooooo---
6---oooooooooo---
5---oooooooooo---
4---oooooooooo---
3---oooooooooo---
2---oooooooooo---
1---oooooooooo---
0---oooooooooo---
 0123456789ABCDEF
*/