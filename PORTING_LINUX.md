# R1Q2 — port i modernizacja dla Linux Mint

Dokument roboczy projektu. Aktualizujemy go po każdym zakończonym etapie oraz po istotnych testach. Główna instrukcja użytkownika znajduje się w `README.md`.

**Stan na:** 2026-11-17
**Aktualny etap:** etap 10 zakończony — ujednolicono unikalne nazwy binarek (`r1q2`, `ref_r1q2gl`, `r1q2_game*`), zmigrowano klienta Windows 32-bit na wspólny backend SDL2 (obraz, menu Video, mysz, klawiatura, dźwięk) oraz naprawiono pobieranie HTTP z automatycznym fallbackiem do UDP Quake 2
**Status:** cel osiągnięty — Linux x86_64 oraz Windows 32-bit SDL2 (`make win32`, Windows 11) przetestowane i potwierdzone przez użytkownika

## 1. Cel i sposób pracy

Celem jest stopniowe dostosowanie starego R1Q2/Quake II do współczesnego Linuxa bez jednoczesnego przepisywania silnika lub renderera.

Zasady:

- małe, odseparowane zmiany i test po każdym etapie;
- zachowanie działającego punktu odniesienia;
- szacunek dla istniejących hacków i punktów rozszerzenia: przed zmianą sprawdzamy ich rolę w konfiguracji i modach; w szczególności zachowujemy komendę/alias `newgame`;
- obecnie renderer OpenGL compatibility, bez migracji na core/shadery;
- aktywny backend to natywny SDL2; stary SDL 1.2 pozostaje tylko jako niekompilowany punkt odniesienia;
- audio SDL2 jest zaimplementowane i zaakceptowane; domyślne jest `s_initsound 1`, a wyłączenie pozostaje dostępne przez `s_initsound 0`;
- software renderer jest wyłączony w loaderze i menu;
- minimalna/domyślna rozdzielczość to 800x600;
- nie modyfikujemy oryginalnych danych gry ani katalogu `/home/user/.quake2` bez wyraźnej prośby użytkownika;
- nie wykonujemy commitów ani innych mutacji Git bez wyraźnej zgody użytkownika.

## 2. Środowisko

- Linux Mint 22.2 x86_64, jądro 6.8.0-147-generic;
- GCC 13.3.0, GNU Make 4.3;
- SDL 1.2.68 (warstwa zgodności nad SDL2), SDL2 2.30.0;
- libpng 1.6.43, libjpeg-turbo 2.1.5, zlib 1.3;
- wymagane pakiety build: `libsdl2-dev libgl1-mesa-dev libglu1-mesa-dev libpng-dev libjpeg-dev zlib1g-dev libcurl4-openssl-dev`;
- testowy renderer: NVIDIA GeForce RTX 3060, OpenGL 4.6, sterownik 595.99.02;
- oryginalne zasoby gry: `/home/user/.quake2/baseq2` (nie są częścią repozytorium);
- opcjonalne źródła referencyjne: `reference/quake2idsoftware/` i `reference/q2pro/` — wyłącznie do porównań, nie do wspólnej kompilacji i nie do nadpisywania plików R1Q2.

## 3. Kompilacja i artefakty

Wymagane pakiety na Linux Mint 22/Ubuntu 24.04:

```sh
sudo apt install libsdl2-dev libgl1-mesa-dev libglu1-mesa-dev libpng-dev libjpeg-dev zlib1g-dev libcurl4-openssl-dev
```

Z katalogu głównego repozytorium:

```sh
make -j"$(nproc)"
```

Pełna rekompilacja:

```sh
make distclean
make -j"$(nproc)"
```

Wariant bez HTTP:

```sh
make distclean
make -j"$(nproc)" USE_CURL=0
```

Osobny build ASan/UBSan w `/tmp/opencode/r1q2-sanitize`:

```sh
make -j"$(nproc)" sanitize
```

Wyniki:

- `binaries/client/r1q2`
- `binaries/r1q2ded/r1q2ded`
- `binaries/game/r1q2_gamex86_64.so`
- `binaries/ref_gl/ref_r1q2gl.so`

Układ wymagany przy uruchomieniu z katalogu gry:

```text
r1q2
ref_r1q2gl.so
baseq2/r1q2_gamex86_64.so
baseq2/pak0.pak i pozostałe oryginalne zasoby
```

Renderer jest ładowany jako `./ref_r1q2gl.so`, dlatego katalog roboczy ma znaczenie.

## 4. Zakończony punkt startowy — SDL 1.2 / OpenGL

W pierwszej fazie wykonano:

- natywną kompilację wszystkich czterech komponentów dla x86_64;
- przejście z `libpng12` na systemowy libpng16 i libjpeg-turbo;
- aktualizację loadera PNG do publicznego API libpng, konwersję do RGBA i obsługę błędów;
- użycie systemowego `jpeg_mem_src` i poprawienie zwalniania pamięci JPEG;
- zmianę `image_t.texnum` na `GLuint`;
- domyślne `s_initsound 0`;
- wymuszenie `vid_ref gl` i usunięcie fallbacku `soft`/`softx`;
- ograniczenie menu wideo do OpenGL;
- domyślny tryb 800x600 i wymuszanie minimum 800x600 także dla starszych ustawień;
- automatyczne tworzenie katalogów `.depends`;
- dodanie `-lm` przy linkowaniu biblioteki gry;
- poprawienie definicji `cl_localents` i linkowania `SV_FixPlayerSkin` dla GCC 13;
- poprawienie testów bitowych przycisków myszy 4 i 5.

Testy punktu startowego:

- pełna rekompilacja zakończona powodzeniem;
- `r1q2ded +quit` zakończone kodem 0;
- klient z minimalnymi tymczasowymi zasobami uruchomił OpenGL i zakończył `+quit` kodem 0;
- osobno zweryfikowano ładowanie testowych PNG i JPEG;
- użytkownik potwierdził start z oryginalnymi zasobami, działanie `map q2dm1`, myszy i klawiatury oraz zmiany klasycznych rozdzielczości.

## 5. Etap 1 — poprawki przed SDL2

### 5.1. Start nowej gry singleplayer

W `client/menu.c` poprawiono sprawdzanie mapy z `CM_MapWillLoad("base1")` na `CM_MapWillLoad("maps/base1.bsp")`.

Ostatecznie zachowano sekwencję `loading ; killserver ; wait ; newgame`. Wcześniejsza tymczasowa zamiana na `map base1` została wycofana po wyjaśnieniu użytkownika: `newgame` może być definiowane w `.cfg`, wybierać właściwą mapę startową dla pełnej wersji/demo i pełnić rolę punktu rozszerzenia dla modów. Nie zastępujemy takich komend bez zrozumienia ich zastosowania.

Testy użytkownika potwierdziły uruchomienie `base1`, działanie aliasu `newgame` oraz przejścia `base1 -> base2 -> base3`. Kolejny test potwierdził również powrót `base3 -> base2` i save/load.

### 5.2. Błędne porównanie checksumy mapy na x86_64

Objaw zgłoszony dla `base1`:

```text
ERROR: Local map version differs from server: 0xcae30620 != 0xcae30620
```

Przyczyna: starszy serwer zapisuje checksumę przez `%i`. Dla wartości większych od `INT_MAX` powstaje ujemna liczba dziesiętna. `strtoul` na LP64 zwracał 64-bitową wartość ujemną modulo 2^64, która była porównywana bez rzutowania z 32-bitową checksumą mapy. Wypisanie przez `atoi` i `%x` pokazywało identyczne dolne 32 bity, maskując przyczynę.

W `client/cl_main.c` wynik `strtoul` jest teraz jawnie rzutowany do `uint32` przed porównaniem. Zachowuje to zgodność zarówno ze starymi ujemnymi, jak i nowymi dodatnimi zapisami checksum.

Test jednostkowy konwersji dla `0xcae30620` i starego zapisu `-891091424` zakończył się zgodnością 32-bitowych wartości.

### 5.3. Duże obrazy źródłowe PNG

Zgłoszony komunikat:

```text
Invalid or oversized PNG file: players/male/grunt.png
```

