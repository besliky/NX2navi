# Changes on the `custom/anton-laptop` branch

Fork of NX2navi tuned for one machine: MSI Thin A15 B7VF, AMD Ryzen 5 7535HS (6 cores / 12 threads, Zen 3+, AVX2/FMA),
NVIDIA GeForce RTX 4060 Laptop 8 GB, 16 GB RAM, 40 GB page file, Windows 11 Pro. Every change is one commit; this file
says what changed, why, and what it measured before and after ([BENCHMARKS.md](BENCHMARKS.md) has the method and the
tables). Nothing built from the game leaves this machine.

Layout on the machine: sources `C:\work\self\nx2navi` (this checkout), the built game `C:\work\self\game`
(`ocarina.exe`, settings, saves, shader caches), a frozen copy of the stock 0.6.0 install in
`C:\work\self\baseline-0.6.0` (without `romfs`: it runs with `NX2NAVI_ROMFS=C:\work\self\game\romfs`), build logs in
`C:\work\self\logs`, benchmark logs and summaries in `C:\work\self\bench`, the runner in `C:\work\self\scripts\bench.py`.
The launcher's own install in `%LOCALAPPDATA%\Programs\NX2navi` is the untouched reference.

## Daily use

- Play: `C:\work\self\game\ocarina.exe` (start it from its folder; a shortcut with "Start in" set to that folder is
  fine). Settings are `C:\work\self\game\nx2navi.ini` (F1 in the game edits them), saves live in
  `C:\work\self\game\profile`. The log is `C:\work\self\game\logs\nx2navi.log`; its `version:` line reads
  `NX2navi 0.6.1` (or later) for this fork's build, `0.6.0` for the reference install. The launcher in the Start menu
  still runs the untouched reference install and may update it; it never touches `C:\work\self`.
- The reference install's save and this build's save are separate copies of the same format: to move progress, copy
  `profile\save` from one `game` folder to the other while neither game runs (or use the launcher's Import Saves with
  `C:\work\self\game\profile`).
- Update to upstream and rebuild (minutes when only the runtime changed, about 18 min when the lifter or cpu.h did):

  ```
  cd C:\work\self\nx2navi
  git fetch upstream
  git rebase upstream/main          # the branch is custom/anton-laptop
  C:\work\self\.venv-nx\Scripts\python.exe -I tools\make_game.py --rebuild --out C:\work\self\game ^
      --msys %LOCALAPPDATA%\Programs\NX2navi\toolchain\msys64\ucrt64 --jobs 12
  ```

  When upstream changes the shader translator (`tools/xlate`, `tools/vendor/ryujinx`), take `nx2navi_xlate.exe` and
  `nx2navi_xlate.id` from that release's `NX2navi-app-<version>.zip` (`bin/`) into this checkout's `bin/` before the
  rebuild: the .NET SDK that builds it is not installed here. The game then carries the old translations over and
  translates again in the background.
- Measure: `C:\work\self\.venv-nx\Scripts\python.exe -I C:\work\self\scripts\bench.py --exe-dir C:\work\self\game
  --tag <name> --mode save --wall 150 --script C:\work\self\scripts\input\load_slot1.txt`, one run at a time
  (`scripts\run_queue.sh <list>` serialises several). Results: `C:\work\self\bench\<name>.json` and `.log`.

## Build

```
C:\work\self\.venv-nx\Scripts\python.exe -I tools\make_game.py --rebuild --out C:\work\self\game ^
    --msys %LOCALAPPDATA%\Programs\NX2navi\toolchain\msys64\ucrt64 --jobs 12
```

`--rebuild` reuses the ExeFS, RomFS image and lifted C of the first build (copied from the reference install, whose
`build-cache/state.json` names the same dump). The toolchain is the launcher's own MSYS2 UCRT64 clang 23.1.5, read
only. The shader translator `bin/nx2navi_xlate.exe` is the 0.6.0 release's (the .NET SDK is not installed here); the
shader cache was copied from the reference install and carries over (`NX2NAVI_SHADER_CARRY`), so no shader preparation
ran. `.venv-nx` is a uv Python 3.14 with numpy, lz4, zstandard, zbic, fonttools and pillow; `.venv-nx12` a Python 3.12
with unicorn and capstone for the differential tests.

## 1. nn::web::ShowWebPage answered on the host (Zelda Notes crash)

- Commit: "Answer nn::web::ShowWebPage on the host: the page is reported closed instead of the SDK dereferencing a
  missing library applet."
