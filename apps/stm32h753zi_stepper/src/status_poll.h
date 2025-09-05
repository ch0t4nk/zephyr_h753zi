#pragma once
#include <stdint.h>
#include <stdbool.h>

#if defined(CONFIG_STEPPER_STATUS_POLL)
/* Enable or disable the periodic status polling worker at runtime. */
void stepper_poll_set_enabled(bool en);
bool stepper_poll_is_enabled(void);

/* Copy up to `n` most-recent status samples for device `dev` into `out`.
 * Returns the number of samples written (<= n). Most-recent first.
 */
uint8_t stepper_poll_get_recent(uint8_t dev, uint8_t n, uint16_t *out);
#endif
