#ifndef __FAN_H__
#define __FAN_H__

typedef enum {
    FAN_SPEED_OFF = 0,
    FAN_SPEED_LOW,
    FAN_SPEED_MEDIUM,
    FAN_SPEED_HIGH
} fan_speed_t;

int fan_app_start(void);
void fan_set_speed(fan_speed_t speed);

#endif // !__FAN_H__