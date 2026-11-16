# NEXT_SESSION_PROMPT — Gęsty brief kontekstowy dla agenta AI (R1Q2 Linux + Win32)

Język komunikacji: **polski**. Przed działaniem przeczytaj `AGENTS.md`, ten plik i `PORTING_LINUX.md`, następnie sprawdź `git status -s`.

## 1. Inwarianty repozytorium i bezpieczeństwa (KRYTYCZNE)
- **Drzewo robocze zawiera niezacommitowane zmiany produkcyjne.** Zakaz: `git reset`, `git clean`, `git checkout --`, `git restore`, `git commit`, `git push` bez jawnego polecenia użytkownika.
- **Zasoby gry (`/home/user/.quake2`):** wyłącznie do odczytu; zakaz modyfikacji/instalacji bez prośby. Testy syntetyczne/tymczasowe wyłącznie w `/tmp/opencode`.
- **Drzewa referencyjne (`reference/quake2idsoftware/`, `reference/q2pro/`):** wyłącznie do analizy porównawczej; zakaz kompilacji i mechanicznego kopiowania.
- **Kontrakty silnika i fizyki R1Q2 (NIENARUSZALNE):**
  - Zachować alias/komendę `newgame` w `client/menu.c` (`loading ; killserver ; wait ; newgame`) — punkt rozszerzenia cfg/modów.
  - Zachować fizykę ruchu (`qcommon/pmove.c`), logikę `game/`, bitową identyczność `crand()` (`(int32)(randomMT() - 0x7FFFFFFFU)`), format pakietów sieciowych i FPU 24-bit (`Sys_SetFPU`).
  - Renderer: OpenGL 2.1 compatibility (`ref_gl`), `vid_ref gl`, brak software renderera, brak migracji na core profile/shadery.
  - Mysz (`IN_Frame` w `linux/vid_so.c`): grab sterowany przez `cl.refresh_prepped` i `cls.key_dest` (menu/konsola zwalniają kursor; w oknie wymagane `_windowed_mouse 1`).

## 2. Zrealizowany i zweryfikowany stan projektu (Etapy 1–9: ZAKOŃCZONE)
- **Platforma hosta:** Linux Mint 22.2 x86_64, jądro 7.0/6.8, GCC 13.3, SDL2 2.30, MinGW-w64 i686 GCC 13.2.
- **Linux x86_64 (`make -j"$(nproc)"`):**
  - Artefakty: `binaries/client/quake2`, `binaries/r1q2ded/r1q2ded`, `binaries/game/gamex86_64.so`, `binaries/ref_gl/ref_gl.so`.
  - Wideo SDL2 (`linux/gl_sdl2.c`, `linux/vid_menu.c`): min/domyślnie `800x600`, presety do `3840x2160` (4K), `gl_mode -2` (natywna rozdzielczość monitora), `gl_mode -1` (`vid_width`/`vid_height`), `vid_fullscreen` (`0`=okno, `1`=exclusive, `2`=fullscreen desktop), `vid_display` (wybór monitora SDL, potwierdzone na 3 monitorach do 4K: 1440x900, 1920x1080, 3840x2160), High DPI drawable.
  - Audio SDL2 (`linux/snd_sdl2.c`): 16-bit stereo, 11025/22050/44100/48000 Hz (`s_khz`), ring buffer + `SDL_LockAudioDevice`, domyślnie `s_initsound 1` (`0`=fallback). Reset liczników bufora DMA przy `snd_restart` w `client/snd_dma.c`.
  - Poprawność LP64 i ASan/UBSan (`make sanitize` -> `/tmp/opencode/r1q2-sanitize`):
    - `client/cl_main.c`: rzutowanie `strtoul(CS_MAPCHECKSUM)` na `uint32` (naprawa ujemnych checksum > `INT_MAX`).
    - `ref_gl/gl_model.c`: wywołanie `Hunk_End()` również przy trafieniu w `model_size_cache` (naprawa `munmap failed (22)` przy powrocie na mapę `base3 -> base2`).
    - `linux/q_shlinux.c`: `HUNK_HEADER_SIZE = sizeof(void *)` (wyrównanie 8B na LP64), walidacja długości ścieżek w `Sys_FindFirst`/`Sys_FindNext` (`Sys_MakePath`).
    - `qcommon/common.c`, `qcommon/net_chan.c`, `server/sv_ents.c`, `client/cl_ents.c`, `server/sv_world.c`, `client/snd_mem.c`, `ref_gl/gl_rsurf.c`: usunięte UB (przesunięcia bitowe na `uint32`, strażnik `z_game_magic` przez `memcpy`/`memcmp`, granica `map < MAXLIGHTMAPS`).
    - `game/q_shared.c`, `client/console.c`: poprawiony warunek przepełnienia w `Com_sprintf` (`len < 0 || len >= size`) i rozmiar bufora wersji w `Con_DrawConsole` (`version[64]`).
    - `ref_gl/gl_rmain.c`, `ref_gl/gl_draw.c`, `client/cl_view.c`, `linux/gl_sdl2.c`, `linux/vid_menu.c`, `linux/vid_so.c`: ujednolicona funkcja `R_UpdateHudScale()` (poprawne `gl_hudscale` po `vid_restart`/`Alt+Enter`, brak ucinania prawej krawędzi, minimum `320x240`), niezależne pikselowe skalowanie celownika `ch_scale` (`Draw_Crosshair` bez zmiany ABI `GetRefAPI`) oraz kontrolki `hud scale` (`1x`–`4x`) i `crosshair scale` (`1x`–`4x`) w menu Video.

