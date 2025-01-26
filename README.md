Zephyr project for a Bluetooth-controlled PWM LED controller and temperature
sensor running on battery power. Designed to work with Wuerth Proteus-III
nRF52840 modules.

This repository also contains a minimal MAX17048 fuel gauge driver. At the time
there was no upstream support in Zephyr.

# Building

1. Setup the Nordic Connect SDK (version 2.2.0) and apply the patches from the `patches/` directory.
2. Edit `setup-env.sh` as required and then source it in your shell.
3. Run `west build`.
