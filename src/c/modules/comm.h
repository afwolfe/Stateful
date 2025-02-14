#pragma once
#include <pebble.h>
void comm_init();

void comm_deinit();

void comm_icon_request(char* iconKey, uint8_t iconIndex);
void comm_tile_request();
void comm_xhr_request(void *context, uint8_t id, uint8_t button);
void comm_callback_start();

#ifdef PBL_PLATFORM_APLITE
    #define INBOX_SIZE 256
#else
    #define INBOX_SIZE 8200
#endif

#define OUTBOX_SIZE 64