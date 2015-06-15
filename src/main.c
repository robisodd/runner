#include "pebble.h"
#include "graphics.h"
Window *main_window;
Layer *root_layer, *message_layer;
AppTimer *looptimer;

// .h stuff
#define UPDATE_MS 50 // Refresh rate in milliseconds
#define screen8   ((uint8_t*)framebuffer->addr)
#define screen16 ((uint16_t*)framebuffer->addr)
#define screen32 ((uint32_t*)framebuffer->addr)
static void main_loop(void *data);  // need to define this here so up button can pause

// end .h stuff


//--------------------------------------------------------------------------------------------------------------------------//

uint8_t map[16*16];
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

int addInt(int n, int m) {
    return n+m;
}
//First thing, lets define a pointer to a function which receives 2 ints and returns and int:

//int (*functionPtr)(int,int);
//Now we can safely point to our function:

//functionPtr = &addInt;


//void *pause_function(void *data);

void pause_game() {
  if(!paused) {
    
  }
}

void focus_handler(bool in_focus) {
  if (!in_focus)
    paused = true;
}

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
  
static void up_push_in_handler(ClickRecognizerRef recognizer, void *context) {
  up_button_depressed = true;
  paused=!paused;
  if(paused)
    app_timer_cancel(looptimer);
  else
    main_loop(NULL);
}
static void up_release_handler(ClickRecognizerRef recognizer, void *context) {up_button_depressed = false;}
static void dn_push_in_handler(ClickRecognizerRef recognizer, void *context) {dn_button_depressed = true;}
static void dn_release_handler(ClickRecognizerRef recognizer, void *context) {dn_button_depressed = false;}
static void sl_push_in_handler(ClickRecognizerRef recognizer, void *context) {sl_button_depressed = true;
                                                                              if(paused) main_loop(NULL);  // frame advance
                                                                             }
static void sl_release_handler(ClickRecognizerRef recognizer, void *context) {sl_button_depressed = false;}
static void click_config_provider(void *context) {
  window_raw_click_subscribe(BUTTON_ID_UP, up_push_in_handler, up_release_handler, context);
  window_raw_click_subscribe(BUTTON_ID_DOWN, dn_push_in_handler, dn_release_handler, context);
  window_raw_click_subscribe(BUTTON_ID_SELECT, sl_push_in_handler, sl_release_handler, context);
}

// ------------------------------------------------------------------------ //
//  Timer Functions
// ------------------------------------------------------------------------ //
static void remove_message(void *data) {
  layer_set_hidden(message_layer, true);  // remove "LIVES x #"
  main_loop(NULL);                   // start the action!
}


static void init_round(void *data) {
  offset=0;
  runner_frame=0;
  runner_x = 30+48;
  runner_y = 150;//31;//168-31;
  countdown=400*8+7; // +7 so it stays on 400 for a full second
  yvel=0;
  jumpmode=0; // 0=on ground & button let go (i.e. ready to jump), 1=jumping & button held, 2=falling & button let go
  runmode=0; // 0=running, 1=stopped (TODO: Mix with JUMPMODE and ALIVE to make PlayerState)
  alive=true;
  coinanimation=0;

  // design level?
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
  if(paused) {
    looptimer=app_timer_register(UPDATE_MS, death_timer_callback, NULL); // Schedule a callback to continue animation
  } else {
  runner_y += yvel;  // move y position
  yvel-=2;           // Death Gravity
  if((int8_t)yvel<-10) yvel=246; // terminal velocity (int -10 = uint 246)
  
  layer_mark_dirty(root_layer);      // Schedule redraw of screen
  if(runner_y<240)                                                       // if not below bottom of screen
    looptimer=app_timer_register(UPDATE_MS, death_timer_callback, NULL); // Schedule a callback to continue animation
  else    
    looptimer=app_timer_register(1000, init_round, NULL);  // Wait a full second, then start over
  }
}


uint8_t Q1, Q2;
static void main_loop(void *data) {
  uint8_t occupied1, occupied2;
  // outline:
  //  Move Player
  //  
  
  
  
  // Move player horizontally
  offset += 5;                  // move tiles left 5 pixels

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
    runner_x+=1; if(runner_x>30+48) runner_x=30+48; // catching back up
    //runner_x+=1; if(runner_x>50+48) runner_x=50+48; // catching back up
  }

  // Move Player Vertically
  yvel-=6;  // Gravity
  if((int8_t)yvel<-10) yvel=246; // terminal velocity (int -10 = uint 246)
  runner_y += yvel;  // fall (or rise)
  
  if(yvel>127) { // if falling downward, check if feet hit (>127 means <0)
    if(runner_y<=15) {  // if touching bottom, dead
      alive=false;
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
  } else if(yvel>0) {  // else moving up (could remove yvel>0 and make this just an else if it works with 0 velocity)
    // add [if hitting head] here
    //occupied=(((runner_x+8+offset)>>4)&15) + ((runner_y+yvel)&240);
    occupied1=map[(((runner_x+ 2+offset)>>4)&15) + (runner_y&240)];
    occupied2=map[(((runner_x+13+offset)>>4)&15) + (runner_y&240)];
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
    vibes_cancel(); vibes_enqueue_custom_pattern((VibePattern){.durations = (uint32_t []){20}, .num_segments = 1});  // pulse with each coin
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
//occupied=map[] <-- put the two lines above Q1 Q2 here
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
      looptimer=app_timer_register(UPDATE_MS, main_loop, NULL); // Schedule a callback
  } else {
    yvel=15;
    lives--;
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
  //graphics_draw_text(ctx, text, fonts_get_system_font("RESOURCE_ID_GOTHIC_14"), GRect(0, 18, 144, 60), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

// ------------------------------------------------------------------------ //
//  Main Functions
// ------------------------------------------------------------------------ //
static void main_window_load(Window *window) {
  //Game is displayed on window's root layer, meaning no erase
  root_layer = window_get_root_layer(window);
  layer_set_update_proc(root_layer, root_layer_update);
  
  // message layer is what shows "LIVES x 3" and "GAME OVER"
  message_layer = layer_create(layer_get_frame(root_layer));
  layer_add_child(root_layer, message_layer);
  layer_set_update_proc(message_layer, message_layer_update);
  
  //focus
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
  window_stack_push(main_window, true); // Display window

  app_focus_service_subscribe(focus_handler);
  
  // init game
  coins = 0;
  score = 0;
  lives = 3;
  init_round(NULL);
}
  
static void deinit(void) {
  app_focus_service_unsubscribe();
  destroy_images();
  window_destroy(main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
/*
Future Ideas:
  Disappearing Blocks (like megaman)
  Falling Blocks (like Mario 3's "donut blocks")
  Stars (temporary invincibility)
  Fireballs (shooting)
  Enemies
  Pipes do nothing (except plants)
  Staircase
  Pits
  Waaay later: Underground level with easy ceiling but some holes and lifts, like level 1-2

================================================================================
This is the layout of the whole world.
  - means not visible (out of screen bounds, but still exists)
  o means a tile on the screen
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
 ================================================================================
*/