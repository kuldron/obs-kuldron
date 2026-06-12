# Development

## Building

Based on the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate);
the standard template build applies:

```sh
cmake --preset macos|windows-x64|ubuntu-x86_64
cmake --build --preset macos|windows-x64|ubuntu-x86_64
```

CI builds all three platforms; pushing a semver tag publishes a GitHub
release with installers.
