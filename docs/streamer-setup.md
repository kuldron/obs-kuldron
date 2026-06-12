# Streaming multitrack audio to Kuldron with OBS

The Kuldron plugin lets your viewers control each of your audio sources
independently — turn the game down, the music off, keep your mic up. This guide
gets you from a normal OBS setup to a live multitrack broadcast.

## 1. Install the plugin

1. Download the installer for your OS from the releases page
   (`obs-kuldron-<version>-macos.pkg` or `obs-kuldron-<version>-windows-x64.zip`).
2. Run the installer (macOS) or extract into your OBS plugins folder (Windows),
   then restart OBS.
3. In OBS, open **View → Docks → Kuldron**. Dock it wherever you like.

## 2. Put each sound on its own OBS track

Kuldron sends one audio track per OBS audio track, so set up your tracks in OBS
exactly as you would for multitrack recording. Decide which sources should be
separate faders for your viewers — a common layout:

| OBS Track | Sources |
|-----------|---------|
| 1 | Microphone |
| 2 | Game / desktop capture |
| 3 | Music / Spotify / browser |

Assign sources to tracks in OBS → **Edit → Advanced Audio Properties**: tick the
Track 1/2/3 boxes per source. (Optionally name the tracks in **Settings → Output
→ Audio** — the Kuldron dock shows those names next to each checkbox.)

> Keep your **mic on its own track** and don't also mix it into the game track,
> or viewers can't fully separate them.

## 3. Configure the dock

In the **Kuldron** dock:

1. **Server** — leave as `rtmp://ingest.kuldron.com:1935/live` unless told
   otherwise.
2. **Stream key** — paste your key from your Kuldron dashboard (Settings →
   Stream key). Click **Show** to verify it pasted correctly.
3. **Audio tracks** — tick each OBS track you want to send. Each ticked track
   becomes an independent fader for viewers. Names come from OBS (Settings →
   Output → Audio); hit **Refresh track names** if you just changed them.

There's no encoder to configure: the plugin uses your OBS **Settings →
Output** stream encoder exactly as you set it.

> One encoder requirement: set **Keyframe Interval** to **2 s or less** (OBS
> Settings → Output; with Advanced output mode it's per-encoder). Kuldron's
> ingest rejects streams with longer or automatic keyframe intervals and tells
> you why in the OBS log.

## 4. Go live

Three ways, depending on your setup:

- **Kuldron is your OBS stream target (simplest).** If your normal OBS
  **Settings → Stream** points at Kuldron, just hit OBS's own **Start
  Streaming** — the plugin notices and quietly upgrades that stream to
  carry your ticked tracks. Nothing to click in the dock; it just works.
- **Manual** — click **Go Live to Kuldron** in the dock. Use this when OBS
  itself isn't streaming, or you want Kuldron independent of OBS's stream.
- **Multistream** — tick **Multistream → Enabled** in the dock. Use this when
  OBS streams *elsewhere* (Twitch/YouTube): your normal Start Streaming also
  lights up Kuldron over its own connection (reusing OBS's running video
  encoder, so it adds no encoding load), and stopping streaming stops it too.

The status line tells you what happened — e.g. **"Live to Kuldron — OBS's
stream upgraded to 3 audio tracks."** If something fails, the message says why
(bad key, GOP/codec rejected, connection failed); the OBS log has detail.

## 5. Verify

On your channel page, the player should show **one volume fader per track**
instead of a single volume slider. Drag them — each source's level changes
independently. If you only see a single slider, the stream reached Kuldron with
one audio track: re-check that you ticked 2+ tracks and that your sources are on
distinct tracks (step 2).

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| "The ingest server rejected the stream (key, GOP, or codec)." | Wrong stream key, or keyframe interval > 2s / automatic. Set **Keyframe Interval** to 2s in OBS Settings → Output. |
| "Could not connect to the Kuldron ingest server." | Network/firewall, or wrong server URL. |
| Only one fader on the player | You ticked only one track, or all sources share a single track. |
| A ticked track's fader does nothing | No source is assigned to that OBS track (silent track). |
