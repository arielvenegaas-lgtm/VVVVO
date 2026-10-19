#pragma once
// Miscellaneous libXenon platform helpers for the VVVVVV port.

// The Xbox build's "bye!" menu option launched the dashboard via
// XLaunchNewImage(). On the 360 there is no dashboard API here, so we reboot
// the console through the SMC (closest equivalent).
void VvvvoQuitToDashboard(void);
