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

#include "fertility_tracker_face.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "watch_utility.h"
#include "filesystem.h"

#define SAVE_FILENAME "fertility_face_data.bin"
#define DEBUG_FILENAME "fert_debug_data.txt"
#define DEBUG_FILE_MAX_LINE_SIZE 15

#define NUM_EE_EL_SEARCH_DAYS 10
#define INVALID_TEMP 90.00f
#define FEVER_TEMP 100.40f
#define MAX_MISSED_DAYS 14

void fertility_tracker_face_setup(uint8_t watch_face_index, void ** context_ptr)
{
    if(*context_ptr == NULL)
    {
        *context_ptr = malloc(sizeof(fertility_tracker_state_t));
        memset(*context_ptr, 0, sizeof(fertility_tracker_state_t));
    } 
    ((fertility_tracker_state_t*)*context_ptr)->state = CALENDAR;
    ((fertility_tracker_state_t*)*context_ptr)->current_date = watch_rtc_get_date_time();
}

void fertility_tracker_face_activate(void *context)
{
    (void) context;
}

bool fertility_tracker_face_loop(movement_event_t event, void *context)
{
    fertility_tracker_state_t *face_buf= (fertility_tracker_state_t *) context;
    watch_date_time_t temp_time;
    char buf[6];
    uint16_t num_days_missed;
    uint16_t i;
    static uint8_t display_flash = 0;
    
    switch(event.event_type)
    {
        case EVENT_ACTIVATE:

            //Update state if a new day has arrived
            temp_time = watch_rtc_get_date_time();
            num_days_missed = num_days_passed(temp_time, face_buf->current_date);

            if(num_days_missed >= MAX_MISSED_DAYS)
            {
                enter_error_state(face_buf);
                face_buf->current_date = temp_time;
                save_debug_data(face_buf); //Save debug data before entering error state
            }
            else if(num_days_missed >= 1)
            {
                //Log missed days 
                //Also, pre-populate today with invalid data in case it is missed
                for(i = 0; i < num_days_missed; i++)
                {
                    save_debug_data(face_buf);
                    face_buf->data_index++;                                    
                    if(face_buf->data_index >= MEMORY_NUM_DAYS)
                    {
                        face_buf->data_index = 0;
                    }
                    face_buf->fluid_buf[face_buf->data_index] = 0; 
                    face_buf->temp_buf[face_buf->data_index] = INVALID_TEMP;
                    face_buf->cycle_day_num++;
                }

                face_buf->current_date = temp_time;
                
                //Update the state based on next state
                if(face_buf->cycle_state != face_buf->cycle_next_state) 
                {
                    if(face_buf->cycle_next_state == FLUID_CHANGE)
                    {
                        //Second day of cycle because M was logged yesterday.
                        face_buf->cycle_day_num = 2;
                    }
                    if(face_buf->cycle_state == TEMP_SHIFT_DETECT && face_buf->cycle_next_state == ESTROGEN)
                    {
                        *(face_buf->first_high_temp) = INVALID_TEMP;
                    }
                    face_buf->cycle_prev_state = face_buf->cycle_state;
                    face_buf->cycle_state = face_buf->cycle_next_state;
                    face_buf->cycle_state_start = temp_time;
                }
            }

            watch_display_text(WATCH_POSITION_FULL, "          ");

            if(is_fertile(face_buf))
            {
                //watch_display_string(" FERT ",4);
                watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "FERT  ", " FERT ");
            }
            else
            {
                //watch_display_string("NFERT ",4);
                watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "nFERT ", "NFERT ");

            }

            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "DAY", "  ");
            //watch_display_string("  ",0);
            snprintf(buf, sizeof(buf), "%2hu", face_buf->cycle_day_num);
            //watch_display_string(buf, 2);
            watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
            break;

        case EVENT_MODE_LONG_PRESS: 
            face_buf->state = CALENDAR;
            movement_move_to_face(0);
            break;

        case EVENT_MODE_BUTTON_UP:
            face_buf->state = CALENDAR;
            movement_move_to_next_face();
            break;

        case EVENT_TIMEOUT:
            face_buf->state = CALENDAR;
            movement_move_to_face(0);
            break;

        case EVENT_LIGHT_BUTTON_DOWN:
            
            switch(face_buf->state)
            {
                case CALENDAR:
                case FLUID_ENTRY:
                case TEMP_ENTRY_1:
                case TEMP_ENTRY_2:
                case TEMP_ENTRY_3:
                case TEMP_ENTRY_4: 
                case CONFIRM_ENTRY:
                case STATE_ERROR:
                    movement_illuminate_led();
                    break;

                default:
                    break;
            }
            break;

        case EVENT_LIGHT_BUTTON_UP:

            switch(face_buf->state)
            {
                case CALENDAR:
                break;

                case FLUID_ENTRY:
                    //watch_display_string(" T",0);
                    watch_display_text_with_fallback(WATCH_POSITION_TOP, "TEMP  ", "T");
                    snprintf(buf, sizeof(buf), "%2hu%hu%hu%hu", (uint8_t)(face_buf->temp_input[0]/10), face_buf->temp_input[1], face_buf->temp_input[2], face_buf->temp_input[3]);
                    //watch_display_string(buf, 4);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    face_buf->state = TEMP_ENTRY_1;
                    break;

                case TEMP_ENTRY_1:
                    face_buf->state = TEMP_ENTRY_2;
                    break;

                case TEMP_ENTRY_2:
                    face_buf->state = TEMP_ENTRY_3;
                    break;

                case TEMP_ENTRY_3:
                    face_buf->state = TEMP_ENTRY_4;
                    break;

                case TEMP_ENTRY_4: 
                    //watch_display_string("SA",0);
                    watch_display_text_with_fallback(WATCH_POSITION_TOP, "SAVE  ", "SA");
                    face_buf->face_action = SAVE;
                    //watch_display_string(" SAVE", 4);
                    watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "SAVE  ", "SAVE");
                    face_buf->state = CONFIRM_ENTRY;
                break;

                case CONFIRM_ENTRY:
                    face_buf->state = CALENDAR;

                    if(face_buf->face_action == SAVE)
                    {
                        face_buf->fluid_buf[face_buf->data_index] = face_buf->fluid_input;
                        face_buf->temp_buf[face_buf->data_index] = face_buf->temp_input[0] + face_buf->temp_input[1] + (0.1 * face_buf->temp_input[2]) + (0.01 * face_buf->temp_input[3]);
                        face_buf->time_buf[face_buf->data_index] = face_buf->time_input;
                        iterate_cycle_fsm(face_buf);
                        save_face_buf(face_buf);
                    }
                    else if(face_buf->face_action == LOAD)
                    {
                        //load data from file if it exists
                        restore_face_buf(face_buf);
                    }
                    else if(face_buf->face_action == RESET)
                    {
                        reset_face_buf(face_buf);
                        //delete files
                        //reset current data
                        //enter error state
                    }

                    watch_display_text(WATCH_POSITION_FULL, "          ");

                    if(is_fertile(face_buf))
                    {
                        //watch_display_string(" FERT ",4);
                        watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "FERT  ", " FERT ");
                    }
                    else
                    {
                        //watch_display_string("NFERT ",4);
                        watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "nFERT ", "NFERT ");
                    }

                    //watch_display_string("  ",0);
                    

                break;

                case STATE_ERROR:
                break;

                default:
                break;
            }
            break;

        case EVENT_ALARM_BUTTON_UP:
            
            //new case depending on face state
            switch(face_buf->state)
            {
                case CALENDAR:
 
                break;

                case FLUID_ENTRY:
                    //Increment fluid entry buffer, display current selection
                    face_buf->fluid_input += 1;
                    if(face_buf->fluid_input > 4)
                    {
                        face_buf->fluid_input = 1;
                    }
                    display_fluid_type(face_buf->fluid_input);
                    break;

                case TEMP_ENTRY_1:
                    //Increment upper two temperature digits, display current selection
                    face_buf->temp_input[0] += 10;
                    if(face_buf->temp_input[0] > 100)
                    {
                        face_buf->temp_input[0] = 90;
                    }
                    snprintf(buf, sizeof(buf), "%2hu%hu%hu%hu", (uint8_t)(face_buf->temp_input[0]/10), face_buf->temp_input[1], face_buf->temp_input[2], face_buf->temp_input[3]);
                    //watch_display_string(buf, 4);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;

                case TEMP_ENTRY_2:
                    //Increment temperature digit 2, display current selection
                    face_buf->temp_input[1] += 1;
                    if(face_buf->temp_input[1] > 9)
                    {
                        face_buf->temp_input[1] = 0;
                    }
                    snprintf(buf, sizeof(buf), "%2hu%hu%hu%hu", (uint8_t)(face_buf->temp_input[0]/10), face_buf->temp_input[1], face_buf->temp_input[2], face_buf->temp_input[3]);
                    //watch_display_string(buf, 4);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;

                case TEMP_ENTRY_3:
                    //Increment temperature 10th decimal, display current selection
                    face_buf->temp_input[2] += 1;
                    if(face_buf->temp_input[2] > 9)
                    {
                        face_buf->temp_input[2] = 0;
                    }
                    snprintf(buf, sizeof(buf), "%2hu%hu%hu%hu", (uint8_t)(face_buf->temp_input[0]/10), face_buf->temp_input[1], face_buf->temp_input[2], face_buf->temp_input[3]);
                    //watch_display_string(buf, 4);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;

                case TEMP_ENTRY_4: 
                    //Increment temperature 100th decimal, display current selection
                    face_buf->temp_input[3] += 1;
                    if(face_buf->temp_input[3] > 9)
                    {
                        face_buf->temp_input[3] = 0;
                    }
                    snprintf(buf, sizeof(buf), "%2hu%hu%hu%hu", (uint8_t)(face_buf->temp_input[0]/10), face_buf->temp_input[1], face_buf->temp_input[2], face_buf->temp_input[3]);
                    //watch_display_string(buf, 4);
                    watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    break;

                case CONFIRM_ENTRY:
                //Cycle between Y and N for confirmation
                    switch(face_buf->face_action)
                    {
                        case SAVE:
                            face_buf->face_action = DELETE;
                            //watch_display_string(" DEL", 5);
                            watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "DELETE", "DEL");                                                       
                            break;
                        
                        case DELETE:
                            face_buf->face_action = LOAD;
                            //watch_display_string("LOAD", 5);
                            watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "LOAD  ", "LOAD");
                            break;
                        
                        case LOAD:
                            face_buf->face_action = RESET;
                            //watch_display_string(" RST", 5);
                            watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "RESET ", "RST");
                            break;
                        
                        case RESET:
                            face_buf->face_action = SAVE;
                            //watch_display_string("SAVE", 5);
                            watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "SAVE  ", "SAVE");
                            break;
                        
                        default:
                            face_buf->face_action = SAVE;
                            watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "SAVE  ", "SAVE");
                            break;
                    }
                break;

                case STATE_ERROR:
                break;

                default:
                break;
            }
            break;
        
        case EVENT_ALARM_LONG_UP:

            switch(face_buf->state)
            {
                case CALENDAR:
                    
                    temp_time = watch_rtc_get_date_time();
    
                    //Set input buffers to current data (default values if new day)
                    face_buf->fluid_input = face_buf->fluid_buf[face_buf->data_index];
                    if(face_buf->temp_buf[face_buf->data_index] >= 100.0)
                    {
                        face_buf->temp_input[0] = 100;
                    }
                    else
                    {
                        face_buf->temp_input[0] = 90;
                    }
                    face_buf->temp_input[1] = ((uint8_t)face_buf->temp_buf[face_buf->data_index] % 10);     
                    face_buf->temp_input[2] = (uint8_t)((uint16_t)(face_buf->temp_buf[face_buf->data_index] * 10) % 10);
                    face_buf->temp_input[3] = (uint8_t)((uint16_t)round(face_buf->temp_buf[face_buf->data_index] * 100) % 10);
                    face_buf->time_input = temp_time;
                   
                    watch_display_text(WATCH_POSITION_TOP_RIGHT, "  ");
                    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "FLD", "FL");

                    //watch_display_string("FL",0);
                    //watch_display_string("  ",2);
                    display_fluid_type(face_buf->fluid_input);

                    face_buf->state = FLUID_ENTRY;
                    break;

                case FLUID_ENTRY:
                break;

                case TEMP_ENTRY_1:
                break;

                case TEMP_ENTRY_2:
                break;

                case TEMP_ENTRY_3:
                break;

                case TEMP_ENTRY_4: 
                break;

                case CONFIRM_ENTRY:
                break;

                case STATE_ERROR:
                break;

                default:
                break;
            }
            
            break;

        case EVENT_TICK:
            switch(face_buf->state)
            {
                case CALENDAR:

                    watch_display_text(WATCH_POSITION_FULL, "          ");

                    if(is_fertile(face_buf))
                    {
                        //watch_display_string(" FERT ",4);
                        watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "FERT  ", " FERT ");
                    }
                    else
                    {
                        //watch_display_string("NFERT ",4);
                        watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "nFERT ", "NFERT ");
                    }
                    
                    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "DAY", "  ");
                    //watch_display_string("  ",0);
                    snprintf(buf, sizeof(buf), "%2hu", face_buf->cycle_day_num);
                    //watch_display_string(buf, 2);
                    watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);

                    break;

                case TEMP_ENTRY_1:

                    if(display_flash == 0)
                    {
                        display_flash = 1;
                        snprintf(buf, sizeof(buf), "  %hu%hu%hu", face_buf->temp_input[1], face_buf->temp_input[2], face_buf->temp_input[3]);
                        //watch_display_string(buf, 4);
                        watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    }
                    else
                    {
                        display_flash = 0;
                        snprintf(buf, sizeof(buf), "%2hu%hu%hu%hu", (uint8_t)(face_buf->temp_input[0]/10), face_buf->temp_input[1], face_buf->temp_input[2], face_buf->temp_input[3]);
                        //watch_display_string(buf, 4);
                        watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    }

                    break;

                case TEMP_ENTRY_2:

                    if(display_flash == 0)
                    {
                        display_flash = 1;
                        snprintf(buf, sizeof(buf), "%2hu %hu%hu", (uint8_t)(face_buf->temp_input[0]/10), face_buf->temp_input[2], face_buf->temp_input[3]);
                        //watch_display_string(buf, 4);
                        watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    }
                    else
                    {
                        display_flash = 0;
                        snprintf(buf, sizeof(buf), "%2hu%hu%hu%hu", (uint8_t)(face_buf->temp_input[0]/10), face_buf->temp_input[1], face_buf->temp_input[2], face_buf->temp_input[3]);
                        //watch_display_string(buf, 4);
                        watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    }

                    break;

                case TEMP_ENTRY_3:

                    if(display_flash == 0)
                    {
                        display_flash = 1;
                        snprintf(buf, sizeof(buf), "%2hu%hu %hu", (uint8_t)(face_buf->temp_input[0]/10), face_buf->temp_input[1], face_buf->temp_input[3]);
                        //watch_display_string(buf, 4);
                        watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    }
                    else
                    {
                        display_flash = 0;
                        snprintf(buf, sizeof(buf), "%2hu%hu%hu%hu", (uint8_t)(face_buf->temp_input[0]/10), face_buf->temp_input[1], face_buf->temp_input[2], face_buf->temp_input[3]);
                        //watch_display_string(buf, 4);
                        watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    }

                    break;

                case TEMP_ENTRY_4:

                    if(display_flash == 0)
                    {
                        display_flash = 1;
                        snprintf(buf, sizeof(buf), "%2hu%hu%hu ", (uint8_t)(face_buf->temp_input[0]/10), face_buf->temp_input[1], face_buf->temp_input[2]);
                        //watch_display_string(buf, 4);
                        watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    }
                    else
                    {
                        display_flash = 0;
                        snprintf(buf, sizeof(buf), "%2hu%hu%hu%hu", (uint8_t)(face_buf->temp_input[0]/10), face_buf->temp_input[1], face_buf->temp_input[2], face_buf->temp_input[3]);
                        //watch_display_string(buf, 4);
                        watch_display_text(WATCH_POSITION_BOTTOM, buf);
                    }

                    break;



                default:

                    break;
            }

            break;

        default:
            break;

    }

    return(true);
}