Loader akceptuje teraz źródłowe PNG do 4096x4096. Podczas uploadu OpenGL obrazy są nadal skalowane do dotychczasowego limitu `MAX_TEXTURE_DIMENSIONS` (1024), więc nie zmieniono jeszcze limitu tekstur GPU ani struktur renderera.

Testowy obraz RGBA 2048x2048 został poprawnie załadowany przez klienta; uruchomienie zakończyło się kodem 0 i bez komunikatu o nieprawidłowym rozmiarze. Właściwy `players/male/grunt.png` wymaga potwierdzenia przez użytkownika.

### 5.4. Inicjalizacja `avelocities`

W `CL_FlyParticles` i `CL_BfgParticles` zastąpiono indeksowanie poza wewnętrznym wymiarem tablicy dwoma zagnieżdżonymi pętlami. Zachowano kolejność losowania i logikę, usuwając niezdefiniowane zachowanie zgłaszane przez GCC 13.

### 5.5. Awaria przy powrocie na wcześniej odwiedzoną mapę

Objaw po przejściach `base1 -> base2 -> base3 -> base2`:

```text
Hunk_Free: munmap failed (22)
Error: recursive error after: Hunk_Free: munmap failed (22)
```

Przyczyna znajdowała się w cache rozmiarów modeli w `ref_gl/gl_model.c`, a nie w systemie zapisywania NPC:

- przy pierwszym ładowaniu modelu wywoływany był `Hunk_End()`, który zapisuje w nagłówku mmap ostateczny rozmiar obszaru;
- przy kolejnym ładowaniu wykorzystywano `model_size_cache` i pomijano `Hunk_End()`;
- nagłówek nowego hunka pozostawał zatem z wartością zero z `Hunk_Begin()`;
- podczas zwalniania modeli `Hunk_Free()` wykonywał `munmap(..., 0)`, co Linux odrzuca z `EINVAL` (22).

Poprawka wywołuje `Hunk_End()` również w gałęzi cache, zachowując dotychczasowy mechanizm `model_size_cache`. Nie zmieniono formatu savegame ani logiki odtwarzania stanu encji.

Test integracyjny na rzeczywistych funkcjach z `linux/q_shlinux.c` wykonał 100 cykli: pierwsze ładowanie, ładowanie z cache i zwolnienie hunka — wszystkie zakończyły się powodzeniem. Test smoke klienta/OpenGL również zakończył się kodem 0. Następnie użytkownik potwierdził pełną sekwencję `base3 -> base2` bez awarii oraz poprawne, także powtórne, wczytanie zapisu `02`.

Wypisywane wcześniej `Function offsets 0` pochodzi z diagnostyki `ReadLevel()` na systemach innych niż Windows i przy niezmienionym module gry nie jest komunikatem o błędzie.

## 6. Aktualny status zagadnień (skondensowany)

- **Ślad cząsteczek blastera (`CL_BlasterTrail`):** ZAMKNIĘTE / OK — obserwowany wcześniej efekt przerw zniknął po wprowadzeniu natywnych rozdzielczości monitora; kod nie wymaga zmian.
- **Cross-build Windows 32-bit (`make win32` -> `binaries/win32/`):** POTWIERDZONE na Windows 11; dalsza rozbudowa menu/wideo wersji Win32 jest wstrzymana (obecny stan wystarcza).
- **Rozdzielczości SDL2, custom, fullscreen desktop i wybór monitora (Linux):** zaakceptowane na 3 monitorach (1440x900, 1920x1080, 3840x2160) w SP i MP.
- **Audio SDL2 (PulseAudio) i HTTP/libcurl + duże obrazy (do 4096x4096):** zaimplementowane i przetestowane.
- **Główny kolejny kandydat:** naprawa powielania ścieżki w `Sys_GetGameAPI` przy absolutnym `basedir` (`linux/sys_linux.c`) oraz drobne detale okna/wejścia na Linuksie.

## 7. Plan kolejnych etapów

### Etap 2 — natywny SDL2 zaimplementowany i potwierdzony w teście jednomonitorowym

Dodano `linux/gl_sdl2.c` i przełączono `binaries/ref_gl/Makefile` z `sdl-config` na `sdl2-config`. Stary `linux/gl_sdl.c` pozostaje w drzewie jako punkt odniesienia, ale nie jest kompilowany.

Zakres implementacji:

- `SDL_Window` i `SDL_GLContext` z jawnym OpenGL 2.1 compatibility profile;
- klasyczne rozdzielczości oraz fullscreen exclusive przez `SDL_SetWindowFullscreen` i `SDL_SetWindowDisplayMode`;
- zachowane `ref_gl.so`, QGL, `RW_IN_*`, `KBD_*`, `GetRefAPI` i `GetExtraAPI`;
- SDL2 keycodes/scancodes, key repeat, modyfikatory z licznikami oraz zwalnianie klawiszy przy utracie focusa;
- obsługa Mouse1–Mouse5, `SDL_MOUSEWHEEL`, relative mouse i `_windowed_mouse` bez zmiany znaczenia cvaru;
- kolejka zdarzeń z zachowaniem kolejności przy przepełnieniu;
- focus/minimize/close, Alt+Enter, Ctrl+G i reset starych delt myszy;
- VSync przez `SDL_GL_SetSwapInterval`, także aktualizowany w `GLimp_BeginFrame`;
- SDL2 window icon z poprawną paletą i kanałem alpha;
- symetryczne `SDL_InitSubSystem`/`SDL_QuitSubSystem(SDL_INIT_VIDEO)`, bez globalnego `SDL_Quit`;
- poprawne niszczenie kontekstu i okna przed `dlclose`, także przy błędach i restartach;
- joystick nadal wyłączony; bez zmian audio, cząsteczek, ABI i algorytmów renderera.

Celowo nie dodano: dynamicznych rozdzielczości, wyboru monitora, fullscreen desktop, High DPI, resize, Unicode/IME ani SDL_GameController.

Weryfikacja:

- pełna rekompilacja wszystkich komponentów po `distclean` zakończona powodzeniem;
- `ref_gl.so` linkuje `libSDL2-2.0.so.0`, nie zawiera zależności od SDL 1.2 ani symboli `SDL_WM_*`, `SDL_SetVideoMode`, `SDL_EnableKeyRepeat`, `SDL_GL_SwapBuffers`;
- wszystkie wymagane eksporty ABI są obecne;
- start OpenGL 800x600 zakończony kodem 0;
- start fullscreen exclusive zakończony kodem 0;
- trzy kolejne `vid_restart` zakończone kodem 0, z poprawnym ponownym utworzeniem kontekstu przy każdym ładowaniu;
- interaktywne wejście, Alt+Tab, Alt+Enter oraz oryginalne mapy wymagają testu użytkownika przed uznaniem etapu za w pełni zakończony.

### Etap 2.1 — mysz i wiele monitorów pod XFCE

Zgłoszenie użytkownika: w konfiguracji trzech monitorów gra startowała na głównym 1440x900, wszystkie ekrany na chwilę gasły, a obrót postaci kończył się na krawędzi ekranu. Problem występował także przy jednym monitorze.

Diagnoza:

- backend włączał `SDL_SetRelativeMouseMode` tylko przy `_windowed_mouse != 0`, którego domyślna wartość to `0`; mysz działała więc jak zwykły kursor ograniczony oknem;
- w `linux/vid_so.c` brakowało obecnej w źródłach id Software logiki `IN_Frame`, która przełącza wejście zależnie od `cl.refresh_prepped` i `cls.key_dest`;
- tryb bezramkowy nie jest wymagany do nieskończonego obrotu. Właściwym mechanizmem jest relative mouse podczas aktywnej rozgrywki;
- przygaszenie ekranów jest zgodne ze zmianą trybu przez fullscreen exclusive (`SDL_WINDOW_FULLSCREEN`), szczególnie pod wpływem XFCE/XRandR. Fullscreen desktop zostanie dodany w etapie 4 i powinien unikać takiej zmiany trybu.

Poprawki:

- przywrócono z referencyjnego `vid_so.c` aktywację wejścia tylko dla rozgrywki; menu/konsola zwalniają capture i pokazują kursor;
- fullscreen podczas rozgrywki zawsze używa `SDL_SetRelativeMouseMode(SDL_TRUE)` i `SDL_SetWindowGrab`;
- w trybie okienkowym capture jest kontrolowany przez `_windowed_mouse` (do pełnego obrotu w oknie ustawić `1`);
- `Ctrl+G` zachowuje możliwość ręcznego zwolnienia/przechwycenia myszy także w fullscreen;
- przy przełączeniach są zerowane stare delty, aby uniknąć skoku widoku.