- Files: `runtime/ov_web.c` (new), `runtime/ipc.c` `applet_stub_handler`.
- Cause: the game's Zelda Notes entry calls `nn::web::ShowWebPage(WebPageReturnValue *, const ShowWebPageArg &)`
  (sdk+0x7711a0, `main` imports it). The SDK asks the applet proxy's "library applet creator" (`ipc.c`
  `applet_proxy_handler`, interface 11) for `CreateLibraryApplet` (command 0). The stub answered every command with
  success and no data (`applet_stub_handler`, its last lines), so the SDK got no object id, took it for an accessor and
  dereferenced null.
- Fix: `HOST_OVERRIDE` of `ShowWebPage` (the loader rebinds the symbol in every module's GOT). It parses the request's
  TLV storage for the URL (type 1), logs `web: nn::web::ShowWebPage(<url>) from <caller>: the host has no web browser
  applet, so the page is reported closed (exit button) and the game goes on`, writes a return storage with one entry
  (type 1, exit reason 0 = exit button) and returns success. The layouts were read from the SDK's own accessors
  (`WebPageReturnValue::GetWebExitReason` walks `{u16 type, u16 size, u32 pad, data}` entries after a `{u16 count,
  u16 pad, u32 shim}` header; both storages are 0x2000 bytes, their constructors zero that much). As a second line,
  `CreateLibraryApplet` now fails with a logged error (`applet: CreateLibraryApplet(<id>) refused`) instead of a success
  without an object, so any other library applet the SDK ever asks for fails as a call rather than crashing.
- Effect: to verify in play (open Zelda Notes from the game's menu): the game continues, the log holds the `web:` line.
- Verification: `ocarina.exe --selftest` (the host-function registry includes the new override).

## 2. Launcher: the game's Cetra Display font is found after it was decoded

- Files: `launcher/ui/app.py` `load_fonts`, `launcher/gamefiles.py` `extract_fonts`.
- Cause: `gamefiles.font_family` prefers the typographic family (name id 16, "Cetra Display Pro"), but Windows
  enumerates a registered font by its legacy family (name id 1, "Cetra Display Pro Medium"), which is what Tk lists.
  The launcher therefore never saw its decoded font, logged `fonts: Segoe UI (the game's Cetra Display is not decoded
  yet)` at every start, decoded it again, and the rewrite of `data/cache/fonts/title.otf` failed with `Permission
  denied`, because the file it had registered is locked.
- Fix: a stored family Tk does not list is matched as a prefix of a listed one; `extract_fonts` leaves a file whose
  bytes already match alone and writes through a temporary file otherwise.
- Effect: the launcher's log says `fonts: Cetra Display Pro Medium (the game's, from data/cache/fonts)` once the fonts
  were decoded; no more `Permission denied`. (Applies to a launcher run from this checkout or to a release made from
  it; the reference install keeps its own 0.6.x launcher.)

## 3. Boot prebuild keeps only the compute pipelines play has met (`lib_prebuild_cs = recorded`)

- Commit: "Prebuild at boot only the compute pipelines play has met (lib_prebuild_cs = recorded), not every prepared
  one." Files: `runtime/vk_pipeline.c` `lb_start`, `runtime/cfg.c`, `nx2navi.ini.example`.
- Cause: the boot prebuild (`lb_start`) took every compute shader of the warm list, the 1,776 recorded in play and
  the 3,951 only prepared by the installer, built a pipeline for each and kept them all: 5,727 pipelines, about
  180 KB of process memory each in NVIDIA's driver.
- Measured (Kokiri Forest, Link standing, the owner's save, 1080p DLAA, windowed): commit census on the title screen
  "other private" 4,217 MiB with the prebuild, 3,173 MiB without it (`lib_prebuild = 0`); in play private bytes
  20,444 MiB (all) against 19,725 MiB (recorded), working set 6,516 MiB against 5,923 MiB, frame rate 43 fps in both.
- Cost: a prepared compute shader the game reaches later is built at its first dispatch from `pipeline-cache.bin`,
  about 1 ms, and is recorded for the next start.

## 4. prof_report.py resolves the exe's functions again

- Commit: "Resolve the exe's Rips against the map with the image base in the per-function report too." File:
  `tools/prof_report.py`.
- The per-function and category sums looked a Rip up as an offset from the runtime base alone, so every exe sample
  read as `?` and the guest/runtime split was 0. With the fix, the first profile of this machine (Kokiri Forest,
  150 s, `NX2NAVI_PROFILE`) reads: running samples are 0.8% of all samples (about one core busy on average over
  140 threads), of them 49% runtime host (`rec_begin` 16%, `vk_exec_op_draw` 7%, `kern_svc` 5%, `rt_dispatch` 5%)
  and 36% lifted game code. The frame rate of 43 fps is therefore not a CPU throughput limit: the GPU takes 17 ms a
  frame and idles 6 ms waiting for the next submission (`perf: frame` lines), the render thread waits on fences 39%
  of its time.

## 5. Large ASTC textures start one mip level down (`tex_quality = medium`)

- Commit: "Start large ASTC textures one mip level down (tex_quality = medium), which takes 1.2 GiB off the video
  memory on an 8 GB card." Files: `runtime/vk.h` (`VkTexImage.lod0`, `vk_tex_level`), `runtime/vk_texture.c`
  (`tex_quality`, `tex_skip_levels`, `img_level`, the image creation, `upload`, `astc_bl_regions`, the texel reads
  and writes, `alias_copy`), `runtime/vk_exec.c`, `runtime/vk_exec_draw.c`, `runtime/vk_exec_xfer.c` (every place a
  view's base level becomes an image level), `runtime/cfg.c`, `nx2navi.ini.example`.
- Cause: the texture census (`mem: textures frame` with `trace = perf`) in Kokiri Forest: ASTC textures decoded to
  BC3 2 724 textures, 2 768 MiB; other CPU textures 806 MiB; render targets 1 387 MiB; VRAM mirrors of the game's
  pools 868 MiB. The use sat at the 7 188 MiB budget (`vram 7103 of 7188`), the largest textures being two
  1365x1365x90 BC3 arrays of 11 levels (263 MiB each) and a 12288x12288 D16 shadow map (288 MiB, the game's own).
- Fix: a mipmapped ASTC texture the CPU made, 1 024 texels or more on its longer side, gets an image one level
  smaller (two levels from 2 048 and one from 512 with `low`); the game's level indices are lowered on the way in. A
  level the image does not hold is skipped on copies with a warning (`copy to or from level ...`), none occurred.
- Measured (my04 against my03-high, same scene): BC3 textures 2 758 to 1 580 MiB, the game's peak VRAM 7 206 to
  6 246 MiB, private bytes 19 990 to 19 186 MiB; frame rate 44.1 to 43.6 fps (noise), GPU time unchanged. Screenshots
  of both at the same spot (`C:\work\self\bench\shots-my04`, `shots-my03high`) are indistinguishable at 1080p.
- Why no frame rate gain: the GPU time was not VRAM paging (see "GPU power limit" below). The gain is headroom: the
  budget no longer fills in this area, so the area loads and Hyrule Field have 1 GiB more before textures are evicted
  and re-uploaded, and the process commit is 1 GiB lower on a 16 GB machine.

## Finding: the GPU runs at a 40 W power limit

`nvidia-smi -q -d POWER` on this laptop: current power limit 40 W, default 35 W, maximum 45 W, "SW Power Capping"
counted while the game ran; in play the GPU drew 30-34 W at 1 965 MHz with 77 % utilisation and took 17 ms a frame
in Kokiri Forest at 1080p with DLAA (`perf: frame ... gpu avg`), about 11 ms of it in the game's own passes
(`NX2NAVI_GPU_PASSES=1`: the deferred lighting pass 2.5 ms at 1440x810, 3.2 M fragments). The CPU is not the limit
(about one core busy, profile in 4). This is the RTX 4060 Laptop of a thin chassis doing its honest work, not the
port's inefficiency: a lower VRAM budget (e1), the texture levels (5) and the eviction thresholds (e4) did not move
the GPU time; only less GPU work does: `dlss = performance` 54 fps, `dlaa = 0` 49 fps. Options for the owner:
MSI Center's performance mode raises the limit to 45 W (about +12 %); DLSS quality or balanced instead of DLAA;
the Optimus path (the picture goes through the integrated Radeon to the display) also costs a copy per frame,
"Discrete graphics" mode where the laptop offers it removes it. None of these is changed by this fork.

## Benchmark discipline

One measured run at a time (`scripts/run_queue.sh` waits until no `ocarina.exe` runs): two games at once on
2026-11-09 halved both their frame rates and the runs were discarded. The owner's gamepad stays active during runs.
