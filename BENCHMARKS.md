# Benchmarks on anton-laptop

Machine: AMD Ryzen 5 7535HS (6C/12T), NVIDIA GeForce RTX 4060 Laptop 8 GB (driver 621.01), 16 GB RAM, 40 GB page
file, Windows 11 Pro 26200. Game settings as the owner plays (`nx2navi.ini` of the reference install): game resolution
1080p, window 1920x1080, DLAA, fps limit 90, PlayStation prompts, Russian text.

## Method

`C:\work\self\scripts\bench.py` runs one build folder with a copy of the owner's profile (never the profile itself),
`NX2NAVI_TRACE=perf`, Discord off, windowed and behind other windows, and
samples the process's working set and private bytes, the system commit and the GPU's memory every 2 s. The game's own
`frametime:` lines (every 60 s and at exit: mean fps, 1% low, frames over 25/50/100 ms) and `perf: frame` lines (every
60 presents) are summarised into `C:\work\self\bench\<tag>.json`.

- **title**: no input; the title screen idles for 120 s, then the window is closed (a clean exit writes the final
  `frametime: exit` line).
- **save**: `tools/input/load_save.txt` loads the owner's first save slot; the game exits by itself when the world clock
  passed 90 s (`ocarina.exe --seconds 90`). The owner's save decides the scene.

The stock reference is a frozen copy of the launcher's 0.6.0 build (`C:\work\self\baseline-0.6.0`, same shader caches,
same translator). Hyrule Field and a second indoor scene need saves there: none exist yet in the owner's profile.

## Build and preparation times (reference install, launcher.log of 2026-11-09)

| Step | Stock 0.6.0 (launcher) |
|---|---|
| lift (ARM64 to C, 4 workers) | 162 s |
| compile (562 files, 12 threads, runtime prebuilt) | 976 s |
| shader preparation (42 142 variants) | 1 616 s |

## Results

All runs on 2026-11-09, windowed 1920x1080 behind other windows, the owner's settings (1080p, DLAA, fps limit 90).
"fps" is the median of the 60-present `perf: frame` windows after the load, "p10" their 10th percentile, "GPU" the
median GPU time per frame, "VRAM" the largest use the game reported (budget 7 188 MiB), "WS"/"private" the largest
working set and private bytes sampled, "W" the median GPU power in the second half of the run.

### Title screen (120 s idle)

| Build | fps | 1% low (exit line) | GPU ms | VRAM MiB | WS MiB | private MiB |
|---|---|---|---|---|---|---|
| stock 0.6.0 (reference copy) | 60.0 | 41.2 | 4.66 | 1 600 | 8 385 | 13 533 |
| fork, build 01 (upstream main + web fix) | 60.0 | 46.0 | 4.59 | 1 621 | 8 329 | 13 320 |
| fork, build 01, `lib_prebuild = 0` | 60.0 | | | | 7 458 | 12 277 |

Commit census on the title screen (fork, build 01): guest memory 8 235 MiB (heap 6 910, alias 1 274), other private
4 217 MiB (3 173 without the boot prebuild), images 1 087 MiB.

### Kokiri Forest, outside Link's house, standing (the owner's save; 150 s wall, about 100 s in the scene)

| Run | Build and setting | fps | p10 | GPU ms | VRAM MiB | WS MiB | private MiB | W |
|---|---|---|---|---|---|---|---|---|
| base-save | stock 0.6.0 | 36.9 | 36.1 | 22.1 | 6 825 | 8 408 | 20 251 | 32 |
| my02-save | fork build 02 (`lib_prebuild_cs = recorded`) | 43.2 | 42.0 | 17.2 | 7 103 | 8 457 | 19 726 | |
| e1 | fork, `NX2NAVI_VRAM_BUDGET_MB=6144` | 43.7 | 42.5 | 17.1 | 7 393 | 8 503 | 19 939 | 34 |
| e2b | fork, `dlss = performance` (scene at 50 %) | 54.2 | 49.8 | 11.2 | 7 784 | 9 260 | 20 425 | 30 |
| e3b | fork, `dlaa = 0` | 48.8 | 47.5 | 15.2 | 6 808 | 8 415 | 19 358 | 33 |
| e4 | fork, `tex_vram_pressure = 75` | 29.4 | 17.3 | 17.4 (43.9 late) | 8 500 | 8 730 | 21 747 | 24 |
| e5 | fork, `astc_bc = 0` (ASTC as RGBA8) | crash | | | | | 22 074 | | 
| e7 | fork, `gpu_passes = 1` (diagnosis) | 44.8 | 42.7 | 16.5 | 6 578 | 9 287 | 19 557 | 30 |
| my03-high | fork build 03, `tex_quality = high` | 44.1 | 42.4 | 17.0 | 7 206 | 8 621 | 19 990 | 32 |
| my04 | fork build 04, `tex_quality = medium` (default) | 43.6 | 41.7 | 17.4 | 6 246 | 8 493 | 19 186 | 32 |

e5 ended in a device loss during an upload with the process at 21.8 GB of commit (ASTC as RGBA8 doubles the texture
memory): a confirmation of the memory sensitivity, not a usable setting.

GPU power: `nvidia-smi -q -d POWER` reports a 40 W limit (default 35, maximum 45) and software power capping during
play. The GPU time is the laptop GPU's capacity at this power, see CHANGES.md "Finding".

Reading: the fork's base (upstream main of 2026-11-09 with its "higher frame rate" work) is 17 % faster than 0.6.0 in
this scene. The CPU is not the limit (profile: about one core busy on average; GPU idle 6 ms a frame waiting for the
next submission). DLAA costs about 2 ms of GPU time; DLSS performance halves the scene and takes the GPU to 11 ms.
Lowering the VRAM budget did nothing (the eviction cannot get the use under it); lowering the pressure threshold made
the game re-upload idle textures continuously (GPU 44 ms). The GPU draws 30-34 W at 1 965 MHz with 77 % utilisation,
the first reading was that the use at the 7.2 GiB budget made the driver page over PCIe; my04 (VRAM under the
budget by 1 GiB, same GPU time) ruled that out, and the power limit of 40 W explains the rest.