Źródła wykorzystane przy diagnozie:

- dokumentacja SDL2 `SDL_SetRelativeMouseMode`: <https://wiki.libsdl.org/SDL2/SDL_SetRelativeMouseMode> — relative mode ukrywa kursor, ogranicza go do okna i raportuje ruch także na jego krawędzi;
- dokumentacja `SDL_SetWindowGrab`: <https://wiki.libsdl.org/SDL2/SDL_SetWindowGrab>;
- flagi `SDL_WINDOW_FULLSCREEN_DESKTOP`: <https://wiki.libsdl.org/SDL2/SDL_WindowFlags>;
- aktualny backend Yamagi Quake II `src/client/input/sdl2.c`: <https://github.com/yquake2/yquake2/blob/master/src/client/input/sdl2.c> — wykorzystuje zdarzenia relative motion oraz grab zależny od fullscreen/konfiguracji;
- zgłoszenie Yamagi/XFCE #586: <https://github.com/yquake2/yquake2/issues/586> oraz opis <https://discourse.libsdl.org/t/silly-xfce-feature-bug-breaks-relative-mouse-mode/28089> — znany problem XFCE `Wrap workspace when reaching the screen edge` może działać mimo gruba; diagnostycznym obejściem jest wyłączenie tej opcji albo `SDL_MOUSE_RELATIVE_MODE_WARP=1 ./quake2`. Nie włączamy warp mode globalnie, ponieważ zmienia sposób pozyskiwania ruchu i nie jest potrzebny do podstawowej poprawki.

Build i smoke fullscreen po poprawce przechodzą. Użytkownik potwierdził responsywną mysz, pełne sterowanie, zwalnianie kursora w konsoli i działanie Alt+Tab na jednym monitorze. Układ trzech monitorów pozostaje do przetestowania w etapie fullscreen desktop.

### Etap 3 — audio SDL2 zaimplementowane i zaakceptowane

Dodano `linux/snd_sdl2.c`, zastępując w buildzie stary OSS `linux/snd_linux.c`. Klient jest teraz kompilowany i linkowany również z `sdl2-config`.

Architektura:

- `SDL_OpenAudioDevice` z domyślnym urządzeniem systemowym;
- 16-bit stereo i częstotliwości z `s_khz`: 11025, 22050, 44100 lub 48000 Hz;
- współdzielony ring buffer o potędze dwójki, około 1–2 sekund, bez zmiany `s_mixahead`;
- callback SDL wyłącznie kopiuje próbki i przesuwa pozycję — nie wywołuje funkcji silnika;
- `SNDDMA_BeginPainting`/`Submit` używają `SDL_LockAudioDevice` z licznikiem zagnieżdżeń;
- `SNDDMA_GetDMAPos` zwraca pozycję w próbkach kanałowych zgodnie z `GetSoundtime`;
- `dma.samples`, `submission_chunk`, `samplebits` i jednostki ramek są zgodne z maskami miksera;
- urządzenie jest zamykane przed zwolnieniem ringu; shutdown jest idempotentny;
- tylko `SDL_InitSubSystem`/`SDL_QuitSubSystem(SDL_INIT_AUDIO)`, bez zamykania wideo ani globalnego `SDL_Quit`;
- błąd urządzenia nie blokuje dalszego uruchomienia gry.

W `client/snd_dma.c` przeniesiono liczniki wrapów do stanu globalnego i resetowano je przy inicjalizacji, aby `snd_restart` nie wracał do starej epoki czasu. Drzewo `knownsounds` jest ponownie używane przy wyłączonym/nieudanym audio i zerowane po zniszczeniu, co zapobiega wyciekom przy kolejnych inicjalizacjach.

Testy techniczne:

- start i wyjście z `SDL_AUDIODRIVER=dummy`: kod 0;
- wszystkie częstotliwości 11/22/44/48 kHz oraz `snd_restart`: kod 0 i poprawne parametry;
- kilka restartów dźwięku oraz `vid_restart` bez zamykania audio;
- testowy WAV 440 Hz odtworzony przez mikser; `soundinfo` pokazało postęp `samplepos` z 0 do 14336, a po kolejnym restarcie wideo do 17408;
- rzeczywiste otwarcie sterownika `pulseaudio` zakończone sukcesem;
- nieprawidłowy sterownik audio daje czytelny komunikat, ale klient nadal uruchamia OpenGL i wychodzi kodem 0;
- użytkownik potwierdził odsłuch oryginalnej gry na PulseAudio, wielokrotne `snd_restart` przy różnych częstotliwościach, `vid_restart`, wczytanie zapisu `02` i przejście `gamemap q2dm1`.

Po akceptacji odsłuchu domyślna wartość została zmieniona na `s_initsound 1`. Możliwość wyłączenia audio przez `s_initsound 0` została zachowana.

### Etap 4 — rozdzielczości i monitory zaimplementowane i zaakceptowane

Zmiany w backendzie i kliencie:

- `vid_fullscreen`: `0` — okno, `1` — fullscreen exclusive, `2` — fullscreen desktop bez zmiany trybu monitora;
- `vid_display` wybiera monitor SDL; numer jest zgodny z aktualną enumeracją systemu i może się zmienić po przełączeniu sprzętu;
- `gl_mode -1` używa custom `vid_width`/`vid_height`, a `gl_mode -2` aktualnej rozdzielczości wybranego monitora;
- presety 800x600 do 3840x2160, w tym 1280x720, 1366x768, 1600x900, 1920x1080, 2560x1440, 3440x1440 i 3840x2160;
- menu wideo zawiera wszystkie presety, `desktop`, `custom`, wybór fullscreen i listę monitorów z nazwą/rozdzielczością;
- pola custom width/height z zakresami 800–16384 i 600–16384;
- centrowanie na wybranym monitorze i ograniczenie okna do `SDL_GetDisplayUsableBounds`, aby nie wychodziło poza widoczny obszar;
- `SDL_WINDOW_ALLOW_HIGHDPI` i `SDL_GL_GetDrawableSize` synchronizują rzeczywisty rozmiar renderowania z `vid.width/height`;
- zmiana `vid_display`, custom width/height, trybu lub fullscreen powoduje kontrolowany restart wideo, a flagi modified są czyszczone po `R_SetMode`;
- Alt+Enter przełącza okno i fullscreen desktop;
- nie zmieniono `refimport_t` ani ABI modułu renderera.

Testy na rzeczywistym układzie XFCE/XRandR:

| `vid_display` | Monitor | Test | Wynik |
|---|---|---|---|
| 0 | DP-3, główny 1440x900 | custom windowed 1920x1080 | poprawnie ograniczone do widocznych 1440x860 |
| 0 | DP-3 | desktop fullscreen 1440x900 | kod 0, poprawny drawable |
| 0 | DP-3 | exclusive 1920x1080 | kod 0, poprawny drawable |
| 1 | HDMI-0 3840x2160 | desktop fullscreen 3840x2160 | kod 0, działa 4K |
| 1 | HDMI-0 | `vid_restart` 3840x2160 | kod 0, monitor i rozmiar zachowane |
| 2 | DP-0 1920x1080 | desktop fullscreen 1920x1080 | kod 0, poprawny wybór monitora |

Pełna rekompilacja po `distclean` zakończyła się powodzeniem. Użytkownik potwierdził zadowolenie z efektu końcowego oraz uruchomił grę multiplayer na serwerze zewnętrznym. Log potwierdza kolejno tryby 1440x900 na `vid_display 0`, 1920x1080 na `vid_display 2` i 3840x2160 na `vid_display 1`. Fullscreen exclusive nadal może powodować przygaszenie monitorów; do pracy wielomonitorowej rekomendowane jest `vid_fullscreen 2`.

### Etap 5 — poprawność, UI, cząsteczki, optymalizacja

