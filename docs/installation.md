# Installation

Download the package for your platform from the
[latest release](https://github.com/kuldron/obs-kuldron/releases/latest),
quit OBS, install, then start OBS again. The plugin is loaded automatically;
you can confirm it's there via View → Docks → Kuldron.

## Windows

Run the `windows-x64` `.exe` installer. (A `.zip` is also published if you
prefer to copy the files into your OBS install directory yourself.)

## macOS

Run the `macos-universal` `.pkg` installer.

macOS will refuse to open it at first, because the package isn't notarized
with Apple. To install anyway:

1. Double-click the `.pkg`. macOS shows a warning that it can't verify the
   package and won't open it. Dismiss the dialog (don't move it to the
   trash).
2. Open **System Settings → Privacy & Security** and scroll down to the
   **Security** section. You'll see a note that the package was blocked,
   with an **Open Anyway** button.
3. Click **Open Anyway**, authenticate, and confirm. The installer now runs
   normally.

On older macOS versions (pre-Sequoia) you can shortcut this: Control-click
the `.pkg` in Finder and choose **Open**.

## Linux

On Ubuntu 24.04 (and compatible), install the `.deb`:

```sh
sudo apt install ./obs-kuldron-*-ubuntu-24.04-x86_64.deb
```

For other distributions, extract the `ubuntu-24.04-x86_64.tar.xz` and copy
its contents into your OBS plugin directory.
