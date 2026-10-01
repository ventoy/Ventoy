# LinuxGUI UDisks2 + Flatpak
This is a fork of Ventoy which uses UDisks2 as a backend for LinuxGUI, so it can run as a flatpak.

## Development

You can get started with

```
flatpak-builder --force-clean --user --install-deps-from=flathub --repo=repo --install output LinuxGUI/net.ventoy.VentoyGUI.yml
```

(See https://docs.flatpak.org/en/latest/first-build.html)
