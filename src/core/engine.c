/*
 * Open Surge Engine
 * engine.c - game engine facade
 * Copyright (C) 2010, 2011, 2018  Alexandre Martins <alemartf(at)gmail(dot)com>
 * http://opensurge2d.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(A5BUILD)
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_native_dialog.h>
#else
#include <allegro.h>
#endif

#include <stdint.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include "engine.h"
#include "global.h"
#include "scene.h"
#include "storyboard.h"
#include "util.h"
#include "assetfs.h"
#include "resourcemanager.h"
#include "stringutil.h"
#include "logfile.h"
#include "video.h"
#include "audio.h"
#include "input.h"
#include "timer.h"
#include "fadefx.h"
#include "sprite.h"
#include "soundfactory.h"
#include "lang.h"
#include "screenshot.h"
#include "modmanager.h"
#include "prefs.h"
#include "commandline.h"
#include "font.h"
#include "fontext.h"
#include "nanoparser/nanoparser.h"
#include "../entities/legacy/nanocalc/nanocalc.h"
#include "../entities/legacy/nanocalc/nanocalc_addons.h"
#include "../entities/legacy/nanocalc/nanocalcext.h"
#include "../entities/actor.h"
#include "../entities/enemy.h"
#include "../entities/player.h"
#include "../entities/character.h"
#include "../scripting/scripting.h"
#include "../scenes/quest.h"
#include "../scenes/level.h"

#if defined(A5BUILD)
/* minimum Allegro version */
#define AL_MIN_MAJOR       5
#define AL_MIN_MINOR       2
#define AL_MIN_REVISION    0
#if ALLEGRO_VERSION_INT < ((AL_MIN_MAJOR << 24) | (AL_MIN_MINOR << 16) | (AL_MIN_REVISION << 8))
#error "This build requires a newer version of Allegro"
#endif
#endif

/* private stuff ;) */
static void clean_garbage();
static void init_basic_stuff(const commandline_t* cmd);
static void init_managers(const commandline_t* cmd);
static void init_accessories(const commandline_t* cmd);
static void init_game_data();
static void push_initial_scene(const commandline_t* cmd);
static void release_accessories();
static void release_managers();
static void release_basic_stuff();
static void init_nanoparser();
static void release_nanoparser();
static void parser_error(const char *msg);
static void parser_warning(const char *msg);
static void calc_error(const char *msg);
static const char* INTRO_QUEST = "quests/intro.qst";
static const char* SSAPP_LEVEL = "levels/surgescript.lev";

#if defined(A5BUILD)
/* public variables */
ALLEGRO_EVENT_QUEUE* a5_event_queue = NULL;
bool a5_key[ALLEGRO_KEY_MAX] = { false };
int a5_mouse_b = 0;
bool a5_display_active = true;
#endif



/* public functions */

/*
 * engine_init()
 * Initializes all the subsystems of
 * the game engine
 */
void engine_init(int argc, char **argv)
{
    commandline_t cmd = commandline_parse(argc, argv);
    init_basic_stuff(&cmd);
    init_managers(&cmd);
    init_accessories(&cmd);
    init_game_data();
    push_initial_scene(&cmd);
}


/*
 * engine_mainloop()
 * Game loop
 */
void engine_mainloop()
{
#if defined(A5BUILD)
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60.0);
    scene_t *current_scene = NULL;
    bool redraw = false;

    /* configure the timer */
    if(!timer)
        fatal_error("Can't create Allegro timer");
    al_register_event_source(a5_event_queue, al_get_timer_event_source(timer));
    al_start_timer(timer);

    /* main loop */
    while(!game_is_over() && !scenestack_empty()) {
        /* handle events */
        do {
            ALLEGRO_EVENT event;
            al_wait_for_event(a5_event_queue, &event);

            switch(event.type) {
                case ALLEGRO_EVENT_TIMER: {
                    /* update game logic */
                    ALLEGRO_EVENT next_event;

                    /* updating the managers */
                    timer_update();
                    input_update();
                    audio_update();
                    clean_garbage();

                    /* updating the current scene */
                    current_scene = scenestack_top();
                    current_scene->update();
                    redraw = true;

                    /* prevent locking */
                    while(al_peek_next_event(a5_event_queue, &next_event) && next_event.type == ALLEGRO_EVENT_TIMER && next_event.timer.source == event.timer.source)
                        al_drop_next_event(a5_event_queue);
                    break;
                }

                case ALLEGRO_EVENT_KEY_DOWN:
                    a5_key[event.keyboard.keycode] = true;
                    break;

                case ALLEGRO_EVENT_KEY_UP:
                    a5_key[event.keyboard.keycode] = false;
                    break;

                case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
                    a5_mouse_b |= 1 << (event.mouse.button - 1);
                    break;

                case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
                    a5_mouse_b &= ~(1 << (event.mouse.button - 1));
                    break;

                case ALLEGRO_EVENT_DISPLAY_SWITCH_IN:
                    a5_display_active = true;
                    break;

                case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
                    a5_display_active = false;
                    break;

                case ALLEGRO_EVENT_DISPLAY_CLOSE:
                    game_quit();
                    break;
            }
        } while(!al_is_event_queue_empty(a5_event_queue));

        /* render */
        if(redraw) {
            if(current_scene == scenestack_top())
                current_scene->render(); /* render after update */
            screenshot_update();
            fadefx_update();
            video_render();
            redraw = false;
        }
    }

    /* done */
    al_destroy_timer(timer);