- selektywne poprawki poprawności ścieżek i osobny build ASan/UBSan;
- priorytet dla rzeczywistych błędów pamięci, nie masowe czyszczenie ostrzeżeń;
- osobny etap UI: skalowanie HUD dla 1440p/4K (`gl_hudscale` już istnieje) oraz niezależne skalowanie crosshair względem HUD, z zachowaniem obecnego wyglądu jako ustawienia domyślnego;
- porównanie cząsteczek z baseline sprzed SDL2;
- testy `cl_async`, point parameters/sprites i FPS;
- profilowanie przed optymalizacją;
- brak planu migracji na Vulkan/OpenGL core na tym etapie.

## 8. Checklist po każdym etapie

1. Pełna lub kontrolowana rekompilacja bez nowych błędów.
2. Start do menu z oryginalnymi zasobami.
3. `map q2dm1`, ruch, klawiatura, mysz i blaster.
4. `map base1` oraz start easy/medium z menu.
5. Save/load i przejście do kolejnej mapy po zmianach singleplayera.
6. Zmiana rozdzielczości, fullscreen/windowed i kilkukrotny `vid_restart`.
7. Alt+Tab, focus i przechwytywanie myszy po SDL2.
8. Wyjście, ponowny start i kontrola konfiguracji.
9. Aktualizacja tego dokumentu: zmiana, wynik testu, nowe problemy, kolejny krok.

## 9. Dziennik sesji

### 2026-11-06 — pierwszy działający build

- Naprawiono kompilację GCC 13/x86_64 i biblioteki obrazów.
- Uruchomiono SDL 1.2/OpenGL na NVIDIA.
- Ustawiono OpenGL jako jedyny renderer, minimum 800x600 i wyłączony dźwięk.
- Użytkownik potwierdził multiplayer `q2dm1` oraz działanie wejścia.

### 2026-11-06 — etap 1, baseline przed SDL2

- Dodano trwałą dokumentację i instrukcje kontynuacji dla kolejnych sesji AI.
- Naprawiono menu nowej gry, 64-bitowe porównanie checksumy i duże PNG.
- Usunięto niezdefiniowane indeksowanie `avelocities`.
- Pełna rekompilacja `distclean` + build przechodzi; test dużego PNG i konwersji checksumy przechodzi.
- Oczekiwany test użytkownika z oryginalnymi mapami i teksturami.

### 2026-11-06 — testy singleplayera i powrót na mapę

- Użytkownik potwierdził działanie `base1` oraz przejść `base1 -> base2 -> base3`.
- Przywrócono `newgame` jako konfigurowalny punkt rozszerzenia; pozostawiono poprawione sprawdzanie `maps/base1.bsp`.
- Zdiagnozowano pominięcie `Hunk_End()` w cache modeli jako przyczynę `munmap failed (22)` przy powrocie na mapę.
- Dodano finalizację hunka w gałęzi cache i przetestowano 100 cykli alokacji/zwalniania na rzeczywistym kodzie Linux.
- Pełna rekompilacja i test smoke klienta przechodzą; pełny test powrotu mapy oraz `newgame` oczekuje na użytkownika.

### 2026-11-06 — zakończenie etapu 1

- Użytkownik potwierdził `newgame` i uruchomienie `base1`.
- Sekwencja `base1 -> base2 -> base3 -> base2` działa bez błędu `munmap`.
- Zapisy `01` i `02` zostały utworzone; zapis `02` został poprawnie wczytany dwukrotnie.
- Etap 1 uznajemy za zakończony. Następny krok: natywny SDL2 bez zmiany funkcji.

### 2026-11-06 — implementacja natywnego SDL2

- Potwierdzono dostęp do źródeł referencyjnych id Software w `reference/quake2idsoftware/`.
- Dodano `linux/gl_sdl2.c`; stary SDL 1.2 pozostawiono jako niekompilowany baseline.
- Przeniesiono okno, kontekst GL, wejście, focus, fullscreen, ikonę i VSync na SDL2.
- Pełny build, start windowed/fullscreen i trzy restarty wideo przechodzą testy smoke.
- Oczekiwane testy użytkownika: klawiatura, mysz, Alt+Tab, Alt+Enter, `q2dm1`, `base1` i powrót na mapę.

### 2026-11-06 — XFCE, trzy monitory i ograniczony ruch myszy

- Zweryfikowano aktualną dokumentację SDL2 i implementację Yamagi Quake II oraz znany problem XFCE workspace wrapping.
- Zidentyfikowano brak aktywacji wejścia w `IN_Frame` i zbyt restrykcyjny warunek `_windowed_mouse` jako przyczyny ograniczonego obrotu.
- Przywrócono przełączanie capture zależnie od stanu gry; fullscreen używa relative mouse, menu/konsola pokazują kursor.
- Zachowano `_windowed_mouse` dla gry w oknie i działanie `Ctrl+G` jako ręcznego override.
- Build i smoke fullscreen przechodzą; oczekiwany test pełnego obrotu i zachowania trzech monitorów.

### 2026-11-06 — akceptacja SDL2 i implementacja audio

- Użytkownik potwierdził poprawną, responsywną mysz, zwalnianie kursora przez konsolę i Alt+Tab na jednym monitorze.
- Dodano backend `linux/snd_sdl2.c` i przeniesiono klienta na SDL2 audio, zachowując stary OSS tylko jako plik referencyjny.
- Zaimplementowano ring buffer, synchronizację callbacku, poprawne jednostki DMA, zamykanie urządzenia i reset wrapów przy `snd_restart`.
- Testy dummy obejmują start, cztery częstotliwości, restarty, odtworzenie WAV i postęp `samplepos`; PulseAudio otwiera się poprawnie.
- Oczekiwany odsłuch oryginalnej gry z `s_initsound 1`; po akceptacji zmienimy domyślną wartość i przejdziemy do dynamicznych trybów ekranu.

### 2026-11-06 — zakończenie etapu audio

- Użytkownik potwierdził zadowolenie z działania dźwięku na PulseAudio.
- Testy obejmują wiele restartów przy 11025/22050/44100 Hz, `vid_restart`, save/load i `gamemap q2dm1`.
- Domyślne `s_initsound` ustawiono na `1`; start na świeżej konfiguracji potwierdził inicjalizację audio bez dodatkowych parametrów, a `s_initsound 0` pozostaje dostępne jako fallback.
- Etap 3 zakończony. Następny krok: fullscreen desktop, dynamiczne rozdzielczości i wybór monitora.

### 2026-11-06 — implementacja rozdzielczości i monitorów

- Dodano fullscreen desktop, custom width/height, tryb natywny monitora, presety do 4K i wybór `vid_display`.
- Menu wideo wyświetla monitory SDL, nowe rozdzielczości i pola custom.
- Okno jest centrowane i ograniczane do usable bounds; drawable obsługuje High DPI.
- Testy potwierdziły wszystkie trzy monitory, desktop 4K, exclusive 1080p oraz restart wideo.
- Pełny build przechodzi. Oczekiwana interaktywna akceptacja menu i rozgrywki przed przejściem do cząsteczek/optymalizacji.

### 2026-11-06 — osiągnięcie celu i przekazanie projektu

- Użytkownik potwierdził zadowolenie z efektu końcowego i osiągnięcie celu.
- Gra połączyła się z zewnętrznym serwerem multiplayer; dźwięk PulseAudio działa podczas rozgrywki.
- W praktyce potwierdzono fullscreen desktop 1440x900, 1920x1080 i 3840x2160 na wszystkich trzech monitorach.
- Kolejny etap będzie dotyczył opcjonalnych detali i funkcji, nie ratowania podstawowej działalności projektu.
- Dodano `NEXT_SESSION_PROMPT.md` jako krótki punkt wejścia dla nowej sesji AI.

Nie wykonano commitów Git. Zmiany są zapisane w drzewie roboczym repozytorium.

### 2026-11-13 — etap 5A: powtarzalny build i higiena repozytorium

