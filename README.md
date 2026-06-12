# obs-kuldron

An optional OBS plugin for streaming to [Kuldron](https://kuldron.com). You
can stream to Kuldron without it; with it you get:

- **Multitrack audio** — send up to 6 OBS audio tracks (mic, game, music, …)
  as separate tracks. Kuldron exposes an individual volume control for each
  one to every viewer.
- **Multistreaming** — when OBS starts streaming somewhere that isn't
  Kuldron, the plugin can simultaneously go live to Kuldron (sharing OBS's
  running video encoder, so the extra stream costs no extra encode).

If Kuldron *is* your OBS stream target, there's nothing to click — hitting
Start Streaming upgrades the stream to multitrack automatically. Otherwise
enable **Multistream** in the Kuldron dock (View → Docks → Kuldron), or use
its Go Live button.

Streamer guide: [docs/streamer-setup.md](docs/streamer-setup.md)

## Building

Based on the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate);
the standard template build applies (`cmake --preset macos|windows-x64|ubuntu-x86_64`).
CI builds all three platforms; pushing a semver tag publishes a GitHub
release with installers.

## License

GPL-2.0-or-later
