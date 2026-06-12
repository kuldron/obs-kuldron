# Installation

Download the package for your platform from the
[latest release](https://github.com/kuldron/obs-kuldron/releases/latest),
quit OBS, install, then start OBS again. The plugin is loaded automatically;
you can confirm it's there via View → Docks → Kuldron.

## Windows

Extract the `windows-x64` `.zip` into `%ProgramData%\obs-studio\plugins`
(create the folder if it doesn't exist). You should end up with
`%ProgramData%\obs-studio\plugins\obs-kuldron\bin\64bit\obs-kuldron.dll`.

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

The `.deb` is built on Ubuntu 24.04; on it (and compatible distributions):

```sh
sudo apt install ./obs-kuldron-*-x86_64-linux-gnu.deb
```
