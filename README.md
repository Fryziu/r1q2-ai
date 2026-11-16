# R1Q2 Linux — SDL2 / OpenGL

Natywny port klienta **R1Q2 / Quake II** dla współczesnego Linuxa x86-64. Projekt zachowuje historyczny kod i kompatybilność zasobów, a jednocześnie zastępuje przestarzałe backendy SDL 1.2 i OSS przez SDL2.

**Stan dokumentacji:** 2026-11-13.

## Najważniejsze funkcje

- klient i serwer dedykowany dla Linux x86-64;
- singleplayer, multiplayer, save/load i przejścia między mapami;
- SDL2 video, klawiatura i mysz, OpenGL compatibility;
- SDL2 audio, 16-bit stereo, 11025/22050/44100/48000 Hz;
- fullscreen exclusive, fullscreen desktop i tryb okna;
- wybór monitora SDL, custom width/height i rozdzielczości do 3840×2160;
- pobieranie map, modeli, dźwięków i innych zasobów przez HTTP/HTTPS z fallbackiem UDP;
- bezpieczne ścieżki URL zgodne z Q2Pro — katalog moda nie jest zastępowany lokalną ścieżką `basedir`;
- poprawki LP64, pamięci, wyrównania, granic buforów i arytmetyki bez znaku;
- dynamiczna obsługa dużych obrazów oraz fallback `r_notexture` dla ekstremalnych zasobów;
- osobny build ASan/UBSan w `/tmp/opencode`.

Renderer software jest wyłączony. Aktywny renderer to `ref_gl.so` z `vid_ref gl`.

## Wymagania

- Linux Mint 22 / Ubuntu 24.04 lub podobny system x86-64;
- GCC i GNU Make;
- SDL2, OpenGL i GLU;
- libpng, libjpeg-turbo, zlib;
- libcurl (można go pominąć przy `USE_CURL=0`);
- oryginalne zasoby Quake II — nie są częścią repozytorium.

Instalacja zależności:

```sh
sudo apt install \
  gcc make pkg-config \
  libsdl2-dev libgl1-mesa-dev libglu1-mesa-dev \
  libpng-dev libjpeg-dev zlib1g-dev libcurl4-openssl-dev
```

## Kompilacja

Z katalogu głównego repozytorium:

```sh
make -j"$(nproc)"
```

Odpowiednik bezpośrednio z katalogu buildu:

```sh
make -C binaries -j"$(nproc)"
```

Pełna rekompilacja:

```sh
make distclean
make -j"$(nproc)"
```

Pomoc dotycząca targetów:

```sh
make help
```

### Build bez HTTP

```sh
make distclean
make -j"$(nproc)" USE_CURL=0
```

`USE_CURL=0` pomija `client/cl_http.c` i zależność od libcurl. Pozostałe komponenty pozostają bez zmian. Po zmianie `USE_CURL` wykonaj `distclean`, ponieważ zmiana zmiennej w linii poleceń nie zmienia timestampu Makefile’a. Edycje samych Makefile’ów są natomiast wykrywane przez zależności obiektów.

### Build ASan/UBSan

```sh
make -j"$(nproc)" sanitize
```

Target tworzy osobne drzewo `/tmp/opencode/r1q2-sanitize` i nie nadpisuje release’owych obiektów ani binarek. Zmienne opcjonalne:

```sh
make sanitize \
  SANITIZE_DIR=/tmp/opencode/r1q2-sanitize \
  SANITIZE_CC=gcc \
  SANITIZE_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
```

`SANITIZE_DIR` musi znajdować się pod `/tmp/opencode`.

## Artefakty

| Komponent | Plik |
|---|---|
| Klient | `binaries/client/quake2` |
| Serwer dedykowany | `binaries/r1q2ded/r1q2ded` |
| Biblioteka gry | `binaries/game/gamex86_64.so` |
| Renderer OpenGL | `binaries/ref_gl/ref_gl.so` |

Artefakty buildu, obiekty `.o` i `.depends` są ignorowane przez Git.

## Instalacja z własnymi zasobami gry

Wymagany jest katalog Quake II z `baseq2/pak0.pak` i pozostałymi oryginalnymi danymi. **Najpierw wykonaj kopię zapasową istniejących binarek.**

Przykład:

