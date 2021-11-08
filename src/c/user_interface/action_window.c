#include <pebble.h>

#include "c/user_interface/action_window.h"
#include "c/user_interface/loading_window.h"
#include "c/modules/data.h"
#include "c/modules/comm.h"
#include "c/modules/apng.h"
#include "c/stateful.h"
static Window *s_action_window;
static ActionBarLayer *s_action_bar_layer;
static TextLayer *s_up_label_layer, *s_mid_label_layer, *s_down_label_layer;
static GRect s_default_label_rect;
static uint8_t s_tap_toggle = 0;
static Tile *tile;
static GBitmap *s_overflow_icon;
static uint8_t s_spinner_target;
static AppTimer *s_spinner_timer = NULL;
static void action_bar_reset_spinner(bool preserve_overflow);
static void action_window_reset_elements(bool select_icon);

uint8_t action_bar_tile_index;

typedef enum {
    TILE_DATA_ICON_KEY,
    TILE_DATA_TEXT
} TileDataType;

//! Calculates an index for icon_key or text elements in the current tile
//! @param id A ButtonId, either Up, Select or Down
//! @return A tile element index
static uint8_t tile_index_lookup(ButtonId id) {
    uint8_t idx;
    switch(id) {
        case BUTTON_ID_UP:
            idx = 0;
            break;
        case BUTTON_ID_SELECT:
            idx = 2;
            break;
        case BUTTON_ID_DOWN:
            idx = 4;
            break;
        default:
            idx = 0;
    }
    return idx + s_tap_toggle;
}

//! Finds an element within the current tile
//! @param id A ButtonId, either Up, Select or Down
//! @param type Which data type to return
//! @return A pointer to an element in the current tile
static char *tile_element_lookup(ButtonId id, TileDataType type) {
    uint8_t idx = tile_index_lookup(id);
    switch(type) {
        case TILE_DATA_ICON_KEY:
            return tile->icon_key[idx];
        case TILE_DATA_TEXT:
            return tile->texts[idx];
        default:
            return "";
    }
}

//! Refreshes all icons if current window is visible, used when icon_array is modified
void action_window_refresh_icons() {
    if (s_action_bar_layer && window_stack_get_top_window() == s_action_window) {
        action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_UP, data_icon_array_search(tile_element_lookup(BUTTON_ID_UP, TILE_DATA_ICON_KEY)), true);
        action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_SELECT, data_icon_array_search(tile_element_lookup(BUTTON_ID_SELECT, TILE_DATA_ICON_KEY)), true);
        action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_DOWN, data_icon_array_search(tile_element_lookup(BUTTON_ID_DOWN, TILE_DATA_ICON_KEY)), true);
        layer_mark_dirty(action_bar_layer_get_layer(s_action_bar_layer));
    }
}

//! Transitions to/from the overflow menu, which exposes a different set of up to 3 buttons
static void action_window_swap_buttons() {
    if (s_spinner_timer) { app_timer_cancel(s_spinner_timer); s_spinner_timer = NULL; }
    action_bar_reset_spinner(true);
    persist_delete(PERSIST_LAST_BUTTON);
    SHORT_VIBE();
    s_tap_toggle = !s_tap_toggle;

    action_window_reset_elements(false);

    layer_mark_dirty(text_layer_get_layer(s_up_label_layer));
    layer_mark_dirty(text_layer_get_layer(s_mid_label_layer));
    layer_mark_dirty(text_layer_get_layer(s_down_label_layer));

    layer_mark_dirty(action_bar_layer_get_layer(s_action_bar_layer));
    layer_mark_dirty(window_get_root_layer(s_action_window));
}

