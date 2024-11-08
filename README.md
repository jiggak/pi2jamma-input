Linux kernel module to read buttons states from pi2jamma and report key presses.

## Usage

```bash
make
dtoverlay pi2jamma_input_overlay.dtbo
insmod pi2jamma_input.ko
```