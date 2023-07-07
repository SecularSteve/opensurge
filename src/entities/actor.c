/*
 * Open Surge Engine
 * actor.c - actor module
 * Copyright (C) 2008-2023  Alexandre Martins <alemartf@gmail.com>
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

#include <limits.h>
#include <math.h>
#include "actor.h"
#include "brick.h"
#include "../util/numeric.h"
#include "../util/util.h"
#include "../core/global.h"
#include "../core/input.h"
#include "../core/logfile.h"
#include "../core/video.h"
#include "../core/timer.h"
#include "../physics/obstacle.h"


/* private stuff */
static void update_animation(actor_t *act);
static bool can_be_clipped_out(const actor_t* act, v2d_t topleft);


/*
 * actor_create()
 * Creates an actor
 */
actor_t* actor_create()
{
    actor_t *act = mallocx(sizeof *act);

    act->spawn_point = v2d_new(0, 0);
    act->position = act->spawn_point;
    act->angle = 0.0f;
    act->speed = v2d_new(0,0);
    act->input = NULL;

    act->animation = NULL;
    act->next_animation = NULL;
    act->animation_frame = 0.0f;
    act->animation_speed_factor = 1.0f;
    act->synchronized_animation = false;
    act->mirror = IF_NONE;
    act->visible = true;
    act->alpha = 1.0f;
    act->hot_spot = v2d_new(0, 0);
    act->scale = v2d_new(1.0f, 1.0f);

    return act;
}


/*
 * actor_destroy()
 * Destroys an actor
 */
void actor_destroy(actor_t *act)
{
    if(act->input != NULL)
        input_destroy(act->input);

    free(act);
}


/*
 * actor_render()
 * Default rendering function
 */
void actor_render(actor_t *act, v2d_t camera_position)
{
    if(act->visible && act->animation != NULL) {
        const image_t* img = actor_image(act);
        v2d_t topleft = v2d_subtract(camera_position, v2d_multiply(video_get_screen_size(), 0.5f));

        /* update animation */
        update_animation(act);

        /* render */
        if(!nearly_zero(act->angle)) {
            if(!nearly_equal(act->scale.x, 1.0f) || !nearly_equal(act->scale.y, 1.0f)) {
                if(!nearly_equal(act->alpha, 1.0f))
                    image_draw_scaled_rotated_trans(img, (int)(act->position.x - topleft.x), (int)(act->position.y - topleft.y), (int)act->hot_spot.x, (int)act->hot_spot.y, act->scale, act->angle, act->alpha, act->mirror);
                else
                    image_draw_scaled_rotated(img, (int)(act->position.x - topleft.x), (int)(act->position.y - topleft.y), (int)act->hot_spot.x, (int)act->hot_spot.y, act->scale, act->angle, act->mirror);
            }
            else {
                if(!nearly_equal(act->alpha, 1.0f))
                    image_draw_rotated_trans(img, (int)(act->position.x - topleft.x), (int)(act->position.y - topleft.y), (int)act->hot_spot.x, (int)act->hot_spot.y, act->angle, act->alpha, act->mirror);
                else
                    image_draw_rotated(img, (int)(act->position.x - topleft.x), (int)(act->position.y - topleft.y), (int)act->hot_spot.x, (int)act->hot_spot.y, act->angle, act->mirror);
            }
        }
        else if(!nearly_equal(act->scale.x, 1.0f) || !nearly_equal(act->scale.y, 1.0f)) {
            if(!nearly_equal(act->alpha, 1.0f))
                image_draw_scaled_trans(img, (int)(act->position.x - act->hot_spot.x*act->scale.x - topleft.x), (int)(act->position.y - act->hot_spot.y*act->scale.y - topleft.y), act->scale, act->alpha, act->mirror);
            else
                image_draw_scaled(img, (int)(act->position.x - act->hot_spot.x*act->scale.x - topleft.x), (int)(act->position.y - act->hot_spot.y*act->scale.y - topleft.y), act->scale, act->mirror);
        }
        else if(!can_be_clipped_out(act, topleft)) {
            if(!nearly_equal(act->alpha, 1.0f))
                image_draw_trans(img, (int)(act->position.x - act->hot_spot.x - topleft.x), (int)(act->position.y - act->hot_spot.y - topleft.y), act->alpha, act->mirror);
            else
                image_draw(img, (int)(act->position.x - act->hot_spot.x - topleft.x), (int)(act->position.y - act->hot_spot.y - topleft.y), act->mirror);
        }
    }
}