#else
    scene_t *scn;

    while(!game_is_over() && !scenestack_empty()) {
        /* updating the managers */
        timer_update();
        input_update();
        audio_update();

        /* current scene: logic & rendering */
        scn = scenestack_top();
        scn->update();
        if(scn == scenestack_top()) /* scn may have been 'popped' out */
            scn->render();

        /* more rendering */
        screenshot_update();
        fadefx_update();
        video_render();

        /* calling the garbage collector */
        clean_garbage();
    }
#endif
}


/*
 * engine_release()
 * Releases the game engine and its
 * subsystems
 */
void engine_release()
{
    release_accessories();
    release_managers();
    release_basic_stuff();
}



/* private functions */

/*
 * clean_garbage()
 * Runs the garbage collector.
 */
void clean_garbage()
{
    static uint32_t last = 0;
    uint32_t t = timer_get_ticks();

    if(t >= last + 2000) { /* every 2 seconds */
        last = t;
        resourcemanager_release_unused_resources();
    }
}


/*
 * init_basic_stuff()
 * Initializes the basic stuff, such as Allegro.
 * Call this before everything else.
 */
void init_basic_stuff(const commandline_t* cmd)
{
    const char* gameid = commandline_getstring(cmd->gameid, NULL);
    const char* datadir = commandline_getstring(cmd->datadir, NULL);

#if defined(A5BUILD)
    srand(time(NULL));
    assetfs_init(gameid, datadir);
    logfile_init();
    init_nanoparser();

    /* initialize Allegro */
    logfile_message("Initializing Allegro 5...");
    
    if(!al_init())
        fatal_error("Can't initialize Allegro");

    if(NULL == (a5_event_queue = al_create_event_queue()))
        fatal_error("Can't create Allegro's event queue");

    /* --- */
    if(!al_install_keyboard())
        fatal_error("Can't initialize the keyboard");
    al_register_event_source(a5_event_queue, al_get_keyboard_event_source());

    if(!al_install_mouse())
        fatal_error("Can't initialize the mouse");
    al_register_event_source(a5_event_queue, al_get_mouse_event_source());

    if(!al_init_native_dialog_addon())
        fatal_error("Can't initialize Allegro's native dialog addon");

    if(!al_init_image_addon())
        fatal_error("Can't initialize Allegro's image addon");

    if(!al_init_primitives_addon())
        fatal_error("Can't initialize Allegro's primitives addon");

    if(!al_init_font_addon())
        fatal_error("Can't initialize Allegro's font addon");

    if(!al_init_ttf_addon())
        fatal_error("Can't initialize Allegro's TTF addon");

    if(!al_install_audio())
        fatal_error("Can't initialize Allegro's audio addon");

    if(!al_init_acodec_addon())
        fatal_error("Can't initialize Allegro's acodec addon");

    for(int samples = 8; samples > 0; samples /= 2) {
        if(al_reserve_samples(samples)) {
            logfile_message("Reserved %d samples", samples);
            break;
        }
        else
            logfile_message("Can't reserve %d samples", samples);
    }
#else
    set_uformat(U_UTF8);
    allegro_init();
    srand(time(NULL));
    assetfs_init(gameid, datadir);
    logfile_init();
    init_nanoparser();
#endif
}


/*
 * init_managers()
 * Initializes the managers
 */
void init_managers(const commandline_t* cmd)
{
    prefs_t* prefs;

    modmanager_init();
    prefs = modmanager_prefs();

    timer_init();
    video_init(
        commandline_getint(
            cmd->video_resolution,
            prefs_has_item(prefs, ".resolution") ? prefs_get_int(prefs, ".resolution") : VIDEORESOLUTION_2X
        ),
        commandline_getint(cmd->smooth_graphics, prefs_get_bool(prefs, ".smoothgfx")),
        commandline_getint(cmd->fullscreen, prefs_get_bool(prefs, ".fullscreen")),
        commandline_getint(cmd->color_depth, video_get_preferred_color_depth())
    );
    video_show_fps(
        commandline_getint(cmd->show_fps, prefs_get_bool(prefs, ".showfps"))
    );
    audio_init();
    input_init();
    input_ignore_joystick(
        !commandline_getint(cmd->use_gamepad, prefs_get_bool(prefs, ".gamepad"))
    );
    resourcemanager_init();
}


