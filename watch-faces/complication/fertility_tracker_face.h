/*
 * MIT License
 *
 * Copyright (c) 2024 Gregory Evans
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef FERTILITY_TRACKER_FACE_H_
#define FERTILITY_TRACKER_FACE_H_

#include "movement.h"

/*
 * A DESCRIPTION OF YOUR WATCH FACE
 *
 * and a description of how use it
 *
 */

#define MEMORY_NUM_DAYS 31 //Number of days to store data

enum fertility_face_state_t {CALENDAR, FLUID_ENTRY, TEMP_ENTRY_1, TEMP_ENTRY_2, TEMP_ENTRY_3, TEMP_ENTRY_4, CONFIRM_ENTRY, STATE_ERROR};
enum cycle_state_t{FLUID_CHANGE, ESTROGEN, TEMP_SHIFT_DETECT, TEMP_SHIFT_DETECT_EXTEND, SEEKING_OVULATION, OVULATION_CONFIRMED, ERRATIC_TEMPS, ERROR};
enum face_action_t{SAVE, DELETE, LOAD, RESET};

typedef struct {
    uint8_t fluid_buf[MEMORY_NUM_DAYS];
    float temp_buf[MEMORY_NUM_DAYS];
    watch_date_time_t time_buf[MEMORY_NUM_DAYS];
    uint8_t fluid_input;
    uint8_t temp_input[4]; //Integer digits 
    watch_date_time_t time_input;
    enum face_action_t face_action;
    uint16_t data_index;
    enum fertility_face_state_t state;
    enum cycle_state_t cycle_next_state;
    enum cycle_state_t cycle_state;
    enum cycle_state_t cycle_prev_state;
    watch_date_time_t cycle_state_start;
    float historic_max_temp_f;
    watch_date_time_t current_date;
    float * first_high_temp;
    uint8_t cycle_day_num;
    
} fertility_tracker_state_t;

#define FERTILITY_TRACKER_MEM_SIZE_BYTES 320

void fertility_tracker_face_setup(uint8_t watch_face_index, void ** context_ptr);
void fertility_tracker_face_activate(void *context);
bool fertility_tracker_face_loop(movement_event_t event, void *context);
void fertility_tracker_face_resign(void *context);

#define fertility_tracker_face ((const watch_face_t){ \
    fertility_tracker_face_setup, \
    fertility_tracker_face_activate, \
    fertility_tracker_face_loop, \
    fertility_tracker_face_resign, \
    NULL, \
})

static void display_fluid_type(uint8_t value);
static bool dates_are_equal(watch_date_time_t time1, watch_date_time_t time2);
static bool is_fertile(fertility_tracker_state_t * data_buf);
static enum cycle_state_t iterate_cycle_fsm(fertility_tracker_state_t * data_buf);
static uint16_t num_days_passed(watch_date_time_t date1, watch_date_time_t date2);
static void enter_error_state(fertility_tracker_state_t * data_buf);
static float get_prev_temp(uint16_t num_days_prev, fertility_tracker_state_t * data_buf);
static uint8_t get_prev_fluid(uint16_t num_days_prev, fertility_tracker_state_t * data_buf);
static float get_historic_max_temp_f(fertility_tracker_state_t * data_buf);
static bool detect_ovulation(fertility_tracker_state_t * data_buf);
static bool save_face_buf(fertility_tracker_state_t * data_buf);
static bool restore_face_buf(fertility_tracker_state_t * data_buf);
static bool reset_face_buf(fertility_tracker_state_t * data_buf);
static void save_debug_data(fertility_tracker_state_t * data_buf);

#endif //FERTILITY_TRACKER_FACE_H_