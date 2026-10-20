# PitchlyGraph, Graph Mode pitch editor (VST3)

Companion to [Pitchly](https://github.com/oxidizedmozzerella/PitchlyV2). Same look, different workflow:
capture a vocal, see its whole waveform on a piano roll, then **draw the notes you want it to sing**.

Current stage: UI + capture + note drawing (draw / select / move / resize / split / erase, per-note settings).
Audio passes through untouched; the pitch-shifting engine that plays your drawn notes is the next stage.

Build: push to GitHub and use Actions (artifacts `PitchlyGraph-VST3-x64` / `-ARM64`), or run `build_windows.bat x64`.
For FL Studio (x64 host) use the x64 build. Put it on the vocal's mixer insert, hit CAPTURE, play the section through it.