- Kontrola środowiska wykazała brak pakietów deweloperskich mimo obecnych, wcześniej zbudowanych binarek. Użytkownik zainstalował SDL2/OpenGL/PNG/JPEG oraz dodatkowo `libglu1-mesa-dev`, wymagany przez `GL/glu.h` w rendererze.
- Przed czyszczeniem zachowano cztery binarki i sumy SHA-256 w `/tmp/opencode/r1q2-baseline-bq9xCk`.
- Dodano `.gitignore` dla `.o`, `.depends`, artefaktów buildu i zewnętrznych drzew `reference/`; nowe źródła i dokumentacja pozostają widoczne dla Git.
- Pełne `distclean` + `make -C binaries -j"$(nproc)"` zakończyło się kodem 0. Kolejny build przyrostowy również zakończył się kodem 0 i nie znalazł pracy do wykonania.
- Wszystkie cztery artefakty są świeżymi plikami ELF x86-64, nie mają nierozwiązanych zależności runtime, a `ref_gl.so` eksportuje `GetRefAPI`.
- `r1q2ded +quit` uruchomiony w `/tmp/opencode/r1q2-ded-smoke-final` zakończył się kodem 0.
- Log pełnej kompilacji: `/tmp/opencode/r1q2-full-build-20261113-final.log`. Build nadal wypisuje 230 ostrzeżeń w starym kodzie; nie włączano `-Werror` ani nie wykonywano masowego czyszczenia.
- Oczekiwany test użytkownika z oryginalnymi zasobami: start do menu, `map q2dm1`, ruch i blaster, audio, `vid_restart`, save/load oraz wyjście/ponowny start. Nie instalowano nowych binarek w `/home/user/.quake2`.
- Kolejny etap po akceptacji baseline: selektywne poprawki poprawności ścieżek i osobny build ASan/UBSan.
- Pobieranie HTTP pozostaje osobnym późniejszym etapem. Ustalono porównanie obsługi `dlserver` i ścieżek pobierania z Q2Pro, z zachowaniem kontraktów R1Q2, protokołu i fallbacku UDP; nie przenosimy mechanicznie całego subsystemu Q2Pro.

### 2026-11-13 — akceptacja etapu 5A i rozszerzenie zadań UI

- Użytkownik potwierdził, że świeży build działa poprawnie z oryginalnymi zasobami.
- Test objął start do menu, PulseAudio 44100 Hz, `map base1`, ruch i blaster, save/load `01`, wyjście i ponowny start oraz kilkukrotne `vid_restart`.
- W logach potwierdzono custom 1440x900, fullscreen desktop 1920x1080 na `vid_display 2`, przejścia rozdzielczości i ponowną inicjalizację renderera bez błędu.
- `couldn't exec r1gl.cfg` pozostaje komunikatem o braku opcjonalnej konfiguracji, a `Function offsets 0` znaną diagnostyką `ReadLevel()`; nie są to nowe regresje.
- Do listy zadań dodano osobny etap UI: skalowanie HUD oraz niezależne skalowanie crosshair względem HUD, z zachowaniem obecnego wyglądu jako ustawienia domyślnego.
- Etap 5A jest zaakceptowany. Następny etap to selektywne poprawki poprawności ścieżek i osobny build ASan/UBSan; HTTP i UI pozostają odseparowanymi zadaniami.

### 2026-11-13 — etap 5B: poprawność i ASan/UBSan

- `Sys_FindFirst`/`Sys_FindNext` w `linux/q_shlinux.c`: dodano walidację długości wejścia i wspólne, sprawdzone sklejanie ścieżki przez `Sys_MakePath`. Zbyt długie ścieżki są odrzucane zamiast ucinanych, aby nie wykonywać operacji na niewłaściwych plikach. Zachowano globowanie, atrybuty i iterację.
- Przed poprawką test odtworzył `buffer overflow detected` dla 128-znakowego wejścia oraz ASan `stack-buffer-overflow` dla ścieżki wynikowej. Po poprawce przechodzą zwykłe wyszukiwanie, filtry katalogów, oba przypadki zbyt długich ścieżek i granica 127/128 znaków.
- Hunk na LP64: nagłówek zajmuje teraz `sizeof(void *)`, więc na x86-64 wszystkie alokacje zaczynają się od wyrównania 8, zgodnego z zaokrągleniem do 32. `Hunk_End` nadal zwraca wyłącznie rozmiar danych. Usunięto nie wyrównane dostępy m.in. w `Mod_LoadTexinfo`; 100 cykli `Hunk_Begin`/`Alloc`/`End`/`Free` przechodzi pod ASan.
- `ref_gl/gl_image.c`: maska alpha używa `255U<<24`, a zamiana rozszerzeń TGA/PNG/JPG używa `memcpy(..., 4)` zamiast nie wyrównanych i zależnych od aliasingu zapisów przez `int *`. Kolejność ładowania i zawartość nazw pozostają bez zmian.
- `qcommon/common.c`: strażnik `0xFDFEFDFE` alokacji game jest zapisywany i sprawdzany przez `memcpy`/`memcmp`; nie zmieniono rozmiaru bloków ani mechanizmu wykrywania nadpisań. `MSG_ReadLong` składa bajty jako `uint32`, zachowując również `0x80000000` i `0xffffffff`.
- `qcommon/net_chan.c`, `server/sv_ents.c`, `client/cl_ents.c` i `server/sv_world.c`: maski bitowe 31, statystyk i `solid2` używają arytmetyki bez znaku. Bajty protokołu pozostają niezmienione.
- `game/q_shared.h`: `crand()` wykonuje odejmowanie modulo 2^32 na `uint32`. Test 100008 próbek potwierdził identyczne wyniki ze starą formułą skompilowaną z `-fwrapv`; rozkład efektów nie został celowo zmieniony.
- `client/snd_mem.c`: skalowanie próbki 8-bitowej do 16-bitowej używa mnożenia przez 256 zamiast UB przy przesuwaniu wartości ujemnej. `ref_gl/gl_rsurf.c`: dodano granicę `map < MAXLIGHTMAPS` przed odczytem stylu.
- Osobny build sanitizerów znajduje się wyłącznie w `/tmp/opencode/r1q2-correctness/san-tree` i używa `CC='gcc -fsanitize=address,undefined -fno-omit-frame-pointer'`; release’owe obiekty nie są nadpisywane.
- Testy jednostkowe w `/tmp/opencode/r1q2-correctness`: 5 testów ścieżek/hunk, granice `MSG_ReadLong` i 100008 próbek `crand` — wszystkie przechodzą.
- Pełny czysty build release zakończył się kodem 0 (218 historycznych ostrzeżeń zamiast 230), a pełny czysty build ASan/UBSan również kodem 0. Wszystkie cztery artefakty release są obecne i nie mają brakujących zależności runtime.
- Runtime sanitizerów w `/tmp/opencode/r1q2-correctness/san-client-runtime` używa prawdziwych paków podlinkowanych tylko do odczytu, dummy audio i NVIDIA OpenGL. Katalog `/home/user/.quake2` nie był modyfikowany.
- Z `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` i `UBSAN_OPTIONS=halt_on_error=1` przechodzą: start/wyjście, `base1`, save/load `san04`, `base1 -> base2 -> base3 -> base2` oraz `vid_restart` z dwoma potwierdzonymi załadowaniami `ref_gl.so`. Brak raportów ASan/UBSan w końcowych scenariuszach. Nie jest to deklaracja analizy wycieków — LSan był wyłączony.
- Końcowe logi: `/tmp/opencode/r1q2-correctness/final-release-clean-build.log`, `/tmp/opencode/r1q2-correctness/final-sanitizer-clean-build.log`, `/tmp/opencode/r1q2-correctness/san-client-runtime/client-final-clean-sanitizer.log`.
- Oczekiwany test użytkownika z release: start, `base1` i `q2dm1`, ruch i blaster/eksplozje, audio, save/load, przejścia map, `vid_restart` oraz multiplayer zewnętrzny dla regresji protokołu.
- Kolejny etap po akceptacji: HTTP z porównaniem ścieżek `dlserver` z Q2Pro. Nie zmieniano HTTP, HUD ani crosshair. `git diff --check` przechodzi i nie wykonano commitów.

### 2026-11-13 — akceptacja etapu 5B i rozpoczęcie analizy HTTP

