/* Auto-generated from stepper_models.yml */
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

static const stepper_model_t stepper_models[] = {
    { "17HS3001-20B", 200, 1200, 1200, 16, 2.000f, 0.400f },
    { "NEMA23-HighTorque", 200, 2000, 3000, 128, 3.000f, 1.200f },
};

static const unsigned int stepper_model_count = sizeof(stepper_models)/sizeof(stepper_models[0]);