```sh
export R1Q2_DIR=/home/user/git/r1q2-tsmod
export Q2DIR="$HOME/.quake2"

cp "$R1Q2_DIR/binaries/client/quake2" "$Q2DIR/quake2"
cp "$R1Q2_DIR/binaries/ref_gl/ref_gl.so" "$Q2DIR/ref_gl.so"
cp "$R1Q2_DIR/binaries/game/gamex86_64.so" "$Q2DIR/baseq2/gamex86_64.so"
```

Opcjonalny serwer dedykowany:

```sh
cp "$R1Q2_DIR/binaries/r1q2ded/r1q2ded" "$Q2DIR/r1q2ded"
```

Układ wymagany przy uruchomieniu:

```text
quake2
ref_gl.so
baseq2/gamex86_64.so
baseq2/pak0.pak i pozostałe oryginalne zasoby
```

Uruchamiaj z katalogu gry, ponieważ renderer jest ładowany jako `./ref_gl.so`:

```sh
cd "$Q2DIR"
./quake2
```

## Podstawowe komendy

```text
map base1
map q2dm1
vid_restart
save 01
load 01
connect ADRES_SERWERA
```

`newgame` pozostaje konfigurowalnym punktem rozszerzenia dla konfiguracji i modów — nie jest zastępowane na sztywno komendą `map`.

## Wideo i wejście

### Rozdzielczość i monitory

- minimalna/domyślna rozdzielczość: **800×600**;
- `vid_fullscreen 0` — okno;
- `vid_fullscreen 1` — exclusive fullscreen;
- `vid_fullscreen 2` — fullscreen desktop, zalecany przy wielu monitorach;
- `vid_display N` — indeks monitora SDL;
- `gl_mode -1` — custom `vid_width` / `vid_height`;
- `gl_mode -2` — natywna rozdzielczość wybranego monitora;
- `vid_restart` — ponowna inicjalizacja renderera.

Okno jest ograniczane do usable bounds, a drawable uwzględnia High DPI. `gl_hudscale` umożliwia skalowanie HUD/menu/konsoli (w menu `Video`: `1x`–`4x` z krokami połówkowymi, z automatycznym zachowaniem minimum `320×240` płótna 2D), natomiast `ch_scale` steruje niezależnym skalowaniem celownika w fizycznych pikselach ekranu (`1x`–`4x` w menu `Video`).

### Mysz i klawiatura

- fullscreen podczas gry używa relative mouse;
- w oknie pełny capture wymaga `_windowed_mouse 1`;
- menu i konsola zwalniają grab i pokazują kursor;
- Alt+Tab, Alt+Enter, Ctrl+G, Mouse1–Mouse5 i kółko są obsługiwane;
- logika `IN_Frame` z `linux/vid_so.c` steruje capture na podstawie stanu gry i nie powinna być usuwana.

## Audio

Domyślnie `s_initsound 1`. Backend `linux/snd_sdl2.c` używa 16-bit stereo, ring buffer i callback SDL zsynchronizowany przez `SDL_LockAudioDevice`.

```text
s_khz 44100
snd_restart
```

Dostępne częstotliwości: 11025, 22050, 44100 i 48000 Hz. `s_initsound 0` wyłącza audio.

## Pobieranie HTTP/HTTPS

Domyślnie `cl_http_downloads 1`. Serwer może przekazać `dlserver` w komunikacie połączenia. Klient pobiera brakujące pliki przez libcurl, zapisuje je jako `.tmp` i dopiero po sukcesie zmienia nazwę docelową.

```text
cl_http_downloads 1
cl_http_filelists 1
cl_http_max_connections 4
```

Dodatkowo `cl_http_proxy` pozwala wskazać proxy.

### Ścieżki i fallback

- zdalny URL używa nazwy katalogu moda z `CL_HTTPGamedir`, zgodnie z Q2Pro;
- pusty katalog gry oznacza `baseq2`;
- lokalny zapis używa `FS_Gamedir()`;
- lokalna ścieżka `basedir` nigdy nie jest wysyłana do serwera;
- `cl_http_downloads 0` wyłącza HTTP i zachowuje pobieranie UDP;
- nieprawidłowy `dlserver`, np. `file://`, jest odrzucany i również pozostawia fallback UDP;
- transfery i przekierowania są ograniczone do HTTP/HTTPS;
- listy `.filelist` są ograniczone do 1 MiB;
- timeout połączenia wynosi 10 s, a wykrywanie zawieszenia 1 KiB/s przez 30 s.