void fertility_tracker_face_resign(void *context)
{
    (void) context;
}

static void display_fluid_type(uint8_t value)
{
    switch(value)
    {
        case 0:
            //watch_display_string("      ", 4);
            watch_display_text(WATCH_POSITION_BOTTOM, "      ");
        break;

        case 1:
            //watch_display_string("  nn  ", 4);
            watch_display_text(WATCH_POSITION_BOTTOM, "  nn  ");
        break;

        case 2:
            //watch_display_string("  G   ", 4);
            watch_display_text(WATCH_POSITION_BOTTOM, "  G   ");
        break;

        case 3:
            //watch_display_string("  EL  ", 4);
            watch_display_text(WATCH_POSITION_BOTTOM, "  EL  ");
        break;

        case 4:
            //watch_display_string("  EE  ", 4);
            watch_display_text(WATCH_POSITION_BOTTOM, "  EE  ");
        break;

        default:
            //watch_display_string("  ERR ", 4);
            watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "ERROR", "  ERR ");
        break;
    }
}

//Compares the date of both arguments. Returns true if they have the same date, otherwise false.
static bool dates_are_equal(watch_date_time_t time1, watch_date_time_t time2)
{
    bool result;
    result = (time1.unit.month == time2.unit.month);
    result &= (time1.unit.day == time2.unit.day);
    result &= (time1.unit.year == time2.unit.year);
    return result;
}