/*
 * actor_render_repeat_xy()
 * Tiled rendering
 */
void actor_render_repeat_xy(actor_t *act, v2d_t camera_position, bool repeat_x, bool repeat_y)
{
    if(act->visible && act->animation != NULL) {
        const image_t *img = actor_image(act);
        int w = image_width(img);
        int h = image_height(img);
        v2d_t topleft = v2d_subtract(camera_position, v2d_multiply(video_get_screen_size(), 0.5f));

        /* update animation */
        update_animation(act);

        /* compute the position of the top-left tile */
        v2d_t final_pos;
        final_pos.x = ((int)act->position.x % (repeat_x ? w : INT_MAX)) - act->hot_spot.x - topleft.x - (repeat_x ? w : 0);
        final_pos.y = ((int)act->position.y % (repeat_y ? h : INT_MAX)) - act->hot_spot.y - topleft.y - (repeat_y ? h : 0);

        /* render cols x rows tiles */
        int cols = repeat_x ? (VIDEO_SCREEN_W / w + 3) : 1;
        int rows = repeat_y ? (VIDEO_SCREEN_H / h + 3) : 1;
        for(int i = 0; i < cols; i++) {
            for(int j = 0; j < rows; j++)
                image_draw(img, (int)final_pos.x + i*w, (int)final_pos.y + j*h, act->mirror);
        }
    }
}


/*
 * actor_change_animation()
 * Changes the animation of an actor
 */
void actor_change_animation(actor_t *act, const animation_t *anim)
{
    /* no need to change */
    if(act->animation == anim || anim == NULL)
        return;

    /* are we playing a transition to anim? If so,
       we wait until the end of the transition,
       unless anim is also a transition (in
       which case we change the animation) */
    if(act->animation != NULL && act->animation->next == anim) {
        if(!(animation_is_transition(anim) || actor_animation_finished(act)))
            return;
    }

    /* is there a transition from act->animation to anim? */
    const animation_t* transition = animation_get_transition(act->animation, anim);
    if(transition != NULL) {
        act->next_animation = anim; /* anim may not be act->animation->next, as this may be a transition to "any" animation */
        anim = transition; /* change to the transition animation */
    }

    /* change & reset the animation */
    act->animation = anim;
    act->hot_spot = anim->hot_spot;
    act->animation_frame = 0.0f;
    act->animation_speed_factor = 1.0f;
}



/*
 * actor_change_animation_frame()
 * Changes the animation frame
 */
void actor_change_animation_frame(actor_t *act, int frame)
{
    act->animation_frame = clip(frame, 0, act->animation->frame_count - 1);
}



/*
 * actor_change_animation_speed_factor()
 * Changes the speed factor of the current animation
 * The default factor is 1.0 (i.e., 100% of the original
 * animation speed)
 */
void actor_change_animation_speed_factor(actor_t *act, float factor)
{
    act->animation_speed_factor = max(0.0f, factor);
}



/*
 * actor_animation_finished()
 * Returns true if the current animation has finished
 */
bool actor_animation_finished(const actor_t *act)
{
    if(!act->animation)
        return false;

    float frame = act->animation_frame + (act->animation->fps * act->animation_speed_factor) * timer_get_delta();
    return (!act->animation->repeat && (int)frame >= act->animation->frame_count);
}



/*
 * actor_is_transition_animation_playing()
 * Returns true if a transition animation is playing
 */
bool actor_is_transition_animation_playing(const actor_t *act)
{
    if(!act->animation)
        return false;

    return animation_is_transition(act->animation);
}



/*
 * actor_synchronize_animation()
 * should I use a shared animation frame?
 */
void actor_synchronize_animation(actor_t *act, bool sync)
{
    act->synchronized_animation = sync;
}


/*
 * actor_animation_frame()
 * The current frame of the animation, in [0, frame_count - 1]
 */
int actor_animation_frame(const actor_t* act)
{
    return (int)(act->animation_frame);
}


/*
 * actor_image()
 * Returns the current image of the
 * animation of this actor
 */
const image_t* actor_image(const actor_t *act)
{
    if(!act->animation)
        fatal_error("actor_image(): no animation is playing");

    return animation_get_image(act->animation, (int)(act->animation_frame));
}

/*
 * actor_action_spot()
 * The action spot of the current animation, appropriately flipped
 */