//! Changes action window's active colors based on a provided action.
//! Called when a button is clicked or an XHR function returns
//! @param action Controls color / vibration pattern applied 
void action_window_set_color(ColorAction action) {
    if (!s_action_window) { return; }
    light_enable_interaction();
    #ifndef PBL_COLOR
        return;
    #endif

    if (action == COLOR_ACTION_GOOD || action == COLOR_ACTION_BAD || action == COLOR_ACTION_ERROR) {
        if (s_spinner_timer) { app_timer_cancel(s_spinner_timer); s_spinner_timer = NULL; }
        action_bar_reset_spinner(false);
        LONG_VIBE();
    } 

    GColor8 new_color, new_highlight;
    switch(action) {
        case COLOR_ACTION_GOOD:
            new_color = GColorIslamicGreen;
            new_highlight = GColorMayGreen;
            break;
        case COLOR_ACTION_BAD:
            new_color = GColorFolly;
            new_highlight = GColorSunsetOrange;
            break;
        case COLOR_ACTION_ERROR:
            new_color = GColorChromeYellow;
            new_highlight = GColorRajah;
            break;

        case COLOR_ACTION_VIBRATE_RESPONSE:
            if (s_spinner_timer) { app_timer_cancel(s_spinner_timer); s_spinner_timer = NULL; }
            action_bar_reset_spinner(false);
        case COLOR_ACTION_VIBRATE_INIT:
            SHORT_VIBE();
            new_color = (s_tap_toggle) ? tile->highlight : tile->color;
            new_highlight = (s_tap_toggle) ? tile->color : tile->highlight;
            break;
        case COLOR_ACTION_RESET_ONLY:
            if (s_spinner_timer) { app_timer_cancel(s_spinner_timer); s_spinner_timer = NULL; }
            action_bar_reset_spinner(false);
            new_color = (s_tap_toggle) ? tile->highlight : tile->color;
            new_highlight = (s_tap_toggle) ? tile->color : tile->highlight;
            break;
    }

    window_set_background_color(s_action_window, new_color);
    action_bar_layer_set_background_color(s_action_bar_layer, new_highlight);
    text_layer_set_text_color(s_up_label_layer, text_color_legible_over(new_color));
    text_layer_set_text_color(s_mid_label_layer, text_color_legible_over(new_color));
    text_layer_set_text_color(s_down_label_layer, text_color_legible_over(new_color));

    layer_mark_dirty(text_layer_get_layer(s_up_label_layer));
    layer_mark_dirty(text_layer_get_layer(s_mid_label_layer));
    layer_mark_dirty(text_layer_get_layer(s_down_label_layer));
    layer_mark_dirty(window_get_root_layer(s_action_window));
    layer_mark_dirty(action_bar_layer_get_layer(s_action_bar_layer));
}

//! Drives label animations, called on the back of button clicks
//! @param button_id The button that triggered the call
void action_window_inset_highlight(ButtonId button_id) {
    TextLayer *labels[] = {s_up_label_layer, s_mid_label_layer, s_down_label_layer};
    Layer *layer;

    for (uint8_t i=0; i < ARRAY_LENGTH(labels); i++) {
        layer = text_layer_get_layer(labels[i]);

        GRect frame = layer_get_frame(layer);
        GRect start_rect = frame;
        start_rect.size.w = s_default_label_rect.size.w;
        start_rect.origin.x = s_default_label_rect.origin.x;

        GRect finish_rect = start_rect;
        finish_rect.origin.x -= finish_rect.size.w * 0.1;

        PropertyAnimation *prop_anim = NULL;
        if (button_id == i + 1 && frame.origin.x != finish_rect.origin.x) {
            prop_anim = property_animation_create_layer_frame(layer, &start_rect, &finish_rect);
        } else if (button_id != i + 1 && frame.origin.x == finish_rect.origin.x) {
            prop_anim = property_animation_create_layer_frame(layer, &finish_rect, &start_rect);
        } else if (button_id == i + 1) {
            start_rect = finish_rect;
            start_rect.origin.x -= start_rect.size.w * .05;
            prop_anim = property_animation_create_layer_frame(layer, &start_rect, &finish_rect);
        }
        if (prop_anim) {
            Animation *anim = property_animation_get_animation(prop_anim);
            animation_set_curve(anim, AnimationCurveEaseOut);
            animation_set_duration(anim, 100);
            animation_schedule(anim);
        }
    }
}

//! apng module callback, assigns animation data to last clicked button every frame
//! @param icon A bitmap image representing a frame in an ongoing animation
void action_bar_set_spinner(GBitmap *icon) {
    action_bar_layer_set_icon(s_action_bar_layer, s_spinner_target, icon);
    layer_mark_dirty(action_bar_layer_get_layer(s_action_bar_layer));
}