//Run every time data is entered. Apply next state once per day
static enum cycle_state_t iterate_cycle_fsm(fertility_tracker_state_t * data_buf)
{
    float historic_max_temp_f;
    watch_date_time_t temp_time;
    uint16_t days_in_state;
    int i;
    uint8_t num_outliers = 0;
    float temp_temp;
    bool ovulation_confirmed = false;
    bool temp_shift_detected = false;

    temp_time = watch_rtc_get_date_time();

    switch(data_buf->cycle_state)
    {

        case FLUID_CHANGE:
            //Next state is ESTROGEN if an EE or EL was logged today.
            if(data_buf->fluid_buf[data_buf->data_index] > 2) //EL OR EE
            {
                data_buf->cycle_next_state = ESTROGEN;
            }

            break; 

        case ESTROGEN:
            /* Next state is TEMP_SHIFT_DETECT if a temperature is logged today that is at least 0.2 degrees(f)
               greater than the max of the past 6 days.

               In case of one invalid temperature (90.00 or any temp > 99.50), this value can be skipped and the previous day is used.
               In case of two invalid temperatures, both can be skipped but an extra day must be used (ie the max of 7 days).
               In case of >2 invalid temperatures wait in this state
            */
            historic_max_temp_f = get_historic_max_temp_f(data_buf); 
            if(data_buf->fluid_buf[data_buf->data_index] == 1) //M logged
            {
                data_buf->cycle_next_state = FLUID_CHANGE;
            }
            else if(historic_max_temp_f == INVALID_TEMP)
            {
                break;
            }
            else if(data_buf->temp_buf[data_buf->data_index] - historic_max_temp_f >= 0.2f)
            {
                    //Move to seeking temp shift, log historic max
                    data_buf->cycle_next_state = TEMP_SHIFT_DETECT;
                    data_buf->historic_max_temp_f = historic_max_temp_f;
                    data_buf->first_high_temp = &(data_buf->temp_buf[data_buf->data_index]);
            }

            break;

        case TEMP_SHIFT_DETECT:
            days_in_state = num_days_passed(data_buf->cycle_state_start, temp_time) + 1;

            for(i = 0; i < days_in_state; i++)
            {
                temp_temp = get_prev_temp(i + 1, data_buf);
                if(temp_temp == INVALID_TEMP || temp_temp >= FEVER_TEMP || (temp_temp - data_buf->historic_max_temp_f) < 0.2f)
                {
                    num_outliers++;
                }
            }

            if(data_buf->fluid_buf[data_buf->data_index] == 1) //M logged 
            {
                data_buf->cycle_next_state = FLUID_CHANGE;
            }
            else if(num_outliers >= 2)
            {
                //two days not 0.2f above historic max temp or invalid
                data_buf->cycle_next_state = ESTROGEN;
            }
            else if(days_in_state - num_outliers >= 2)
            {
                temp_temp = data_buf->temp_buf[data_buf->data_index];
                if(temp_temp == INVALID_TEMP || temp_temp >= FEVER_TEMP || (temp_temp - data_buf->historic_max_temp_f) < 0.2f)
                {
                    //Outlier, do nothing
                }
                else if((temp_temp - data_buf->historic_max_temp_f) >= 0.4f)
                {
                    //Before moving on, double check if we should skip seeking ovulation
                    if(detect_ovulation(data_buf) == true)
                    {
                        //Skip SEEKING_OVULATION
                        data_buf->cycle_next_state = OVULATION_CONFIRMED;
                    }
                    else
                    {
                        data_buf->cycle_next_state = SEEKING_OVULATION;
                    }
                }
                else
                {
                    //Extend temp shift detect
                    data_buf->cycle_next_state = TEMP_SHIFT_DETECT_EXTEND;
                }


            }

            break;

        case TEMP_SHIFT_DETECT_EXTEND:

            temp_temp = data_buf->temp_buf[data_buf->data_index];
            if(temp_temp == INVALID_TEMP || temp_temp >= FEVER_TEMP || (temp_temp - data_buf->historic_max_temp_f) < 0.2f)
            {
                temp_shift_detected = false;
            }
            else
            {
                temp_shift_detected = true;    
            }

            ovulation_confirmed = detect_ovulation(data_buf);

            if(data_buf->fluid_buf[data_buf->data_index] == 1) //M logged 
            {
                data_buf->cycle_next_state = FLUID_CHANGE;
            }
            else if((temp_shift_detected == true) && (ovulation_confirmed == true))
            {
                data_buf->cycle_next_state = OVULATION_CONFIRMED;
            }
            else if(temp_shift_detected == true)
            {
                data_buf->cycle_next_state = SEEKING_OVULATION;
            }
            else
            {
                data_buf->cycle_next_state = ERRATIC_TEMPS;
            }

            break;

        case SEEKING_OVULATION:
            
            if(data_buf->fluid_buf[data_buf->data_index] == 1) //M logged 
            {
                data_buf->cycle_next_state = FLUID_CHANGE;
            }
            else if(detect_ovulation(data_buf) == true)
            {
                data_buf->cycle_next_state = OVULATION_CONFIRMED;
            }

            break;

        case OVULATION_CONFIRMED:

            if(data_buf->fluid_buf[data_buf->data_index] == 1) //m logged
            {
                data_buf->cycle_next_state = FLUID_CHANGE;
            }

            break;

        case ERRATIC_TEMPS:

            if(data_buf->fluid_buf[data_buf->data_index] == 1) //m logged
            {
                data_buf->cycle_next_state = FLUID_CHANGE;
            }

            break;

        case ERROR:
            //Next state is FLUID_CHANGE when M is logged.
            if(data_buf->fluid_buf[data_buf->data_index] == 1) //m logged
            {
                data_buf->cycle_next_state = FLUID_CHANGE;
            }

            break;

        default:
            enter_error_state(data_buf);
            break;
    }
    return(data_buf->cycle_next_state);
}

