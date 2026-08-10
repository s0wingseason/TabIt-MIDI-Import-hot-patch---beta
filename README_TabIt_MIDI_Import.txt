TabIt MIDI Import hot patch - beta
==================================

What this patch changes
-----------------------
- Replaces the obsolete Tools > Check for Updates command with:
    Tools > Import MIDI Tool.
- The menu command launches the sibling native helper TabItMidiImport.exe.
- No registration, activation, license, or authentication code is modified.

Install / run
-------------
1. Keep these two files in the SAME folder:
     TabIt_MIDI_Import_Patched.exe
     TabItMidiImport.exe
2. Launch TabIt_MIDI_Import_Patched.exe.
3. Choose Tools > Import MIDI Tool.
4. Select a .mid or .midi file.
5. Choose where to save the generated .tbt file.
6. The helper asks Windows to open that .tbt file in the registered TabIt application.
   If file association does not auto-open it, use File > Open in TabIt.

Current beta import behavior
----------------------------
- Standard MIDI File (PPQ timing) input.
- One pitched MIDI channel per imported file. Drum channel 10 is ignored.
- Auto-detects standard 4-string bass vs standard 6-string guitar.
- Notes are quantized to TabIt's native 1/16-note space grid.
- Maps simultaneous notes to distinct playable strings at frets 0-24.
- Reads the MIDI tempo and initial time signature.
- Does not yet support time-signature changes within the song.
- Note attacks are imported; MIDI note-off durations are not represented separately.

Immediate Muse use case
-----------------------
The previously generated bass-only MIDI was validated through the same converter core:
434 note attacks were recovered 1:1 in the emitted TabIt note grid.

Files / hashes
--------------
Original supplied TabIt.exe SHA-256:
  8e7af9014ff94c7fa2e17345e8dc69439f381c00839de469ae33ad3523dfeda9

Patched TabIt SHA-256:
  145ef45f6f25996da5262fcf50c2c9e28e5cd1693d8256493d5e4960d8fed81e

Native helper SHA-256:
  423733275c72e1c033f255b03063578d122eabaffe4008df7725e239951645f3

Implementation notes
--------------------
The patched 32-bit TabIt executable redirects the old TCheckForUpdatesClick handler to a
small code stub placed in unused .text raw padding. That stub resolves the helper path next
to the running TabIt executable and launches it using TabIt's existing ShellExecuteA import.
The helper is a self-contained 64-bit Windows executable and requires no Python or other
third-party runtime.