//! apng helper function, stops animation and resets all icons to pre-animation state
//! @param preserve_overflow Stops clobbering of select buttons long press transition
static void action_bar_reset_spinner(bool preserve_overflow) {
    s_spinner_timer = NULL;
    apng_stop_animation();
    action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_UP, data_icon_array_search(tile_element_lookup(BUTTON_ID_UP, TILE_DATA_ICON_KEY)));
    if (!preserve_overflow) { 
        action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_SELECT, data_icon_array_search(tile_element_lookup(BUTTON_ID_SELECT, TILE_DATA_ICON_KEY)));
    } else {
        action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_SELECT, s_overflow_icon);
    }
    action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_DOWN, data_icon_array_search(tile_element_lookup(BUTTON_ID_DOWN, TILE_DATA_ICON_KEY)));
}

//! apng helper function, resets icons and starts apng animation afresh
static void action_bar_start_spinner() {
    action_bar_reset_spinner(false);
    apng_start_animation();
}

//! Callback for all short button clicks, starts spinner animation and asks pebblekit
//! to perform an XHR request
//! @param recognizer The click recognizer that detected a "click" pattern
//! @param context Pointer to application specified data 
static void normal_click_callback(ClickRecognizerRef recognizer, void *ctx) {
    ButtonId button = click_recognizer_get_button_id(recognizer);
    uint8_t button_index = tile_index_lookup(button);
    if (strlen(tile->texts[button_index]) == 0) {
        action_window_set_color(COLOR_ACTION_RESET_ONLY);
    } else {
        s_spinner_target = button;
        s_spinner_timer = app_timer_register(200, action_bar_start_spinner, NULL);
        action_window_set_color(COLOR_ACTION_VIBRATE_INIT);
        action_window_inset_highlight(button);
        comm_xhr_request(action_bar_tile_index, button_index);
    }
}

//! Select long press callback, display overflow icon and then switches to/from overflow after delay 
//! @param recognizer The click recognizer that detected a "click" pattern
//! @param context Pointer to application specified data 
static void mid_hold_click_down_callback(ClickRecognizerRef recognizer, void *ctx) {
    action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_SELECT, s_overflow_icon, true);
    app_timer_register(100, action_window_swap_buttons, NULL);
}

//! Select long press released callback, replaces overflow icon with proper icon
//! @param recognizer The click recognizer that detected a "click" pattern
//! @param context Pointer to application specified data 
static void mid_hold_click_up_callback(ClickRecognizerRef recognizer, void *ctx) {
    action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_SELECT, data_icon_array_search(tile_element_lookup(BUTTON_ID_SELECT, TILE_DATA_ICON_KEY)), true);
}

//! Back click callback, exits app completly, skipping menu_window, if double click is detected
//! @param recognizer The click recognizer that detected a "click" pattern
//! @param context Pointer to application specified data 
static void back_click_callback(ClickRecognizerRef recognizer, void *ctx) {
    if (click_number_of_clicks_counted(recognizer) > 1) {
        window_stack_pop_all(true);
    } else {
        window_stack_pop(true);
    }
}

static void click_config_provider(void *ctx) {
    window_single_click_subscribe(BUTTON_ID_UP, normal_click_callback);
    window_single_click_subscribe(BUTTON_ID_SELECT, normal_click_callback);
    window_single_click_subscribe(BUTTON_ID_DOWN, normal_click_callback);

    window_long_click_subscribe(BUTTON_ID_SELECT, 250, mid_hold_click_down_callback, mid_hold_click_up_callback);
    window_multi_click_subscribe(BUTTON_ID_BACK, 1, 2, 150, true, back_click_callback);
}