//Can be called any time the fsm has been iterated or any time watch face is entered
static bool is_fertile(fertility_tracker_state_t * data_buf)
{
    bool fertility_status = true;
    watch_date_time_t temp_time;
    temp_time = watch_rtc_get_date_time();

    switch(data_buf->cycle_state)
    {
        case FLUID_CHANGE:
            /* Check today's fluid entry, return fertility status based on result.
                First 4 days in this state if temp shift occured: M = NF 
                else M = F
                G = NF
                EL/EE = F
            */
            if(data_buf->fluid_buf[data_buf->data_index] > 2) //EL/EE
            {
                    fertility_status = true;
            }
            else if(data_buf->fluid_buf[data_buf->data_index] == 2) //G
            {
                    fertility_status = false;
            }
            else if(data_buf->fluid_buf[data_buf->data_index] == 1) //M
            {
                if(num_days_passed(data_buf->cycle_state_start, temp_time) < 4 && data_buf->cycle_prev_state == OVULATION_CONFIRMED)
                {
                    fertility_status = false;
                }
                else
                {
                    fertility_status = true;
                }
            }
            else
            {
                    fertility_status = true;
            }
            break; 

        case ESTROGEN:
            fertility_status = true;
            break;

        case TEMP_SHIFT_DETECT:
            if(data_buf->cycle_next_state == OVULATION_CONFIRMED && temp_time.unit.hour > 17)
            {
                //NF after 6pm if next state is OVULATION_CONFIRMED
                fertility_status = false;
            }
            else
            {
                fertility_status = true;
            }
            break;

        case TEMP_SHIFT_DETECT_EXTEND:
            if(data_buf->cycle_next_state == OVULATION_CONFIRMED && temp_time.unit.hour > 17)
            {
                //NF after 6pm if next state is OVULATION_CONFIRMED
                fertility_status = false;
            }
            else
            {
                fertility_status = true;
            }
            break;

        case SEEKING_OVULATION:
            if(data_buf->cycle_next_state == OVULATION_CONFIRMED && temp_time.unit.hour > 17)
            {
                //NF after 6pm if next state is OVULATION_CONFIRMED
                fertility_status = false;
            }
            else
            {
                fertility_status = true;
            }
            break;

        case OVULATION_CONFIRMED:
            fertility_status = false;
            break;

        case ERRATIC_TEMPS:
            fertility_status = true;
            break;

        case ERROR:
            fertility_status = true;
            break;

        default:
            fertility_status = true;
            break;
    }

    return fertility_status;

}