- **HTTP i duże tekstury (`client/cl_http.c`, `ref_gl/gl_image.c`):**
  - `USE_CURL=1` domyślnie (możliwe `USE_CURL=0` po `make distclean`). URL budowany z `CL_HTTPGamedir()`, zapis lokalny przez `FS_Gamedir()`, obsługa 301/302, limit `.filelist`, fallback UDP.
  - `LoadPNG`, `GL_Upload8`, `GL_LoadPic`: akceptacja źródeł do `4096x4096` (dynamiczny bufor `malloc` w `GL_Upload8` powyżej `512x256`, skalowanie przy uploadzie do `MAX_TEXTURE_DIMENSIONS = 1024`), fallback do `r_notexture` zamiast `ERR_DROP` dla przekroczonych wymiarów (potwierdzone na `makuta.pcx` 640×480).
- **Windows 32-bit Cross-Compile (`make -j"$(nproc)" win32`):**
  - Konfiguracja w `binaries/Makefile.win32`; obiekty tymczasowe w `/tmp/opencode/win32-build`, statyczne zależności (`libpng 1.6.43`, `libjpeg-turbo 2.1.5`, `libcurl 8.5.0` z Schannel i wątkami Win32) w `/tmp/opencode/win32-deps`.
  - Artefakty w `binaries/win32/`: `r1q2.exe`, `quake2.exe`, `ref_gl.dll`, `ref_r1gl.dll`, `gamex86.dll`, `r1q2ded.exe` (brak zewnętrznych zależności DLL poza systemowymi bibliotekami Windows).
  - Dostosowania MinGW-w64 GCC (`game/q_shared.h`, `qcommon/qcommon.h`, `client/snd_mix.c`, `win32/*`): zachowane `__attribute__` dla GCC pod `_WIN32` (krytyczne dla `__stdcall`/`APIENTRY`), `_ReturnAddress() -> __builtin_return_address(0)`, asembler MSVC i SEH `__try/__except` pod `#ifdef _MSC_VER`, GCC inline asm dla `Sys_GetFPUStatus`/`Sys_SetFPU`.
  - **Status Win32:** potwierdzone przez użytkownika poprawne uruchomienie i działanie na **Windows 11**.

## 3. Zagadnienia zamknięte / wstrzymane (NIE OTWIERAĆ AUTOMATYCZNIE)
- **Ślad blastera (`CL_BlasterTrail`):** **ZAMKNIĘTE / OK.** Obserwowany wcześniej efekt przerywania zniknął po wprowadzeniu natywnych rozdzielczości monitora. Nie modyfikować `CL_BlasterTrail` ani rysowania cząsteczek.
- **Rozbudowa klienta Windows (zrównanie `win32/vid_menu.c` z wersją Linux):** **WSTRZYMANE.** Użytkownik potwierdził, że obecna działająca wersja Windows 11 w zupełności wystarcza; nie rozwijać warstwy `win32/` bez nowego polecenia.
- **Etap poprawności ASan/UBSan i HTTP/libcurl:** **ZAKOŃCZONE.**
- **Komunikaty niebędące błędami:** `couldn't exec r1gl.cfg` (brak opcjonalnego cfg), `Function offsets ...` (diagnostyka `ReadLevel()`).

## 4. Plan kolejnych zadań (wybierać pojedynczo z użytkownikiem)
1. **Browser serwerów (Master Server List HTTP/HTTPS + OOB `status`) na wzór Q2Pro (GŁÓWNY KOLEJNY CEL):**
   - Wstępna analiza w `reference/OpenCode_findings_Q2Pro_masterServerListProtocol.md` oraz wzorzec w `reference/q2pro/src/client/ui/servers.c`.
   - Wykorzystanie istniejącego `libcurl` (`USE_CURL=1`) do pobierania list serwerów HTTP/HTTPS (format binarny 6B IP+port oraz plain text), odpytywanie UDP OOB `status` z rate-limitingiem i nowoczesny widok przeglądarki w kliencie.
2. **Naprawa powielania ścieżki w `Sys_GetGameAPI` przy absolutnym `basedir`:**
   - W `linux/sys_linux.c` lokalna gra z absolutnym `basedir` powiela ścieżkę przy ładowaniu `gamex86_64.so`.
3. **Drobne detale wideo / wejścia na Linuxie (np. odstępy pól custom width/height w `vid_menu.c`, XFCE, przenoszenie okna między monitorami, opcjonalnie kontroler/IME):**
   - Każde jako osobne, małe zadanie.
4. **Optymalizacja:** wyłącznie po wcześniejszym profilowaniu, bez przepisywania architektury renderera.