Przykładowe ścieżki:

```text
baseq2.filelist
baseq2/maps/testhttp.filelist
testmod.filelist
testmod/maps/testhttp.filelist
```

## Duże i nieprawidłowe obrazy

`GL_Upload8` zachowuje mały bufor stosu dla typowych obrazów, a większe konwertuje przez dynamiczny bufor `size_t`. Obrazy takie jak `players/makuta/makuta.pcx` 640×480 są więc poprawnie skalowane zamiast kończyć połączenie błędem `GL_Upload8 ... too large`.

Nieprawidłowe wymiary i obrazy powyżej 4096×4096 pikseli używają `r_notexture` z ostrzeżeniem. Nie wyłącza to krytycznych błędów struktury mapy BSP.

## Testy

Podstawowy zakres manualny:

1. start do menu i dźwięk;
2. `map base1`, `map q2dm1`, ruch i blaster;
3. start easy/medium z menu;
4. save/load i przejścia `base1 -> base2 -> base3 -> base2`;
5. `vid_restart` i kilka rozdzielczości;
6. fullscreen desktop i wybór monitora;
7. wyjście i ponowny start;
8. serwer zewnętrzny z wymaganym pobraniem mapy;
9. serwer z `players/makuta/makuta.pcx` — brak błędu `GL_Upload8`.

Serwer dedykowany można sprawdzić bez zasobów:

```sh
/tmp/opencode/r1q2-sanitize/binaries/r1q2ded/r1q2ded +quit
```

Dla ASan/UBSan używano m.in.:

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  /tmp/opencode/r1q2-sanitize/binaries/r1q2ded/r1q2ded +quit
```

`detect_leaks=0` oznacza, że te testy nie są deklaracją pełnej analizy wycieków.


## Struktura repozytorium

- `client/` — klient, efekty, wejście, HUD i pobieranie HTTP;
- `server/` — logika serwera;
- `game/` — biblioteka gry;
- `qcommon/` — wspólne struktury, pliki, protokół i alokatory;
- `ref_gl/` — renderer OpenGL;
- `linux/` — SDL2 video/input/audio i funkcje systemowe;
- `binaries/` — Makefile’e i wygenerowane artefakty;
- `reference/` — opcjonalne źródła porównawcze, ignorowane przez Git i nieużywane w buildzie.

Dokumentacja:

- `README.md` — główna instrukcja użytkownika;
- `PORTING_LINUX.md` — szczegółowa historia zmian i testów;
- `AGENTS.md` — zasady pracy dla kolejnych agentów;
- `NEXT_SESSION_PROMPT.md` — krótki brief kolejnej sesji;
- `docs/EFFORT_REPORT.md` — historyczny raport zakresu prac;
- `docs/legacy/` — oryginalne dokumenty R1Q2 (`r1q2.txt`, `readme.txt`, `TODO.txt`), zachowane bez zmian.

## Znane ograniczenia i uwagi

- `couldn't exec r1gl.cfg` oznacza brak opcjonalnego pliku konfiguracyjnego i nie jest błędem krytycznym.
- `Function offsets ...` jest diagnostyką `ReadLevel()` na systemach innych niż Windows; obecne save/load działa.
- Joystick jest wyłączony. Unicode/IME, SDL_GameController i pełna obsługa urządzeń audio nie są zaimplementowane.
- Nie planowana jest migracja na Vulkan, OpenGL core ani shadery dla samej nowoczesności.
- Lokalna gra z absolutnym `basedir` wymaga dodatkowej poprawki `Sys_GetGameAPI`, ponieważ ścieżka `gamex86_64.so` może zostać powielona. Zalecany jest start z katalogu gry zgodnie z sekcją instalacji.
- Stare ostrzeżenia kompilatora są poprawiane selektywnie, nie masowo.

## Licencja i zasoby

Kod R1Q2/Quake II jest objęty licencją **GNU GPL w wersji 2 lub (według wyboru) dowolnej późniejszej**, zgodnie z nagłówkami źródeł. Oryginalne prawa należą do id Software, Inc. i autorów R1Q2. Niniejsze repozytorium zawiera modyfikacje dla współczesnego Linuxa.

Oryginalne zasoby gry, paki `.pak` i inne pliki danych nie są objęte tym buildem i muszą pochodzić z legalnej instalacji Quake II.

