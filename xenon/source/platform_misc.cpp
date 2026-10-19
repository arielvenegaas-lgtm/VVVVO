#include "platform_misc.h"

#include <xenon_smc/xenon_smc.h>

void VvvvoQuitToDashboard(void) {
    // Give any last frame/audio a moment, then reboot the console.
    for (volatile int i = 0; i < 20000000; ++i) { }
    xenon_smc_power_reboot();
    for (;;) { }
}