- Użytkownik zaakceptował build po poprawkach ASan/UBSan. Test objął `base1`, `base1 -> base2 -> base3 -> base2`, save/load `01`, `q2dm1`, efekty i audio PulseAudio, `gl_hudscale 2`, restarty wideo 1080p/4K oraz wyjście i ponowny start.
- Połączenie z zewnętrznym serwerem `23.227.170.222:27916` zakończyło się załadowaniem `q2dm2` i rzeczywistą rozgrywką multiplayer. Nie potwierdzono regresji protokołu po zmianach masek.
- Serwery wymagające `ven_dm3.bsp` i `q2rdm2.bsp` nie mogły pobrać brakujących map, ponieważ klient nadal jest zbudowany bez `USE_CURL`. To potwierdza zakres kolejnego etapu, a nie regresję etapu 5B.
- Rozpoczęto analizę HTTP. Wzorzec Q2Pro dla zwykłych plików to `dlserver + katalog_moda + "/" + quakePath`; lokalna ścieżka zapisu pozostaje pod kontrolą `FS_Gamedir()`. Obecne użycie `FS_Gamedir()` do budowy URL-a R1Q2 działa przypadkowo przy `./baseq2`, ale jest niepoprawne przy absolutnym `basedir`.
- `libcurl4-openssl-dev` nie jest jeszcze zainstalowany. Nie zmieniono kodu HTTP w tym wpisie.

### 2026-11-13 — etap 6: pobieranie HTTP przez libcurl

- Użytkownik zainstalował `libcurl4-openssl-dev` 8.5.0. `binaries/client/Makefile` kompiluje `client/cl_http.c` z `USE_CURL` i korzysta z `pkg-config` dla libcurl. Pełny `distclean` był konieczny, ponieważ Make nie śledzi zmiany `CFLAGS` i pierwszy build przyrostowy pozostawił stare obiekty bez integracji HTTP.
- Ścieżki są zgodne z Q2Pro: zdalny URL to `dlserver + CL_HTTPGamedir() + "/" + quakePath`, a lokalny zapis to `FS_Gamedir()/quakePath.tmp`. Pusty `cl.gamedir` oznacza `baseq2`, nie pusty wskazuje katalog moda. Lokalna ścieżka `basedir` nigdy nie jest wysyłana do serwera.
- Listy mają poprawne postacie `baseq2.filelist` i `baseq2/maps/testhttp.filelist`; dla moda analogicznie `testmod.filelist` i `testmod/maps/testhttp.filelist`. Escaping zachowuje wyłącznie znaki alfanumeryczne oraz `/-_.~`, pozostałe koduje procentowo.
- Przed poprawką test z absolutnym `basedir` wysyłał `/tmp/opencode/.../baseq2/...` i kończył się 404. Po poprawce ten sam scenariusz pobiera mapę i ładuje ją bez żadnej lokalnej ścieżki w URL.
- `CL_SetHTTPServer` akceptuje wyłącznie HTTP/HTTPS, ogranicza długość URL, sprawdza inicjalizację curl i respektuje `cl_http_downloads 0`. Transfery i przekierowania są ograniczone do HTTP/HTTPS. Dodano timeout połączenia 10 s, próg zawieszenia 1 KiB/s przez 30 s i `CURLOPT_NOSIGNAL`.
- Listy są ograniczone do 1 MiB, mają kontrolowane realokacje, obsługę pustej zawartości i poprawne zwalnianie bufora. Zbyt długie ścieżki oraz błędy startu transferu kończą żądanie bez pozostawiania zawieszonego `pendingCount`.
- Zaktualizowano API do `CURLOPT_XFERINFOFUNCTION`, `CURLINFO_SIZE_DOWNLOAD_T`, `CURLOPT_PROTOCOLS_STR` i `CURLOPT_REDIR_PROTOCOLS_STR`. Końcowy build nie zawiera ostrzeżeń `cl_http.c` ani deprecated API.
- Testy lokalne obejmują: zwykły download i 404, puste listy, przekierowanie 302 do `/mirror/...`, katalog `testmod`, absolutny `basedir` oraz dwie listy po 2 MiB. Limit kończy się kontrolowanym `Maximum file size exceeded` bez raportów ASan/UBSan.
- Fallback UDP został potwierdzony dwukrotnie: `dlserver=file:///...` jest odrzucane, a `cl_http_downloads 0` nie generuje żadnych zapytań HTTP. W obu przypadkach pełna mapa została pobrana przez protokół Quake II i załadowana.
- Test na zewnętrznym serwerze `217.182.73.153` pobrał `openffa/maps/ztn2dm1.bsp` (361376 bajtów) z `http://217.182.73.153/downloads/openffa/...`, załadował mapę i połączył gracza z serwerem. Plik zapisano wyłącznie w `/tmp/opencode/r1q2-http-live/openffa/maps/ztn2dm1.bsp`.
- Pełny czysty build release i pełny czysty build ASan/UBSan zakończyły się kodem 0. Finalny lokalny test release zakończył się `Player entered the game`, a finalny sanitizerowy test przekroczonej listy również kodem 0 i bez raportów.
- Podczas jednego 20-sekundowego shutdownu w trakcie ładowania zasobów X zgłosił `GLXBadContextState`; mapa była już pobrana. Ponowiony 45-sekundowy test i wszystkie kolejne testy zakończyły się normalnie. Nie zmieniano kodu wideo; zachowanie pozostaje do obserwacji jako niezależny edge case shutdownu.
- `/home/user/.quake2` nie było modyfikowane. Logi i dane testowe są w `/tmp/opencode/r1q2-http-test`, `/tmp/opencode/r1q2-http-live` i `/tmp/opencode/r1q2-correctness` (`http-final-release-clean.log`, `http-final-sanitizer-clean.log`).
- Oczekiwany test użytkownika: połączenie z serwerem wymagającym mapy, automatyczne pobranie i wejście do gry, audio oraz opcjonalnie `cl_http_downloads 0` dla potwierdzenia UDP na własnej konfiguracji.
- Po akceptacji kolejny wybór to osobny etap: analiza śladu blastera albo skalowanie HUD i niezależne skalowanie crosshair. `git diff --check` przechodzi i nie wykonano commitów.

### 2026-11-13 — etap 6A: duże obrazy i bezpieczny fallback

- Zgłoszony przez użytkownika błąd `GL_Upload8: players/makuta/makuta.pcx: 640x480 too large` wynikał ze stałego bufora konwersji na 131072 piksele. Obraz ma 307200 pikseli; nie jest uszkodzony i nie przekracza późniejszego limitu skalowania `GL_Upload32`.
- `GL_Upload8` zachowuje mały bufor stosu dla typowych obrazów, a dla większych przydziela pamięć dynamicznie według `size_t`. Po uploadzie bufor jest zwalniany. Istniejący renderer nadal skaluje obraz do swojego limitu, więc poprawny skin nie musi być pomijany.
- `GL_LoadPic` odrzuca nieprawidłowe wymiary oraz obrazy powyżej 4096×4096 pikseli, wybierając `r_notexture` z czytelnym ostrzeżeniem zamiast rozłączenia. Nie zmieniono obsługi mapy BSP ani nie wyłączono błędów krytycznych struktury gry.
- Test przed poprawką odtworzył `ERROR: GL_Upload8 ... 640x480 too large` z `model makuta` i `skin makuta/makuta`. Po zmianie ten sam przypadek przechodzi w release i pod ASan/UBSan bez zgłoszeń.
- Fallback przetestowano poprawnym WAL 4096×4097 na mapie z teksturą `oversize`, której nie ma w pakietach. `GL_LoadPic` wybrał `r_notexture`, mapa została załadowana, a ASan/UBSan nie zgłosił błędów.
- Test na zewnętrznym `23.227.170.222:27916` pobrał `players/makuta/makuta.pcx` i pozostałe modele, a klient wszedł do gry `q2dm6` (`Player entered the game`) bez błędu `GL_Upload8`. Zapis odbył się wyłącznie w `/tmp/opencode/r1q2-http-live`.
- Pełne czyste buildy release i ASan/UBSan zakończyły się kodem 0. Logi: `/tmp/opencode/r1q2-correctness/oversize-final-release-clean.log`, `/tmp/opencode/r1q2-correctness/oversize-final-sanitizer-clean.log`, `/tmp/opencode/r1q2-http-live/live-oversize-makuta.log`.
- Niezależna uwaga do późniejszej analizy: lokalna gra z absolutnym `basedir` ujawniła powielenie ścieżki przy ładowaniu `gamex86_64.so` w `Sys_GetGameAPI`. Nie dotyczy to pobierania HTTP ani obrazów i nie było zmieniane w tym etapie.
- Oczekiwany test użytkownika: wejście na serwer z `makuta.pcx` bez `GL_Upload8 ... too large`, poprawne modele i dalsza rozgrywka. `/home/user/.quake2` nie było modyfikowane i nie wykonano commitów.



