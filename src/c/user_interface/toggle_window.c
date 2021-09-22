#include <pebble.h>

#include "c/user_interface/toggle_window.h"
#ifndef HEADER_FILE
#define HEADER_FILE
#define DEBUG 1
#include "c/modules/data.h"
#include "c/modules/comm.h"
#endif
static Window *s_main_window;
static GFont ubuntu18, ubuntu10;
static ActionBarLayer *s_action_bar_layer;
static TextLayer *s_up_label_layer, *s_mid_label_layer, *s_down_label_layer;
static GRect s_label_bounds;
static uint8_t tap_toggle = 0;

static void up_click_callback(ClickRecognizerRef recognizer, void *ctx) {
    toggle_window_set_color(3);
    toggle_window_inset_highlight(BUTTON_ID_UP);
    outbox(ctx, tile.id, 0 + tap_toggle);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Up clicked!");
}
static void mid_click_callback(ClickRecognizerRef recognizer, void *ctx) {
    toggle_window_set_color(3);
    toggle_window_inset_highlight(BUTTON_ID_SELECT);
    outbox(ctx, tile.id, 2 + tap_toggle);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "mid clicked!");
}
static void down_click_callback(ClickRecognizerRef recognizer, void *ctx) {
    toggle_window_set_color(3);
    toggle_window_inset_highlight(BUTTON_ID_DOWN);
    outbox(ctx, tile.id, 4 + tap_toggle);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Down clicked!");
}

static void click_config_provider(void *ctx) {
    window_single_click_subscribe(BUTTON_ID_UP, up_click_callback);
    window_single_click_subscribe(BUTTON_ID_SELECT, mid_click_callback);
    window_single_click_subscribe(BUTTON_ID_DOWN, down_click_callback);
} 

void toggle_window_swap_buttons() {
    // toggle_window_set_color(3);
    window_set_background_color(s_main_window, (tap_toggle) ? tile.color :tile.highlight);
    action_bar_layer_set_background_color(s_action_bar_layer, (tap_toggle) ? tile.highlight : tile.color );
    toggle_window_inset_highlight(BUTTON_ID_BACK);
    tap_toggle = !tap_toggle;
    text_layer_set_text(s_up_label_layer, tile.texts[tap_toggle]);
    text_layer_set_text(s_mid_label_layer, tile.texts[2 + tap_toggle]);
    text_layer_set_text(s_down_label_layer, tile.texts[4 + tap_toggle]);
    action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_UP, tile.icons[tap_toggle], true);
    action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_SELECT, tile.icons[2 + tap_toggle], true);
    action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_DOWN, tile.icons[4 + tap_toggle], true);
    layer_mark_dirty(text_layer_get_layer(s_up_label_layer));
    layer_mark_dirty(text_layer_get_layer(s_mid_label_layer));
    layer_mark_dirty(text_layer_get_layer(s_down_label_layer));
    layer_mark_dirty(action_bar_layer_get_layer(s_action_bar_layer));
    layer_mark_dirty(window_get_root_layer(s_main_window));
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
    toggle_window_swap_buttons();
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    GEdgeInsets up_label_insets = {.top = bounds.size.h * .135, .right = ACTION_BAR_WIDTH * 1.3 };
    GEdgeInsets mid_label_insets = {.top = bounds.size.h * .43, .right = ACTION_BAR_WIDTH * 1.3};
    GEdgeInsets down_label_insets = {.top = bounds.size.h * .735, .right = ACTION_BAR_WIDTH * 1.3};
    s_up_label_layer = text_layer_create(grect_inset(bounds, up_label_insets));
    s_mid_label_layer = text_layer_create(grect_inset(bounds, mid_label_insets));
    s_down_label_layer = text_layer_create(grect_inset(bounds, down_label_insets));
    s_label_bounds = layer_get_frame(text_layer_get_layer(s_up_label_layer));
    text_layer_set_text(s_up_label_layer, tile.texts[0]);
    text_layer_set_text(s_mid_label_layer, tile.texts[2]);
    text_layer_set_text(s_down_label_layer, tile.texts[4]);
    text_layer_set_background_color(s_up_label_layer, GColorClear);
    text_layer_set_background_color(s_mid_label_layer, GColorClear);
    text_layer_set_background_color(s_down_label_layer, GColorClear);
    text_layer_set_text_color(s_up_label_layer, GColorWhite);
    text_layer_set_text_color(s_mid_label_layer, GColorWhite);
    text_layer_set_text_color(s_down_label_layer, GColorWhite);
    text_layer_set_text_alignment(s_up_label_layer, GTextAlignmentRight);
    text_layer_set_text_alignment(s_mid_label_layer, GTextAlignmentRight);
    text_layer_set_text_alignment(s_down_label_layer, GTextAlignmentRight);
    text_layer_set_font(s_up_label_layer, ubuntu18);
    text_layer_set_font(s_mid_label_layer, ubuntu18);
    text_layer_set_font(s_down_label_layer, ubuntu18);

    layer_add_child(window_layer, text_layer_get_layer(s_up_label_layer));
    layer_add_child(window_layer, text_layer_get_layer(s_mid_label_layer));
    layer_add_child(window_layer, text_layer_get_layer(s_down_label_layer));

    s_action_bar_layer = action_bar_layer_create();
    action_bar_layer_set_background_color(s_action_bar_layer, tile.highlight);
    action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_UP, tile.icons[0]);
    action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_SELECT, tile.icons[2]);
    action_bar_layer_set_icon(s_action_bar_layer, BUTTON_ID_DOWN, tile.icons[4]);
    action_bar_layer_add_to_window(s_action_bar_layer, window);
    action_bar_layer_set_click_config_provider(s_action_bar_layer, click_config_provider);
    accel_tap_service_subscribe(tap_handler);
}

