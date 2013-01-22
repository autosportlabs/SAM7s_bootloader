
// This file should contain #defines specific to your hardware. I need
// to know:

// The location of a GPIO pin wired to an LED, with the LED wired to the
// pin's anode (so that HIGH means that the LED is on).
#define GPIO_LED            0

// The location of a GPIO pin wired through a 1.5k resistor to DP (D+,
// whatever you want to call it; USB line). I can disconnect the pull-up
// that indicates that something is connected to the port by tri-stating
// that line, or connect it by driving that line HIGH.
#define GPIO_USB_PU         1

