#ifndef LED_STATUS_H
#define LED_STATUS_H

#include <stdint.h>
#include <stdbool.h>

#define RID_LED_PIN_NC  -1

#ifndef CONFIG_RID_LED_R_GPIO
#define CONFIG_RID_LED_R_GPIO RID_LED_PIN_NC
#endif
#ifndef CONFIG_RID_LED_G_GPIO
#define CONFIG_RID_LED_G_GPIO RID_LED_PIN_NC
#endif
#ifndef CONFIG_RID_LED_B_GPIO
#define CONFIG_RID_LED_B_GPIO RID_LED_PIN_NC
#endif

void led_status_init(void);
void led_status_update(bool gps_valid, bool transmitting);

#endif