### 2026-11-13 — etap 7: porządki buildu i dokumentacja użytkownika

- Główny `Makefile` jest teraz działającym wrapperem dla `make -C binaries`: `make`, `make clean`, `make distclean`, `make sanitize` i `make help` działają z katalogu głównego.
- `binaries/client/Makefile` obsługuje `USE_CURL=1` domyślnie oraz `USE_CURL=0` bez `cl_http.c` i zależności od libcurl. Dodano walidację wartości oraz zmienne `PKG_CONFIG` i `SDL2_CONFIG`.
- `binaries/make.inc` usuwa przy `clean` wszystkie `.o`, także obiekty wyłączonych backendów, dodaje `.DELETE_ON_ERROR` i zależność obiektów od `Makefile`/`make.inc`. Usunięto podwójne dołączanie zależności `.d`.
- Dodano `make sanitize`, który kopiuje źródła i buduje ASan/UBSan wyłącznie pod `/tmp/opencode`. Ścieżka spoza tego katalogu jest odrzucana; release’owe obiekty i binarki pozostają nietknięte.
- Dodano główne `README.md`: funkcje, wymagania, build, instalacja, konfiguracja SDL2, HTTP, duże obrazy, testy, struktura repo i ograniczenia. Oryginalny `readme.txt` pozostaje bez zmian.
- Zaktualizowano `AGENTS.md`, `NEXT_SESSION_PROMPT.md` i sekcję kompilacji w tym dokumencie. Zmienne `USE_CURL=0` i target `sanitize` są opisane także dla kolejnych sesji.
- Weryfikacja: główny `make` i build przyrostowy kodem 0; `USE_CURL=0` bez `cl_http.o` i bez libcurl w `ldd`; przywrócony build domyślny z `libcurl.so.4`; `make sanitize` kodem 0, sanitizerowy `r1q2ded +quit` kodem 0; `USE_CURL=2` i niebezpieczna `SANITIZE_DIR` są odrzucane.
- Sumy czterech binarek release przed i po `make sanitize` są identyczne. Stare `snd_linux.o` i `gl_sdl.o` zostały usunięte. `make help` przechodzi, `git diff --check` przechodzi i nie wykonano commitów.
- Logi: `/tmp/opencode/r1q2-correctness/makefile-reorg-build.log`, `makefile-no-curl-build.log`, `makefile-default-restored-build.log`, `make-sanitize-target.log`, `makefile-final-incremental.log` i `makefile-final-build.log`.
- Następny wybór po akceptacji: analiza śladu blastera albo skalowanie HUD i niezależne skalowanie crosshair.

### 2026-11-15 — etap 8: cross-kompilacja dla Windows 32-bit (i686-w64-mingw32)

- Zbudowano w `/tmp/opencode/win32-deps` statyczne biblioteki 32-bitowe dla `i686-w64-mingw32`: `libpng 1.6.43`, `libjpeg-turbo 2.1.5` oraz `libcurl 8.5.0` (z natywnym backendem TLS Schannel i wątkami Win32 bez zależności od `libwinpthread`).
- Dostosowano kod `win32/` i wspólne nagłówki do kompilatora MinGW-w64 GCC z zachowaniem zgodności z MSVC (`_MSC_VER`) oraz Linuxem:
  - `game/q_shared.h`: pod `_WIN32` zachowano `__attribute__` dla GCC (co zapobiega wycinaniu atrybutu `__stdcall` z nagłówków Win32/OpenGL), zmapowano `_ReturnAddress()` na `__builtin_return_address(0)` i ograniczono `#pragma intrinsic` oraz `__declspec(restrict)` do `_MSC_VER`.
  - `qcommon/qcommon.h`, `win32/q_shwin.c`, `client/snd_mix.c`: bloki asemblera inline MSVC (`__asm`) ograniczono do `_MSC_VER`, wybierając dla GCC przenośne warianty C.
  - `win32/sys_win.c`, `win32/vid_dll.c`: poprawiono wielkość liter `<richedit.h>` i ukośniki w `#include "../client/client.h"`, ujednolicono typ `qboolean ActiveApp`, dodano wariant GCC dla `Sys_GetFPUStatus`/`Sys_SetFPU` (zachowujący bitową maskę słowa kontrolnego x87) i uwarunkowano bloki SEH `__try`/`__except` przez `#ifdef _MSC_VER`.
  - `win32/snd_win.c`, `win32/vid_menu.c`, `win32/glw_imp.c`: ujednolicono prototypy `SNDDMA_InitDirect`, `SNDDMA_Init`, `GLimp_SetMode` i `GLimp_Init` oraz zmieniono nazwę lokalnego `DriverCallback` na `VID_DriverCallback` (konflikt z `<mmiscapi.h>`).
- Dodano `binaries/Makefile.win32` i target `make win32`, który tworzy w `binaries/win32/` samowystarczalne binarki PE32 (`r1q2.exe`, `quake2.exe`, `ref_gl.dll`, `ref_r1gl.dll`, `gamex86.dll`, `r1q2ded.exe`) ze statycznie wlinkowanymi `libpng`, `libjpeg`, `libcurl`, `zlib` i `libgcc`.
- Weryfikacja: zarówno `make -j"$(nproc)" win32`, jak i czysty build Linuxowy (`make distclean && make -j"$(nproc)"`) zakończyły się kodem 0; tablice eksportów PE zawierają `GetRefAPI`, `GetExtraAPI` i `GetGameAPI`, a importy ograniczają się wyłącznie do systemowych bibliotek Windows.
- Test użytkownika: potwierdzono poprawne uruchomienie i działanie gry w systemie Windows 11. Dalsze zrównywanie warstwy `win32/` z `linux/` odłożone na później.
- Dodatkowo użytkownik potwierdził, że ślad blastera (`CL_BlasterTrail`) jest prawidłowy (wcześniejszy artefakt zniknął po wprowadzeniu natywnych rozdzielczości monitora) — temat zamknięty.
- Następny etap do wyboru w kolejnej sesji: skalowanie HUD (`gl_hudscale`) oraz niezależne skalowanie crosshair dla 1440p/4K.

### 2026-11-16 — etap 9: naprawa `gl_hudscale` po `vid_restart`, niezależne `ch_scale` i menu Video

