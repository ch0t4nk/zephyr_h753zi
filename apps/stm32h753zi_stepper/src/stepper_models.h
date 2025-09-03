#pragma once
#include <stdint.h>

typedef struct {
    const char *name;
    int steps_per_rev;
    int max_speed;
    int max_current_ma;
    int max_microstep;
    float rated_voltage;
    float holding_torque;
} stepper_model_t;

const stepper_model_t *stepper_get_model(unsigned int idx);
const stepper_model_t *stepper_get_active(unsigned int axis);
int stepper_set_active_by_name(unsigned int axis, const char *name);
unsigned int stepper_get_model_count(void);
int stepper_persist_active(void);