/*  Returns the number of days difference between the two datetime arguments.
    Does not support more than a year. Can handle new month and new year situations
*/
static uint16_t num_days_passed(watch_date_time_t date1, watch_date_time_t date2)
{
    uint16_t date1_ytd;
    uint16_t date2_ytd;
    date1_ytd = watch_utility_days_since_new_year((uint16_t)date1.unit.year, (uint8_t)date1.unit.month, (uint8_t)date1.unit.day);
    date2_ytd = watch_utility_days_since_new_year((uint16_t)date2.unit.year, (uint8_t)date2.unit.month, (uint8_t)date2.unit.day);
    uint16_t result = 0;

    if(date1.unit.year == date2.unit.year)
    {
        result = abs(date2_ytd - date1_ytd);
    }
    else
    {
        if(date1.unit.year > date2.unit.year)
        {
            if(is_leap((uint16_t)date2.unit.year))
            {
                result = (366 - date2_ytd) + date1_ytd;
            }
            else
            {
                result = (365 - date2_ytd) + date1_ytd;
            }
        }
        else
        {
            if(is_leap((uint16_t)date1.unit.year))
            {
                result = (366 - date1_ytd) + date2_ytd;
            }
            else
            {
                result = (365 - date1_ytd) + date2_ytd;
            }
        }

    }

    return result;
}

