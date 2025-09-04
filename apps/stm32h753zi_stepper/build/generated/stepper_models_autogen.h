/* Auto-generated from stepper_models.yml */
#pragma once

#include <stdint.h>

#define STEPPER_MODEL_EXTENDED 1
typedef struct {
    const char *name;
    int steps_per_rev;
    int max_speed;
    int max_current_ma;
    int max_microstep;
    float rated_voltage;
    float holding_torque;
    int use_microstep;
    int kval_run;
    int kval_acc;
    int kval_dec;
    int kval_hold;
    int ocd_thresh_ma;
    int stall_thresh_ma;
    int acc_sps2;
    int dec_sps2;
} stepper_model_t;

static const stepper_model_t stepper_models[] = {
    { "17HS3001-20B", 200, 1200, 1200, 16, 2.000f, 0.400f, 16, 24, 24, 24, 12, 1500, 800, 1500, 1500 },
    { "NEMA23-HighTorque", 200, 2000, 3000, 128, 3.000f, 1.200f, 16, 32, 32, 32, 16, 2000, 1200, 2500, 2500 },
};

static const unsigned int stepper_model_count = sizeof(stepper_models)/sizeof(stepper_models[0]);
