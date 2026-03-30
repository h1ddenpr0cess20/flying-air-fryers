# Flying Air Fryers

A homage to the classic After Dark "Flying Toasters" screensaver, reimagined with air fryers and tumbling chicken nuggets.

## Browser Version

No build step required. Open `index.html` in any modern web browser.

Use the speed slider at the bottom to control how fast the fryers fly.

## XScreenSaver Version

A native X11/Cairo screensaver hack that runs as a real XScreenSaver module on Linux.

### Dependencies

Debian/Ubuntu:
```bash
sudo apt install libcairo2-dev libx11-dev xscreensaver
```

Fedora/RHEL:
```bash
sudo dnf install cairo-devel libX11-devel xscreensaver
```

### Build and Install

```bash
make
./airfryer              # test in a standalone window
sudo make install       # install system-wide
```

After installing, restart xscreensaver-demo and select "Flying Air Fryers" from the list.

### Options

- **Speed** - adjustable via the xscreensaver-demo settings panel, or from the command line:

```bash
./airfryer -speed 200   # 2x speed
./airfryer -speed 50    # half speed
```

### Uninstall

```bash
sudo make uninstall
```

## License

MIT