static void enter_error_state(fertility_tracker_state_t * data_buf)
{
    data_buf->cycle_state = ERROR;
    data_buf->cycle_next_state = ERROR;
}

//returns a temperature value from num_days_prev days before today
static float get_prev_temp(uint16_t num_days_prev, fertility_tracker_state_t * data_buf)
{
    uint16_t index;
    if(num_days_prev > data_buf->data_index)
    {
        index = MEMORY_NUM_DAYS - (num_days_prev - data_buf->data_index);
    }
    else
    {
        index = data_buf->data_index - num_days_prev;
    }
    
    return(data_buf->temp_buf[index]);
}

//returns a fluid value from num_days_prev days before today
static uint8_t get_prev_fluid(uint16_t num_days_prev, fertility_tracker_state_t * data_buf)
{
    uint16_t index;
    if(num_days_prev > data_buf->data_index)
    {
        index = MEMORY_NUM_DAYS - (num_days_prev - data_buf->data_index);
    }
    else
    {
        index = data_buf->data_index - num_days_prev;
    }
    
    return(data_buf->fluid_buf[index]);
}

/*Max of previous 6 days if all valid temps
Max of previous 6 days (ignoring invalid temp) if one invalid temp was logged.
Max of previous 7 days (ignoring two invalid temps) if two invalid temps were logged.
Error condition if more than two invalid temps
*/
static float get_historic_max_temp_f(fertility_tracker_state_t * data_buf)
{
    uint8_t i;
    uint8_t invalid_temp_cnt = 0;
    float max = 0.0f;
    float temp;

    for(i = 0; i < 6; i++)
    {
        temp = get_prev_temp((i + 1), data_buf);
        if(temp <= INVALID_TEMP || temp >= FEVER_TEMP)
        {
            invalid_temp_cnt++;
            if(invalid_temp_cnt > 2)
            {
                return INVALID_TEMP;
            }
        }
        else if(temp > max)
        {
            max = temp;
        }
    }

    if(invalid_temp_cnt == 2)
    {
        temp = get_prev_temp(7, data_buf);
        if(temp <= INVALID_TEMP || temp >= FEVER_TEMP)
        {
            return INVALID_TEMP;
        }
        else if(temp > max)
        {
            max = temp;
        }
    }

    return(max);
}

