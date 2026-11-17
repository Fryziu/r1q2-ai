# Instrukcje dla agentów — R1Q2 (Linux x86_64 + Win32 Cross-Build)

Przed rozpoczęciem pracy przeczytaj `NEXT_SESSION_PROMPT.md` (gęsty brief techniczny) oraz `PORTING_LINUX.md` (pełny dziennik zmian i testów). Po każdym etapie zaktualizuj dokumentację.

## Architektura i stan projektu
- **Linux x86_64 (główny cel):** natywny build GCC 13.3 na Linux Mint 22.2 x86_64 (`binaries/client/r1q2`, `binaries/r1q2ded/r1q2ded`, `binaries/game/r1q2_gamex86_64.so`, `binaries/ref_gl/ref_r1q2gl.so`).
- **Windows 32-bit (`make win32`):** cross-kompilacja `i686-w64-mingw32-gcc` (`binaries/Makefile.win32` -> `binaries/win32/{r1q2.exe,ref_r1q2gl.dll,r1q2_gamex86.dll}`, bez serwera dedykowanego). Statyczne zależności (`SDL2`, `libpng`, `libjpeg-turbo`, `libcurl`+Schannel, `zlib`, `libgcc`) w `/tmp/opencode/win32-deps`. Klient Windows dzieli wspólny kod SDL2 (`linux/gl_sdl2.c`, `linux/vid_so.c`, `linux/vid_menu.c`, `linux/snd_sdl2.c`, `linux/qgl_linux.c`) z wersją Linux.
- **Wideo (Linux + Windows):** SDL2 (`linux/gl_sdl2.c`, `linux/vid_menu.c`) + OpenGL 2.1 compatibility (`vid_ref gl`, software renderer wyłączony). Rozdzielczości od `800x600` do `3840x2160`, `gl_mode -2` (desktop), `gl_mode -1` (`vid_width`/`vid_height`), `vid_fullscreen` (`0`=okno, `1`=exclusive, `2`=fullscreen desktop), `vid_display` (wybór monitora SDL), `gl_hudscale` i `ch_scale`.
- **Wejście (Linux + Windows):** Mysz jest stale przechwycona (`SDL_SetRelativeMouseMode(SDL_TRUE)` + `SDL_SetWindowGrab`), dopóki okno gry ma fokus (`app_active`) — także w menu (`Esc`) i konsoli (`~`), co pozwala bindować `Mouse 1`–`Mouse 5` i docelowo obsługiwać własny kursor w menu. `IN_Frame` w `linux/vid_so.c` przełącza `RW_IN_Activate`, blokując jedynie obracanie kamery 3D w menu/konsoli. Zwolnienie myszy do systemu następuje przy `Alt+Tab` (utrata fokusu okna) lub skrócie `Ctrl+G`.
- **Audio (Linux + Windows):** SDL2 (`linux/snd_sdl2.c`, 16-bit stereo, ring buffer + `SDL_LockAudioDevice`, domyślnie `s_initsound 1`, fallback `0`).
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

---
*Maintained with **Cline** (`[CLINE-AI-SIG::R1Q2-SDL2-2026-11-17]`)*