/*
 * init_accessories()
 * Initializes the accessories
 */
void init_accessories(const commandline_t* cmd)
{
    prefs_t* prefs = modmanager_prefs();
    const char* custom_lang = commandline_getstring(cmd->language_filepath,
        prefs_has_item(prefs, ".langpath") ? prefs_get_string(prefs, ".langpath") : NULL
    );

    setlocale(LC_ALL, "en_US.UTF-8"); /* work with UTF-8 */
    setlocale(LC_NUMERIC, "C"); /* use '.' as the decimal separator on atof() */
    video_display_loading_screen();
    sprite_init();
    font_init(commandline_getint(cmd->allow_font_smoothing, TRUE));
    fontext_register_variables();
    soundfactory_init();
    charactersystem_init();
    objects_init();
    scripting_init(cmd->user_argc, cmd->user_argv);
    storyboard_init();
    screenshot_init();
    fadefx_init();
    lang_init();
    if(custom_lang && *custom_lang)
        lang_loadfile(custom_lang);
    
    scenestack_init();
}

/*
 * init_game_data()
 * Initializes the game data
 */
void init_game_data()
{
    player_set_lives(PLAYER_INITIAL_LIVES);
    player_set_score(0);
}


/*
 * push_initial_scene()
 * Decides which scene should be pushed into the scene stack
 */
void push_initial_scene(const commandline_t* cmd)
{
    int custom_level = (commandline_getstring(cmd->custom_level_path, NULL) != NULL);
    int custom_quest = (commandline_getstring(cmd->custom_quest_path, NULL) != NULL);

    if(custom_level) {
        scenestack_push(storyboard_get_scene(SCENE_LEVEL), (void*)(commandline_getstring(cmd->custom_level_path, "")));
    }
    else if(custom_quest) {
        scenestack_push(storyboard_get_scene(SCENE_QUEST), (void*)(commandline_getstring(cmd->custom_quest_path, "")));
        scenestack_push(storyboard_get_scene(SCENE_INTRO), NULL);
    }
    else if(scripting_testmode()) {
        scenestack_push(storyboard_get_scene(SCENE_LEVEL), (void*)(SSAPP_LEVEL));
        /*scenestack_push(storyboard_get_scene(SCENE_INTRO), NULL);*/
    }
    else {
        scenestack_push(storyboard_get_scene(SCENE_QUEST), (void*)(INTRO_QUEST));
        scenestack_push(storyboard_get_scene(SCENE_INTRO), NULL);
        if(!prefs_has_item(modmanager_prefs(), ".langpath"))
            scenestack_push(storyboard_get_scene(SCENE_LANGSELECT), NULL);
    }
}


/*
 * release_accessories()
 * Releases the previously loaded accessories
 */
void release_accessories()
{
    scenestack_release();
    storyboard_release();
    lang_release();
    fadefx_release();
    screenshot_release();
    scripting_release();
    objects_release();
    charactersystem_release();
    soundfactory_release();
    font_release();
    sprite_release();
}

/*
 * release_managers()
 * Releases the previously loaded managers
 */
void release_managers()
{
    modmanager_release();
    input_release();
    video_release();
    resourcemanager_release();
    audio_release();
    timer_release();
}


/*
 * release_basic_stuff()
 * Releases the basic stuff, such as Allegro.
 * Call this after everything else.
 */
void release_basic_stuff()
{
    release_nanoparser();
    logfile_release();
    assetfs_release();
#if defined(A5BUILD)
    /* Release Allegro */
    al_destroy_event_queue(a5_event_queue);
    a5_event_queue = NULL;
#else
    allegro_exit();
#endif
}

/*
 * init_nanoparser()
 * Initializes nanoparser
 */
void init_nanoparser()
{
    /* nanoparser */
    nanoparser_set_error_function(parser_error);
    nanoparser_set_warning_function(parser_warning);

    /* nanocalc */
    nanocalc_init(); /* initializes a basic nanocalc */
    nanocalc_set_error_function(calc_error); /* error callback */
    nanocalc_addons_init(); /* adds some mathematical functions to nanocalc */
    nanocalcext_register_bifs(); /* more bindings */
}

/*
 * release_nanoparser()
 * Releases nanoparser
 */
void release_nanoparser()
{
    /* nanoparser */
    ; /* empty */

    /* nanocalc */
    nanocalc_addons_release();
    nanocalc_release();
}

/*
 * parser_error()
 * This is called by nanoparser when an error is raised
 */
void parser_error(const char *msg)
{
    fatal_error("%s", msg);
}

/*
 * parser_warning()
 * This is called by nanoparser when a warning is raised
 */
void parser_warning(const char *msg)
{
    logfile_message("WARNING: %s", msg);
}

/*
 * calc_error()
 * This is called by nanocalc when an error is raised
 */
void calc_error(const char *msg)
{
    fatal_error("%s", msg);
}
