Linux kernel module to read buttons states from pi2jamma and report key presses.

## Usage

```bash
make
dtoverlay pi2jamma-input.dtbo
insmod pi2jamma_input.ko
```

```bash
make
sudo -E make install
sudo cp pi2jamma-input.dtbo /boot/overlays
```

Add `dtoverlay=pi2jamma-input` to `config.txt`