static bool detect_ovulation(fertility_tracker_state_t * data_buf)
{
    int i;
    bool result = false;
    uint8_t el_ee_index = 0;

    for(i = 3; i <= NUM_EE_EL_SEARCH_DAYS; i++)
    {
        if(get_prev_fluid(i, data_buf) > 2)
        {
            //EL or EE found
            el_ee_index = i;
            break;
        }
    }

    if(el_ee_index > 0)
    {
        for(i = el_ee_index ; i > 2; i--)
        {
            if( get_prev_fluid(i - 1, data_buf) == 2 &&
                get_prev_fluid(i - 2, data_buf) == 2 &&
                get_prev_fluid(i - 3, data_buf) == 2) 
            {
                //Previous EL/EE followed by 3 consecutive Gs
                result = true;
                break;
            }
        }
    }

    return(result);
}

static bool save_face_buf(fertility_tracker_state_t * data_buf)
{
    char filename [] = "fertility_face_data.bin";
    bool return_val = true;

    if(filesystem_file_exists(filename) == true)
    {
        filesystem_rm(filename);
    }

    return_val = filesystem_write_file(filename, (char *)data_buf, sizeof(fertility_tracker_state_t));

    return(return_val);
}

static bool restore_face_buf(fertility_tracker_state_t * data_buf)
{
    char buf[FERTILITY_TRACKER_MEM_SIZE_BYTES];
    char filename [] = SAVE_FILENAME;
    bool return_val = true;

    if((filesystem_file_exists(filename) == true) && (FERTILITY_TRACKER_MEM_SIZE_BYTES <= sizeof(fertility_tracker_state_t)))
    {
        return_val = filesystem_read_file(filename, buf, sizeof(fertility_tracker_state_t));
        if(return_val == true)
        {
            memcpy(data_buf, buf, sizeof(fertility_tracker_state_t));
        }
        
    }
    else
    {
        return_val = false;
    }

    return(return_val);
}

static bool reset_face_buf(fertility_tracker_state_t * data_buf)
{
    char filename [] = SAVE_FILENAME;
    bool return_val = true;

    if(filesystem_file_exists(filename) == true)
    {
        filesystem_rm(filename);
    }

    memset(data_buf, 0, sizeof(fertility_tracker_state_t));
    data_buf->state = CALENDAR;
    data_buf->current_date = watch_rtc_get_date_time();
    enter_error_state(data_buf);

    return return_val;
}

static void save_debug_data(fertility_tracker_state_t * data_buf)
{
    char filename [] = DEBUG_FILENAME;
    char buf [DEBUG_FILE_MAX_LINE_SIZE];

    snprintf(buf, sizeof(buf), "%.2f,%u,%u,%u\n", data_buf->temp_buf[data_buf->data_index], data_buf->fluid_buf[data_buf->data_index], data_buf->cycle_state, data_buf->cycle_day_num);

    if(filesystem_file_exists(filename) == false)
    {
        filesystem_write_file(filename, &buf, strlen(&buf));
    }
    else
    {
        filesystem_append_file(filename, &buf, strlen(&buf));
    }
}