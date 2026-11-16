# Instrukcje dla agentów — R1Q2 (Linux x86_64 + Win32 Cross-Build)

Przed rozpoczęciem pracy przeczytaj `NEXT_SESSION_PROMPT.md` (gęsty brief techniczny) oraz `PORTING_LINUX.md` (pełny dziennik zmian i testów). Po każdym etapie zaktualizuj dokumentację.

## Architektura i stan projektu
- **Linux x86_64 (główny cel):** natywny build GCC 13.3 na Linux Mint 22.2 x86_64 (`binaries/client/quake2`, `binaries/r1q2ded/r1q2ded`, `binaries/game/gamex86_64.so`, `binaries/ref_gl/ref_gl.so`).
- **Windows 32-bit (`make win32`):** cross-kompilacja `i686-w64-mingw32-gcc` (`binaries/Makefile.win32` -> `binaries/win32/{r1q2.exe,quake2.exe,ref_gl.dll,ref_r1gl.dll,gamex86.dll,r1q2ded.exe}`). Statyczne zależności (`libpng`, `libjpeg-turbo`, `libcurl`+Schannel, `zlib`, `libgcc`) w `/tmp/opencode/win32-deps`. Działanie potwierdzone na Windows 11; dalsza rozbudowa UI Win32 jest wstrzymana.
- **Wideo (Linux):** SDL2 (`linux/gl_sdl2.c`, `linux/vid_menu.c`) + OpenGL 2.1 compatibility (`vid_ref gl`, software renderer wyłączony). Rozdzielczości od `800x600` do `3840x2160`, `gl_mode -2` (desktop), `gl_mode -1` (`vid_width`/`vid_height`), `vid_fullscreen` (`0`=okno, `1`=exclusive, `2`=fullscreen desktop), `vid_display` (wybór monitora SDL).
- **Wejście (Linux):** `IN_Frame` w `linux/vid_so.c` steruje grabem myszy na podstawie `cl.refresh_prepped` i `cls.key_dest` (w oknie wymaga `_windowed_mouse 1`; menu i konsola zwalniają kursor). Nie usuwaj tej logiki.
- **Audio (Linux):** SDL2 (`linux/snd_sdl2.c`, 16-bit stereo, ring buffer + `SDL_LockAudioDevice`, domyślnie `s_initsound 1`, fallback `0`).
- **HTTP i obrazy (wspólne):** `client/cl_http.c` (`USE_CURL=1`, URL z `CL_HTTPGamedir()`, zapis do `FS_Gamedir()`, fallback UDP). `ref_gl/gl_image.c` akceptuje źródła do `4096x4096` (skalowane przy uploadzie do `1024`) z fallbackiem `r_notexture`.
- **Ślad blastera (`CL_BlasterTrail`):** ZAMKNIĘTE — problem przerw zniknął po wprowadzeniu natywnych rozdzielczości monitora. Nie modyfikować.
- **Zasoby i referencje:** `/home/user/.quake2` jest tylko do odczytu. `reference/` służy wyłącznie do porównań (nie kompilować, nie kopiować mechanicznie).

## Komendy budowania
- Linux release: `make -j"$(nproc)"` (pełny: `make distclean && make -j"$(nproc)"`)
- Linux bez HTTP: `make distclean && make -j"$(nproc)" USE_CURL=0`
- Linux ASan/UBSan (`/tmp/opencode/r1q2-sanitize`): `make -j"$(nproc)" sanitize`
- Windows 32-bit (`binaries/win32/`): `make -j"$(nproc)" win32`

## Zasady zmian
- Wszystkie zmiany w repozytorium są niezacommitowane — zakaz `git reset`, `git clean`, `git checkout --`, `git restore` oraz `git commit`/`push` bez zgody użytkownika.
- Wprowadzaj małe, izolowane zmiany zgodne ze stylem kodu i po każdej zmianie weryfikuj kompilację.
- Zachowuj historyczną fizykę, protokół sieciowy, `crand()`, FPU 24-bit oraz punkt rozszerzenia `newgame`.
- Pod `_WIN32` w `game/q_shared.h` nie wyłączaj `__attribute__` dla `__GNUC__` (wymagane przez `__stdcall`/`APIENTRY` w MinGW-w64).
- Testy tymczasowe i logi zapisuj wyłącznie w `/tmp/opencode`.