//! Resets texts, icons, positions and colours
//! @param select_icon Stops clobbering of select buttons long press transition
static void action_window_reset_elements(bool select_icon) {
    Layer *window_layer = window_get_root_layer(s_action_window);
    GRect bounds = layer_get_bounds(window_layer);

    int16_t y_pad = PBL_IF_RECT_ELSE(5, 20);
    int16_t left_pad = ACTION_BAR_WIDTH * 0.7;
    int16_t right_pad = ACTION_BAR_WIDTH * 1.3;
    GRect up_label_bounds = GRect(bounds.origin.x, bounds.origin.y, bounds.size.w, bounds.size.h / 3);
    GRect mid_label_bounds = GRect(bounds.origin.x, bounds.origin.y + (up_label_bounds.origin.y + up_label_bounds.size.h),
                                   bounds.size.w, bounds.size.h / 3);
    GRect down_label_bounds = GRect(bounds.origin.x, bounds.origin.y + (mid_label_bounds.origin.y + mid_label_bounds.size.h),
                                   bounds.size.w, bounds.size.h / 3);

    // Magic value used as pebble height calculations are not accurate for custom font
    int16_t up_text_height = 1.332 * graphics_text_layout_get_content_size(tile_element_lookup(BUTTON_ID_UP, TILE_DATA_TEXT), ubuntu18, GRect(bounds.origin.x, bounds.origin.y, bounds.size.w - (left_pad + right_pad), bounds.size.h), GTextOverflowModeFill, GTextAlignmentRight).h;
    int16_t mid_text_height = 1.332 * graphics_text_layout_get_content_size(tile_element_lookup(BUTTON_ID_SELECT, TILE_DATA_TEXT), ubuntu18, GRect(bounds.origin.x, bounds.origin.y, bounds.size.w - (left_pad + right_pad), bounds.size.h), GTextOverflowModeFill, GTextAlignmentRight).h;
    int16_t down_text_height = 1.332 * graphics_text_layout_get_content_size(tile_element_lookup(BUTTON_ID_DOWN, TILE_DATA_TEXT), ubuntu18, GRect(bounds.origin.x, bounds.origin.y, bounds.size.w - (left_pad + right_pad), bounds.size.h), GTextOverflowModeFill, GTextAlignmentRight).h;

    GEdgeInsets up_label_insets = {.top = y_pad  + ((up_label_bounds.size.h - (up_text_height)) /2), .left = left_pad, .right = right_pad, .bottom = -y_pad};
    GEdgeInsets mid_label_insets = {.top = ((mid_label_bounds.size.h - (mid_text_height)) /2), .left = left_pad, .right = right_pad };
    GEdgeInsets down_label_insets = {.top = -y_pad + ((down_label_bounds.size.h - (down_text_height)) /2), .left = left_pad, .right = right_pad, .bottom = y_pad};
    layer_set_frame(text_layer_get_layer(s_up_label_layer), grect_inset(up_label_bounds, up_label_insets));
    layer_set_frame(text_layer_get_layer(s_mid_label_layer), grect_inset(mid_label_bounds, mid_label_insets));
    layer_set_frame(text_layer_get_layer(s_down_label_layer), grect_inset(down_label_bounds, down_label_insets));
    s_default_label_rect = layer_get_frame(text_layer_get_layer(s_up_label_layer));

    text_layer_set_text(s_up_label_layer, tile_element_lookup(BUTTON_ID_UP, TILE_DATA_TEXT));
    text_layer_set_text(s_mid_label_layer, tile_element_lookup(BUTTON_ID_SELECT, TILE_DATA_TEXT));
    text_layer_set_text(s_down_label_layer, tile_element_lookup(BUTTON_ID_DOWN, TILE_DATA_TEXT));

    action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_UP, default_icon);
    action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_UP, data_icon_array_search(tile_element_lookup(BUTTON_ID_UP, TILE_DATA_ICON_KEY)), true);
    action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_DOWN, default_icon);
    action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_DOWN, data_icon_array_search(tile_element_lookup(BUTTON_ID_DOWN, TILE_DATA_ICON_KEY)), true);
    if (select_icon) {
        action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_SELECT, default_icon);
        action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_SELECT, data_icon_array_search(tile_element_lookup(BUTTON_ID_SELECT, TILE_DATA_ICON_KEY)), true);
    }

    GColor8 toggle_color = (!s_tap_toggle) ? tile->color :tile->highlight;
    GColor8 toggle_highlight = (s_tap_toggle) ? tile->color :tile->highlight;
    text_layer_set_text_color(s_up_label_layer, text_color_legible_over(toggle_color));
    text_layer_set_text_color(s_mid_label_layer, text_color_legible_over(toggle_color));
    text_layer_set_text_color(s_down_label_layer, text_color_legible_over(toggle_color));
    window_set_background_color(s_action_window, toggle_color);
    action_bar_layer_set_background_color(s_action_bar_layer, toggle_highlight);
}  

