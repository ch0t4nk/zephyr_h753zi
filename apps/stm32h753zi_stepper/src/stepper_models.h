#pragma once
/* Pull the generated model typedefs and data (supports extended fields).
 * During IDE/static analysis the generated header may not exist yet; provide
 * a stub fallback so source files still compile for diagnostics.
 */
#if defined(__has_include)
#  if __has_include("stepper_models_autogen.h")
#    include "stepper_models_autogen.h"
#  else
#    define STEPPER_MODEL_EXTENDED 1
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
static const stepper_model_t stepper_models[] = {0};
static const unsigned int stepper_model_count = 0;
#  endif
#else
#  include "stepper_models_autogen.h"
#endif

const stepper_model_t *stepper_get_model(unsigned int idx);
const stepper_model_t *stepper_get_active(unsigned int axis);
int stepper_set_active_by_name(unsigned int axis, const char *name);
unsigned int stepper_get_model_count(void);
int stepper_persist_active(void);