v2d_t actor_action_spot(const actor_t* act)
{
    if(!act->animation)
        return v2d_new(0, 0);

    v2d_t hot_spot = act->animation->hot_spot;
    v2d_t offset = v2d_subtract(act->animation->action_spot, hot_spot);
    v2d_t sign = v2d_new(
        act->mirror & IF_HFLIP ? -1.0f : 1.0f,
        act->mirror & IF_VFLIP ? -1.0f : 1.0f
    );

    /* flip the action spot relative to the hot spot */
    return v2d_add(hot_spot, v2d_compmult(offset, sign));
}

/*
 * actor_action_offset()
 * An offset that, when added to the position of the actor in space, results
 * in the position of the (appropriately flipped) action spot in space.
 */
v2d_t actor_action_offset(const actor_t* act)
{
    return v2d_subtract(actor_action_spot(act), act->hot_spot);
}


/* private stuff */

/* this logic updates the animation of an actor */
void update_animation(actor_t *act)
{
    if(act->animation == NULL)
        return;

    if(!(act->synchronized_animation) || !(act->animation->repeat)) {
        /* the animation isn't synchronized: every object updates its animation at its own pace */
        act->animation_frame += (act->animation->fps * act->animation_speed_factor) * timer_get_delta();
        if((int)act->animation_frame >= act->animation->frame_count) {
            /* the animation has finished playing */
            if(act->animation->repeat) {
                /* repeat the animation */
                act->animation_frame = (((int)act->animation_frame % act->animation->frame_count) + act->animation->repeat_from) % act->animation->frame_count;
            }
            else {
                /* the animation has ended */
                act->animation_frame = act->animation->frame_count - 1;

                /* is the current animation a transition? */
                if(act->next_animation != NULL) {
                    actor_change_animation(act, act->next_animation);
                    act->next_animation = NULL;
                }
            }
        }
    }
    else {
        /* the animation is synchronized: this only makes sense if the animation does repeat */
        act->animation_frame = (act->animation->fps * act->animation_speed_factor) * (0.001f * timer_get_ticks());
        act->animation_frame = (((int)act->animation_frame % act->animation->frame_count) + act->animation->repeat_from) % act->animation->frame_count;
    }
}

/* Checks if the actor can be clipped out (rendering) */
bool can_be_clipped_out(const actor_t* act, v2d_t topleft)
{
    int x = (int)(act->position.x - act->hot_spot.x - topleft.x);
    int y = (int)(act->position.y - act->hot_spot.y - topleft.y);

    const image_t* img = actor_image(act);
    int w = image_width(img);
    int h = image_height(img);

    const image_t* backbuffer = video_get_backbuffer();
    int sw = image_width(backbuffer);
    int sh = image_height(backbuffer);

    return (x + w <= 0 || x >= sw || y + h <= 0 || y >= sh);
}




/* ===========================================
                 legacy code
   =========================================== */
static const float MAGIC_DIFF = -2;  /* platform movement & collision detectors magic */
static brick_t* brick_at(const brick_list_t *list, v2d_t spot);
static void calculate_rotated_boundingbox(const actor_t *act, v2d_t spot[4]);
static void sensors_ex(actor_t *act, v2d_t vup, v2d_t vupright, v2d_t vright, v2d_t vdownright, v2d_t vdown, v2d_t vdownleft, v2d_t vleft, v2d_t vupleft, struct brick_list_t *brick_list, struct brick_t **up, struct brick_t **upright, struct brick_t **right, struct brick_t **downright, struct brick_t **down, struct brick_t **downleft, struct brick_t **left, struct brick_t **upleft);



/*
 * actor_collision()
 * Check if there is collision between actors
 */
int actor_collision(const actor_t *a, const actor_t *b)
{
    v2d_t a_pos, b_pos;
    v2d_t a_size, b_size;
    v2d_t a_spot[4], b_spot[4]; /* rotated spots */

    calculate_rotated_boundingbox(a, a_spot);
    calculate_rotated_boundingbox(b, b_spot);

    a_pos.x = min(a_spot[0].x, min(a_spot[1].x, min(a_spot[2].x, a_spot[3].x)));
    a_pos.y = min(a_spot[0].y, min(a_spot[1].y, min(a_spot[2].y, a_spot[3].y)));
    b_pos.x = min(b_spot[0].x, min(b_spot[1].x, min(b_spot[2].x, b_spot[3].x)));
    b_pos.y = min(b_spot[0].y, min(b_spot[1].y, min(b_spot[2].y, b_spot[3].y)));

    a_size.x = max(a_spot[0].x, max(a_spot[1].x, max(a_spot[2].x, a_spot[3].x))) - a_pos.x;
    a_size.y = max(a_spot[0].y, max(a_spot[1].y, max(a_spot[2].y, a_spot[3].y))) - a_pos.y;
    b_size.x = max(b_spot[0].x, max(b_spot[1].x, max(b_spot[2].x, b_spot[3].x))) - b_pos.x;
    b_size.y = max(b_spot[0].y, max(b_spot[1].y, max(b_spot[2].y, b_spot[3].y))) - b_pos.y;

    return (
        (a_pos.x + a_size.x >= b_pos.x && a_pos.x <= b_pos.x + b_size.x) &&
        (a_pos.y + a_size.y >= b_pos.y && a_pos.y <= b_pos.y + b_size.y)
    );
}