static void action_window_load(Window *window) {
    apng_set_data(RESOURCE_ID_LOADING_MINI, &action_bar_set_spinner);

    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    s_action_bar_layer = action_bar_layer_create();
    s_overflow_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_OVERFLOW);
    s_tap_toggle = 0;
    s_up_label_layer = text_layer_create(GRectZero);
    s_mid_label_layer = text_layer_create(GRectZero);
    s_down_label_layer = text_layer_create(GRectZero);
    text_layer_set_text(s_up_label_layer, tile->texts[0]);
    text_layer_set_text(s_mid_label_layer, tile->texts[2]);
    text_layer_set_text(s_down_label_layer, tile->texts[4]);
    text_layer_set_background_color(s_up_label_layer, GColorClear);
    text_layer_set_background_color(s_mid_label_layer, GColorClear);
    text_layer_set_background_color(s_down_label_layer, GColorClear);
    text_layer_set_text_color(s_up_label_layer, text_color_legible_over(tile->color));
    text_layer_set_text_color(s_mid_label_layer, text_color_legible_over(tile->color));
    text_layer_set_text_color(s_down_label_layer, text_color_legible_over(tile->color));
    text_layer_set_text_alignment(s_up_label_layer, GTextAlignmentRight);
    text_layer_set_text_alignment(s_mid_label_layer, GTextAlignmentRight);
    text_layer_set_text_alignment(s_down_label_layer, GTextAlignmentRight);
    text_layer_set_font(s_up_label_layer, ubuntu18);
    text_layer_set_font(s_mid_label_layer, ubuntu18);
    text_layer_set_font(s_down_label_layer, ubuntu18);
    text_layer_set_overflow_mode(s_up_label_layer,GTextOverflowModeFill);
    text_layer_set_overflow_mode(s_mid_label_layer,GTextOverflowModeFill);
    text_layer_set_overflow_mode(s_down_label_layer,GTextOverflowModeFill);

    action_window_reset_elements(true);

    layer_add_child(window_layer, text_layer_get_layer(s_up_label_layer));
    layer_add_child(window_layer, text_layer_get_layer(s_mid_label_layer));
    layer_add_child(window_layer, text_layer_get_layer(s_down_label_layer));
    action_bar_layer_set_click_config_provider(s_action_bar_layer, click_config_provider);
    action_bar_layer_add_to_window(s_action_bar_layer, window);
}


static void action_window_unload(Window *window) {
    if (s_action_window) {
        if (s_spinner_timer) { app_timer_cancel(s_spinner_timer); s_spinner_timer = NULL; }
        apng_stop_animation();
        persist_delete(PERSIST_LAST_BUTTON);
        text_layer_destroy(s_up_label_layer);
        text_layer_destroy(s_mid_label_layer);
        text_layer_destroy(s_down_label_layer);
        if(s_overflow_icon) { gbitmap_destroy(s_overflow_icon); s_overflow_icon = NULL; }
        action_bar_layer_destroy(s_action_bar_layer);
        s_action_bar_layer = NULL;
        free(window_get_user_data(s_action_window));
        window_destroy(s_action_window);
        s_action_window = NULL;
    }
}

void action_window_pop() {
  window_stack_remove(s_action_window, false);
  action_window_unload(s_action_window);
}

void action_window_push(Tile *current_tile, uint8_t index) {
    if (!s_action_window) {
        tile = current_tile;
        action_bar_tile_index = index;
        s_action_window = window_create();
        window_set_background_color(s_action_window, tile->color);
        window_set_window_handlers(s_action_window, (WindowHandlers) {
            .load = action_window_load,
            .unload = action_window_unload,
        });
        window_stack_push(s_action_window, true);
    }

}

