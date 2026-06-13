#pragma once

#define WIFI_MAX_RETRY  5

void wifi_manager_init(void);

/* Blocks until connected (or max retries exhausted). */
void wifi_manager_wait_connected(void);