/*
 * actor_brick_collision()
 * Actor collided with a brick?
 */
int actor_brick_collision(const actor_t *act, const brick_t *brk)
{
    v2d_t actor_topleft = v2d_subtract(act->position, v2d_rotate(act->hot_spot, act->angle));
    v2d_t actor_bottomright = v2d_add( actor_topleft , v2d_rotate(v2d_new(image_width(actor_image(act)), image_height(actor_image(act))), act->angle) );
    v2d_t brick_topleft = brick_position(brk);
    v2d_t brick_bottomright = v2d_add( brick_topleft, brick_size(brk) );
    float a[4] = { actor_topleft.x , actor_topleft.y , actor_bottomright.x , actor_bottomright.y };
    float b[4] = { brick_topleft.x , brick_topleft.y , brick_bottomright.x , brick_bottomright.y };
    return bounding_box(a,b);
}



/*
 * actor_sensors()
 * Get obstacle bricks around the actor
 */
void actor_sensors(actor_t *act, brick_list_t *brick_list, brick_t **up, brick_t **upright, brick_t **right, brick_t **downright, brick_t **down, brick_t **downleft, brick_t **left, brick_t **upleft)
{
    static const float SIDE_CORNERS_HEIGHT = 0.5f; /* height of the left/right sensors */
    int frame_width = image_width(actor_image(act));
    int frame_height = image_height(actor_image(act));

    v2d_t feet       = v2d_add(v2d_subtract(act->position, act->hot_spot), v2d_new(frame_width/2, frame_height));
    v2d_t vup        = v2d_add ( feet , v2d_rotate( v2d_new(0, -frame_height+MAGIC_DIFF), -act->angle) );
    v2d_t vdown      = v2d_add ( feet , v2d_rotate( v2d_new(0, -MAGIC_DIFF), -act->angle) ); 
    v2d_t vleft      = v2d_add ( feet , v2d_rotate( v2d_new(-frame_width/2+MAGIC_DIFF, -frame_height*SIDE_CORNERS_HEIGHT), -act->angle) );
    v2d_t vright     = v2d_add ( feet , v2d_rotate( v2d_new(frame_width/2-MAGIC_DIFF, -frame_height*SIDE_CORNERS_HEIGHT), -act->angle) );
    v2d_t vupleft    = v2d_add ( feet , v2d_rotate( v2d_new(-frame_width/2+MAGIC_DIFF, -frame_height+MAGIC_DIFF), -act->angle) );
    v2d_t vupright   = v2d_add ( feet , v2d_rotate( v2d_new(frame_width/2-MAGIC_DIFF, -frame_height+MAGIC_DIFF), -act->angle) );
    v2d_t vdownleft  = v2d_add ( feet , v2d_rotate( v2d_new(-frame_width/2+MAGIC_DIFF, -MAGIC_DIFF), -act->angle) );
    v2d_t vdownright = v2d_add ( feet , v2d_rotate( v2d_new(frame_width/2-MAGIC_DIFF, -MAGIC_DIFF), -act->angle) );

    sensors_ex(act, vup, vupright, vright, vdownright, vdown, vdownleft, vleft, vupleft, brick_list, up, upright, right, downright, down, downleft, left, upleft);
}


/*
 * actor_brick_at()
 * Gets a brick at a certain offset (may return NULL)
 */
const brick_t* actor_brick_at(actor_t *act, const brick_list_t *brick_list, v2d_t offset)
{
    return brick_at(brick_list, v2d_add(act->position, offset));
}


/* private stuff */

/* brick_at(): given a list of bricks, returns
 * one that collides with the given spot
 * PS: this code ignores the bricks that are
 * not obstacles */