static void window_unload(Window *window) {
    text_layer_destroy(s_up_label_layer);
    text_layer_destroy(s_mid_label_layer);
    text_layer_destroy(s_down_label_layer);

    action_bar_layer_destroy(s_action_bar_layer);

    for(uint8_t i=0; i < ARRAY_LENGTH(tile.texts); i++) {
        free(tile.texts[i]);
    }

    for(uint8_t i=0; i < ARRAY_LENGTH(tile.icons); i++) {
        free(tile.icons[i]);
    }

    //free(&tile);

    fonts_unload_custom_font(ubuntu18);
    fonts_unload_custom_font(ubuntu10);

    app_message_deregister_callbacks();

    window_destroy(window);
}
void app_timer_callback(void *data) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "data: %d", *(uint8_t*) data);
    toggle_window_set_color(*(uint8_t*) data);
    free(data);
}
void toggle_window_set_color(uint8_t type) {
    // uint8_t *val = (uint8_t*) malloc(sizeof(uint8_t));
    // *val = 3;
    // AppTimer *app_timer = app_timer_register(1000, app_timer_callback, (void*) val);
    switch(type) {
        case 0:
            window_set_background_color(s_main_window, GColorIslamicGreen);
            action_bar_layer_set_background_color(s_action_bar_layer, GColorMayGreen);
            break;
        case 1:
            window_set_background_color(s_main_window, GColorFolly);
            action_bar_layer_set_background_color(s_action_bar_layer, GColorSunsetOrange);
            break;
        case 2:
            window_set_background_color(s_main_window, GColorChromeYellow);
            action_bar_layer_set_background_color(s_action_bar_layer, GColorRajah);
            break;
        default:
            // app_timer_cancel(app_timer);
            window_set_background_color(s_main_window, tile.color);
            action_bar_layer_set_background_color(s_action_bar_layer, tile.highlight);
            break;
    }
    layer_mark_dirty(window_get_root_layer(s_main_window));
    layer_mark_dirty(action_bar_layer_get_layer(s_action_bar_layer));
}
void toggle_window_inset_highlight(ButtonId button_id) {
    GRect up_rect = layer_get_frame(text_layer_get_layer(s_up_label_layer));
    GRect mid_rect = layer_get_frame(text_layer_get_layer(s_mid_label_layer));
    GRect down_rect = layer_get_frame(text_layer_get_layer(s_down_label_layer));
    up_rect.size.w = s_label_bounds.size.w; up_rect.origin.x = s_label_bounds.origin.x;
    mid_rect.size.w = s_label_bounds.size.w; mid_rect.origin.x = s_label_bounds.origin.x;
    down_rect.size.w = s_label_bounds.size.w; down_rect.origin.x = s_label_bounds.origin.x;
    layer_set_frame(text_layer_get_layer(s_up_label_layer), up_rect);
    layer_set_frame(text_layer_get_layer(s_mid_label_layer), mid_rect);
    layer_set_frame(text_layer_get_layer(s_down_label_layer), down_rect);
    switch (button_id) {
        case BUTTON_ID_UP:
            up_rect.size.w *= .9;
            layer_set_frame(text_layer_get_layer(s_up_label_layer), up_rect);
            break;
        case BUTTON_ID_SELECT:
            mid_rect.size.w *= .9;
            layer_set_frame(text_layer_get_layer(s_mid_label_layer), mid_rect);
            break;
        case BUTTON_ID_DOWN:
            down_rect.size.w *= .9;
            layer_set_frame(text_layer_get_layer(s_down_label_layer), down_rect);
            break;
        default:
            break;
    }
    layer_mark_dirty(text_layer_get_layer(s_up_label_layer));
    layer_mark_dirty(text_layer_get_layer(s_mid_label_layer));
    layer_mark_dirty(text_layer_get_layer(s_down_label_layer));
}


void toggle_window_push() {
    s_main_window = window_create();
    window_set_background_color(s_main_window, tile.color);
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });


    ubuntu18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UBUNTU_BOLD_18));
    ubuntu10 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UBUNTU_BOLD_10));

    // GColorIslamicGreen;
    // GColorMayGreen;
    // GColorFolly;
    // GColorSunsetOrange;

    window_stack_push(s_main_window, true);

}