- Przyczyna błędu po `vid_restart` przy `gl_hudscale > 1`: `UpdateDrawableSize()` w `linux/gl_sdl2.c` wywoływało `ri.Vid_NewWindow(vid.width, vid.height)` z pełnym rozmiarem fizycznym zamiast podzielonym przez `gl_hudscale->value` (w odróżnieniu od `win32/glw_imp.c`), a `Cvar_Get("gl_hudscale", ...)` podczas `vid_restart` zwracało istniejący cvar z `modified == false`. W rezultacie klient liczył `scr_vrect` dla pełnej rozdzielczości, a `R_RenderView` mnożyło `refdef` jeszcze raz przez `gl_hudscale->value`, wypychając obraz 3D i UI poza granice okna.
- Wprowadzono wspólną funkcję `R_UpdateHudScale()` w `ref_gl/gl_rmain.c` (z prototypem w `ref_gl/gl_local.h`), wywoływaną w `UpdateDrawableSize()`, `R_Init()` oraz przed ustawieniem `qglOrtho` w `R_BeginFrame()` (gdy `gl_hudscale->modified`). Zsynchronizowano `vid_scaled_width`/`vid_scaled_height` oraz `r_hudscale_x`/`r_hudscale_y` z zaokrąglonymi wymiarami `Vid_NewWindow`, eliminując ucinanie prawej krawędzi konsoli przy niepodzielnych skalach (np. `3x` w `1280x720`), oraz dodano bezpiecznik minimalnego wirtualnego płótna `320x240`. Dodano wywołanie `UpdateDrawableSize()` w `ToggleFullscreen()` (`Alt+Enter`) oraz `cl.force_refdef = true` w `VID_NewWindow()` (`linux/vid_so.c`).
- Dodano niezależne od `gl_hudscale` skalowanie celownika przez cvar `ch_scale` (`CVAR_ARCHIVE`, domyślnie `1`; `gl_hudscale` również otrzymało `CVAR_ARCHIVE`). Wyeksportowano w `ref_gl/gl_draw.c` (i `ref_gl/ref_gl.def`) opcjonalną funkcję `Draw_Crosshair`, ładowaną dynamicznie w `linux/vid_so.c` i `win32/vid_dll.c` (bez naruszania ABI `GetRefAPI`), rysującą celownik w natywnych pikselach ekranu na geometrycznym środku widoku 3D (z fallbackiem `DrawStretchPic`).
- W menu graficznym OpenGL (`linux/vid_menu.c`) zastąpiono nieaktywne `8-bit textures` kontrolkami `hud scale` (`1x`, `1.5x`, `2x`, `2.5x`, `3x`, `4x`) oraz `crosshair scale` (`1x`, `1.5x`, `2x`, `2.5x`, `3x`, `3.5x`, `4x`).
- Podczas testu uruchomieniowego ASan/UBSan wykryto i naprawiono przepełnienie bufora stosu w `Con_DrawConsole` (`client/console.c`: powiększono `version[24]` do `version[64]`) wynikające z błędu warunku przepełnienia w `Com_sprintf` (`game/q_shared.c`: `len == size` poprawiono na `len < 0 || len >= size` z bezpiecznym kopiowaniem `memcpy`).
- Zweryfikowano czystym buildem release (`make distclean && make -j8`), buildem `make sanitize` oraz uruchomieniem i zrzutami ekranu pod ASan/UBSan (kod wyjścia 0).
- Wykonano porządkowanie drzewa repozytorium: usunięto stare pliki projektowe VC6 (`.dsw`, `.dsp`, `.aps`, `qe3.ico`, `ref_gl/resource.h`), martwy 32-bitowy asembler `ref_soft`/DOS, wycofane backendy (`svgalib`, `rw_x11`, `gl_fxmesa`, `snd_linux`, `gl_sdl`, `al_linux`, `rw_ddraw`/`rw_dib`, `sv_null`, `m_rider.h`, `client_dll`) oraz nieaktualny cel `combine` z `Makefile`; przeniesiono oryginalne teksty R1Q2 do `docs/legacy/`, `EFFORT_REPORT.md` do `docs/` i uzupełniono `.gitignore`.
- Ujednolicono i wyodrębniono nazwy artefaktów wykonywalnych i bibliotek dla Linuxa (`r1q2`, `r1q2ded`, `r1q2_gamex86_64.so`, `ref_r1q2gl.so`) oraz Windowsa (`r1q2.exe`, `ref_r1q2gl.dll`, `r1q2_gamex86.dll`), eliminując kolizje nazw z innymi klientami Quake II w tym samym katalogu gry.

### 2026-11-17 — etap 10: unikalne nazwy binarek oraz migracja klienta Windows 32-bit na SDL2

- Zmieniono nazwy budowanych i ładowanych plików na unikalne z prefiksem `r1q2`, aby wklejenie do katalogu Quake II nie nadpisywało innych klientów:
  - Linux (`binaries/`): `binaries/client/r1q2`, `binaries/r1q2ded/r1q2ded`, `binaries/game/r1q2_gamex86_64.so`, `binaries/ref_gl/ref_r1q2gl.so`.
  - Windows (`binaries/win32/`, bez serwera dedykowanego): `r1q2.exe`, `ref_r1q2gl.dll`, `r1q2_gamex86.dll`.
- Przeniesiono klienta i renderer Windows 32-bit (`binaries/Makefile.win32`) na wspólny kod SDL2 używany na Linuksie:
  - Wideo, okno, obsługa wielu monitorów, myszy i klawiatury: `linux/gl_sdl2.c` + `linux/qgl_linux.c` w `ref_r1q2gl.dll` oraz `linux/vid_so.c` + `linux/vid_menu.c` w `r1q2.exe`.
  - Dźwięk: `linux/snd_sdl2.c` w `r1q2.exe` (16-bit stereo, 11/22/44/48 kHz, ring buffer + `SDL_LockAudioDevice`).
  - Dodano eksporty `RW_IN_*`, `KBD_*` oraz `RW_VID_GetNumDisplays` i `RW_VID_GetDisplayMode` w `ref_gl/ref_gl.def` i `linux/gl_sdl2.c`, dzięki czemu klient `r1q2.exe` pobiera listę monitorów i natywne tryby bezpośrednio z zainicjalizowanego podsystemu wideo SDL2 w `ref_r1q2gl.dll` przy linkowaniu statycznym `libSDL2-static.a`.
  - W `win32/sys_win.c` przekierowano `Sys_SendKeyEvents()` na `KBD_Update_fp()`, wyłączono przechwytywanie kolejki komunikatów okna przez `PeekMessage` w trybie klienta i zainicjalizowano `ActiveApp = true`.
  - W `client/cl_main.c` (`CL_RefreshInputs` i `CL_SendCommand_Synchronous`) odblokowano bezwarunkowe wywołanie `IN_Commands()` (wcześniej ograniczone przez `#if (defined JOYSTICK) || (defined LINUX)`), dzięki czemu kliknięcia przycisków myszy `Mouse 1`–`Mouse 5` z `RW_IN_Commands` działają również na Windowsie.
  - Zmieniono zachowanie przechwytywania myszy w `linux/gl_sdl2.c` (wspólne dla Linuxa i Windowsa): mysz pozostaje przechwycona w kontekście okna gry również po wejściu do menu (`Esc`) i konsoli (co umożliwia bindowanie `Mouse 1`–`Mouse 5` w `Customize Controls` oraz przygotowuje grunt pod własny kursor w menu), a `RW_IN_Activate(false)` wyłącza jedynie obracanie kamery 3D w tle; kursor systemowy jest uwalniany przy `Alt+Tab` (utrata fokusu okna) lub `Ctrl+G`.
  - Naprawiono pobieranie map i tekstur przez HTTP oraz fallback do tradycyjnego protokołu UDP Quake 2 (`server/sv_main.c`, `client/cl_http.c`, `client/cl_parse.c`, `client/cl_main.c`):
    - ustawiono domyślne `allow_download "1"` w kliencie (`"0"` tylko dla `DEDICATED_ONLY`),
    - zmieniono inicjalizację libcurl na `curl_global_init(CURL_GLOBAL_DEFAULT)` i dodano `remove()` przed `rename()` dla zgodności z `msvcrt` na Windowsie,
    - dodano `CL_ResetHTTPQueue()` przy `CL_ParseServerData()` (czyszczenie kolejki HTTP pomiędzy kolejnymi mapami w rotacji serwera przy zachowaniu `cls.downloadServer`),
    - dodano automatyczny fallback do pobierania UDP dla map `.bsp` (etap `ENV_CNT`) oraz tekstur `.wal` (etap `TEXTURE_CNT + 2`), gdy plik nie znajduje się na serwerze HTTP i zwraca `404 File Not Found`.
  - Usunięto z `client/menu.c` nieaktualne pod SDL2 opcje Win32 `directinput mouse` i `xp mouse acceleration fix`.
- Weryfikacja: czysty build Linux (`make distclean && make -j"$(nproc)"`) oraz cross-build Windows (`make -j"$(nproc)" win32` ze statycznym `SDL2 2.30.0`, `libpng`, `libjpeg-turbo`, `libcurl`, `zlib`) zakończone kodem 0; tablice importów PE32 zawierają wyłącznie biblioteki systemowe Windows.
- Testy na żywym serwerze `217.182.73.153:27910` potwierdziły zarówno pełne pobieranie HTTP (`maps/redyard.bsp`, `maps/q2dm2fast.bsp`, `textures/fryziu/fmet3_5_yellow.wal`), jak i automatyczne przełączenie na tradycyjne pobieranie UDP Quake 2 po `HTTP 404` oraz wznawianie pobierania `.tmp`. Użytkownik potwierdził działanie obrazu, dźwięku i myszy na Linuxie oraz Windows 11.

---
*Signed-off-by: **Cline** (`[CLINE-AI-SIG::R1Q2-SDL2-2026-11-17]`)*