/* NOTE: this is old (deprecated) code -- see obstaclemap.c */
brick_t* brick_at(const brick_list_t *list, v2d_t spot)
{
    const brick_list_t *p;
    const obstacle_t* obstacle;
    brick_t *ret = NULL;

    /* main algorithm */
    for(p=list; p; p=p->next) {

        /* ignore passable bricks */
        if(brick_type(p->data) == BRK_PASSABLE)
            continue;

        /* I don't want clouds. */
        if(brick_type(p->data) == BRK_CLOUD && (ret && brick_type(ret) == BRK_SOLID))
            continue;

        /* I don't want moving platforms */
        if(brick_behavior(p->data) == BRB_CIRCULAR && (ret && brick_behavior(ret) != BRB_CIRCULAR) && brick_position(p->data).y >= brick_position(ret).y)
            continue;
            
        /* Check for collision */
        if(NULL != (obstacle = brick_obstacle(p->data))) {
            if(obstacle_got_collision(obstacle, spot.x, spot.y, spot.x, spot.y)) {
                if(brick_behavior(p->data) != BRB_CIRCULAR && (ret && brick_behavior(ret) == BRB_CIRCULAR) && brick_position(p->data).y <= brick_position(ret).y) {
                    ret = p->data; /* No moving platforms. Let's grab a regular platform instead. */
                }
                else if(brick_type(p->data) == BRK_SOLID && (ret && brick_type(ret) == BRK_CLOUD)) {
                    ret = p->data; /* No clouds. Let's grab an obstacle instead. */
                }
                else if(brick_type(p->data) == BRK_CLOUD && (ret && brick_type(ret) == BRK_CLOUD)) {
                    if(brick_position(p->data).y > brick_position(ret).y) /* two conflicting clouds */
                        ret = p->data;
                }
                else if(!ret)
                    ret = p->data;
            }
        }
    }

    return ret;
}

/*
 * calculate_rotated_boundingbox()
 * Calculates the rotated bounding box of a given actor
 */
void calculate_rotated_boundingbox(const actor_t *act, v2d_t spot[4])
{
    float w, h, angle;
    v2d_t a, b, c, d, hs;
    v2d_t pos;

    angle = -act->angle;
    w = image_width(actor_image(act));
    h = image_height(actor_image(act));
    hs = act->hot_spot;
    pos = act->position;

    a = v2d_subtract(v2d_new(0, 0), hs);
    b = v2d_subtract(v2d_new(w, 0), hs);
    c = v2d_subtract(v2d_new(w, h), hs);
    d = v2d_subtract(v2d_new(0, h), hs);

    spot[0] = v2d_add(pos, v2d_rotate(a, angle));
    spot[1] = v2d_add(pos, v2d_rotate(b, angle));
    spot[2] = v2d_add(pos, v2d_rotate(c, angle));
    spot[3] = v2d_add(pos, v2d_rotate(d, angle));
}

/*
 * sensors_ex()
 * Like actor_sensors(), but this procedure allows to specify the
 * collision detectors positions'
 */
void sensors_ex(actor_t *act, v2d_t vup, v2d_t vupright, v2d_t vright, v2d_t vdownright, v2d_t vdown, v2d_t vdownleft, v2d_t vleft, v2d_t vupleft, brick_list_t *brick_list, brick_t **up, brick_t **upright, brick_t **right, brick_t **downright, brick_t **down, brick_t **downleft, brick_t **left, brick_t **upleft)
{
    brick_t **cloud_off[5] = { up, upright, right, left, upleft };
    int i;

    if(up) *up = brick_at(brick_list, vup);
    if(down) *down = brick_at(brick_list, vdown);
    if(left) *left = brick_at(brick_list, vleft);
    if(right) *right = brick_at(brick_list, vright);
    if(upleft) *upleft = brick_at(brick_list, vupleft);
    if(upright) *upright = brick_at(brick_list, vupright);
    if(downleft) *downleft = brick_at(brick_list, vdownleft);
    if(downright) *downright = brick_at(brick_list, vdownright);


    /* handle clouds */

    /* bricks: laterals and top */
    for(i=0; i<5; i++) {
        /* forget bricks */
        brick_t **brk = cloud_off[i];
        if(brk && *brk && brick_type(*brk) == BRK_CLOUD)
            *brk = NULL;
    }

    /* bricks: down, downleft, downright */
    if(down && *down && brick_type(*down) == BRK_CLOUD) {
        float offset = min(15, brick_size(*down).y / 3);
        if(!(act->speed.y >= 0 && act->position.y < (brick_position(*down).y + MAGIC_DIFF + 1) + offset)) {
            /* forget bricks */
            if(downleft && *downleft == *down)
                *downleft = NULL;
            if(downright && *downright == *down)
                *downright = NULL;
            *down = NULL;
        }
    }
}

