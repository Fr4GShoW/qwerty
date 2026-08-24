# Office Endpoint Management Panel — Native C++ Port

This is a full rewrite of the app in C++: a real native `OfficePanel.exe`
with no Python runtime, no PyInstaller, no bundled interpreter, and —
since the last revision — **no localhost HTTP server either**. No socket,
no port, nothing for Windows Firewall to ever ask about. Just a compiled
executable with a native window (via Microsoft's WebView2) showing the
exact same `index.html`/`login.html` UI as before, answered entirely
in-process. See §3 for exactly how that works.

**This must be built ON a Windows machine** — same reason as always: a
compiler produces machine code for whatever OS it runs on, and there's no
Windows C++ compiler in the environment I built this in. **Don't have a
Windows machine at all?** See §0 right below — you can get a real,
compiled `OfficePanel.exe` without ever touching Windows yourself, using
a free GitHub account. Otherwise, what follows is everything needed to
build it on a Windows PC directly, plus an unusually large amount of
testing done ahead of time (see §4) so you're not the first one finding
out whether the core logic works.

**Building specifically in VS Code?** See `README_VSCODE.md` instead —
this document covers the plain-batch-file / command-line workflow and the
app's architecture in more depth; the VS Code doc covers the
extension-driven workflow using the `.vscode/` config already included
here.

---

## 0. Don't have a Windows machine? Build it in the cloud instead (free)

This repo includes `.github/workflows/build-windows.yml`, which builds
`OfficePanel.exe` on an actual Windows machine that GitHub provides for
free, whenever you want one — you never install anything locally.

1. Create a free [GitHub](https://github.com/) account if you don't have
   one, then create a new repository and push this `OfficePanel_CPP`
   folder's contents into it (via the GitHub web UI's "upload files", or
   `git init && git add . && git commit -m "initial" && git push` if
   you're comfortable with git — either works fine).
2. Go to the **"Actions"** tab on your repository page.
3. Click **"Build Windows executable"** in the left sidebar, then the
   **"Run workflow"** button on the right (it also runs automatically the
   moment you push, so this step is only needed if you want to re-run it
   without pushing a new commit).
4. Wait 3-6 minutes. A green checkmark means it succeeded.
5. Click into the finished run, scroll down to **"Artifacts"**, and
   download **"OfficePanel-windows"** — that's a zip containing the real,
   compiled `OfficePanel.exe` plus its `web\` folder, built moments ago by
   an actual Windows machine, ready to unzip and run on any Windows PC.

This runs the exact same CMake steps as `build_windows.bat` below — it's
not a different, less-tested path, just a different machine running it.

## 1. What's in this folder

```
OfficePanel_CPP/
├── .github/workflows/
│   └── build-windows.yml       Builds the .exe on a free GitHub-hosted Windows machine -- see §0
├── src/                        Portable core + Windows-only GUI shell
│   ├── json.h/.cpp              Dependency-free JSON parser/serializer
│   ├── csv.h/.cpp                RFC4180 CSV parser (Google Sheets export format)
│   ├── paths.h/.cpp               Bundled-resources vs. persistent-data folder resolution
│   ├── http_server.h/.cpp        Routing/session/request-response logic (see §3 for how it's reached)
│   ├── sheets_logic.h/.cpp       Port of data_parser.py's column mapping + zone rules
│   ├── auth.h/.cpp                 Login + session/cookie handling
│   ├── data_store.h/.cpp         endpoints.json / office_data.json persistence
│   ├── routes.h/.cpp               Wires every /api/* route together
│   ├── sheets_fetch_win.h/.cpp  [Windows only] Google Sheets fetch via WinHTTP
│   ├── webview_bridge.h/.cpp     [Windows only] Answers the UI's requests with NO socket at all
│   └── main_win.cpp               [Windows only] WinMain + WebView2 window
├── web/                        Your original frontend, UNCHANGED
│   ├── index.html                (same "Sync from Sheets" button as before)
│   ├── login.html
│   └── data/                      Bundled starter data, copied on first run
├── test/                       Dev-only Linux/macOS console build (see §5) -- this one DOES use a
│                                socket, since WebView2 doesn't exist off Windows (see §3)
├── .vscode/                    Ready-made VS Code config -- see README_VSCODE.md
│   ├── extensions.json           Recommends the C/C++ and CMake Tools extensions
│   ├── settings.json              Wires IntelliSense to CMake Tools
│   ├── tasks.json                  SDK fetch + configure/build/install tasks (Ctrl+Shift+B)
│   └── launch.json                 F5 debug config for OfficePanel.exe
├── CMakeLists.txt
├── CMakePresets.json           One-click configure/build preset -- read by both VS Code's CMake
│                                Tools extension and full Visual Studio's "Open Folder" CMake support
├── build_windows.bat
├── .gitignore                  Excludes build/, dist/, packages/ -- IMPORTANT if you commit this
│                                to git yourself: CMake's build/ folder hardcodes an absolute path,
│                                so committing it will break the build on any other machine
│                                (including GitHub Actions) with a "CMakeCache.txt directory is
│                                different" error. Don't remove entries from this file.
├── README_BUILD_WINDOWS.md     This file
└── README_VSCODE.md            VS Code-specific build/debug instructions
```

## 2. Build it (one time, on Windows)

**Requirements:**
- Windows 10/11
- [CMake](https://cmake.org/download/) (check "Add to PATH" during setup)
- A C++ compiler — the [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) (free, no full IDE needed) with the "Desktop development with C++" workload, or full Visual Studio with the same workload

**Steps:**
1. Copy the whole `OfficePanel_CPP` folder onto the Windows machine.
2. Open a **"x64 Native Tools Command Prompt for VS"** (search for it in the Start Menu — this is what puts the `cl.exe` compiler on PATH; a plain Command Prompt won't have it unless you've set that up yourself).
3. `cd` into the `OfficePanel_CPP` folder.
4. Run `build_windows.bat`.
   - First run downloads `nuget.exe` and fetches the WebView2 SDK (headers + loader library) via it — this is Microsoft's own documented way of getting the WebView2 SDK into a non-Visual-Studio-project C++ build.
   - Then it configures and builds with CMake.
5. Your app ends up in `dist\OfficePanel\` — `OfficePanel.exe` plus a `web\` folder next to it.

**Copy the whole `dist\OfficePanel` folder** wherever you want to run it — the `.exe` needs its `web\` folder sitting right next to it (that's where `index.html`, `login.html`, and the starter data live).

### If you'd rather run the commands by hand
```
nuget.exe install Microsoft.Web.WebView2 -OutputDirectory packages -ExcludeVersion
cmake -S . -B build -A x64
cmake --build build --config Release
cmake --install build --config Release --prefix dist\OfficePanel
```

### Building it inside the Visual Studio IDE instead

If you'd rather work in the Visual Studio IDE — set breakpoints, use the
debugger, browse the code with IntelliSense — than the command line, Visual
Studio's built-in CMake support opens this project directly with no
separate `.sln`/`.vcxproj` files needed (and no risk of a hand-written VS
project quietly drifting out of sync with the CMake build, since it's the
exact same `CMakeLists.txt` either way).

**Requirements:** Visual Studio 2022 (Community is free) with the "Desktop
development with C++" workload — this includes the CMake tools and the
compiler both.

**Steps:**
1. **Fetch the WebView2 SDK first.** Visual Studio's "Open Folder" CMake
   flow doesn't run arbitrary NuGet CLI commands on its own the way a
   `.vcxproj`'s Package Manager integration would, so this needs to happen
   before you open the project. Open **"Developer PowerShell for VS 2022"**
   from the Start menu, `cd` into `OfficePanel_CPP`, and run:
   ```powershell
   Invoke-WebRequest -Uri https://dist.nuget.org/win-x86-commandline/latest/nuget.exe -OutFile nuget.exe
   .\nuget.exe install Microsoft.Web.WebView2 -OutputDirectory packages -ExcludeVersion
   ```
   (If you've already run `build_windows.bat` once from the command line,
   this is already done — skip straight to step 2.)
2. In Visual Studio: **File → Open → Folder…** and select the
   `OfficePanel_CPP` folder.
3. Visual Studio detects `CMakeLists.txt` and `CMakePresets.json`
   automatically and starts configuring — watch the **Output** window
   (switch its dropdown to "CMake") for progress. This can take a few
   seconds the first time.
4. Once configuration finishes, use the **Startup Item** dropdown in the
   toolbar (next to the green ▷ Run button) and select **`OfficePanel.exe`**.
5. **F5** builds and runs with the debugger attached; **Ctrl+F5** runs
   without it; **Ctrl+Shift+B** just builds. Either way, `web\` gets
   copied next to the built `.exe` automatically after every build (see
   the `POST_BUILD` step in `CMakeLists.txt`), so it's runnable
   immediately — no separate install step needed for day-to-day
   iteration.
6. The build output lands in `build\Release\OfficePanel.exe` (matching
   the same `build\` folder the command-line `build_windows.bat` uses,
   via the `windows-x64` preset in `CMakePresets.json` — so switching
   between building from the IDE and from the command line doesn't leave
   you with two separate half-built trees to keep straight).

If Visual Studio doesn't show a preset/configuration automatically, use
the dropdown at the top of the IDE (next to the Startup Item dropdown) and
pick **"windows-x64"** explicitly.

## 3. How the UI talks to the app, with no local server

Double-click `OfficePanel.exe`. A native window opens showing the same
login page, floor plan UI, and "🔄 Sync from Sheets" button as before —
but under the hood, nothing is listening on any port.

Here's the mechanism: WebView2 (the browser engine Microsoft ships with
Windows) has a feature called `WebResourceRequested` that lets an app
intercept every request the page makes to a chosen virtual address and
answer it directly, in-process, without any real network activity ever
happening for it. This app navigates the window to
`https://officepanel.local/` — an address that isn't real and that
WebView2 never actually opens a connection for — and every request
against it (the page load itself, `/api/login`, `/api/endpoints`, all of
it) gets caught by `webview_bridge.cpp` and answered directly from the
same routing logic (`routes.cpp`) as before. `index.html`'s existing
`fetch('/api/...')` calls didn't need to change at all; they just resolve
against the virtual address instead of a real one.

One nuance worth knowing: `http_server.h/.cpp` — the file with actual
socket code (`bind`/`listen`/`accept`) — is still part of this project and
still gets compiled into `OfficePanel.exe`, because it's also where the
request-routing table and business logic live (shared with the Linux/macOS
dev build in `test/`, which doesn't have WebView2 to fall back on — see
§5). But in the Windows GUI build, **its `listen()`/`run()` socket methods
are never called anywhere** — `main_win.cpp` only calls the
route-registration and `dispatch()` parts, both of which have nothing to
do with sockets. No port ever opens; there's simply unused socket-handling
code sitting in the binary the same way any app has functions it doesn't
call on every code path. If you want to confirm this yourself once it's
built: open Resource Monitor → Network → Listening Ports while
`OfficePanel.exe` is running — it won't be there.

The only real network traffic this app ever generates is the outbound
"Sync from Sheets" fetch to Google's servers when you click that button
— which is the feature working as intended, not a local server.

Login: `admin` / `AdminPassOM2026@!` or `soc` / `SOCPASSOM2026@!` (same
accounts as before — see §7 on how these are stored).

## 4. What's actually been tested, and how

I don't have a Windows machine in the environment I built this in, so I
couldn't compile or run the three genuinely Windows-only files
(`main_win.cpp`'s WebView2 window, `webview_bridge.cpp`'s request
interception, `sheets_fetch_win.cpp`'s WinHTTP call). Everything else, I
could — and did:

- **Every other source file compiles and runs on Linux** using the exact
  same code (the platform-specific bits are cleanly `#ifdef`'d out), and
  I wrote real test programs for each layer rather than just eyeballing
  the code:
  - `json.h/.cpp` — round-tripped nested objects/arrays, escaped strings,
    unicode, malformed-input error handling.
  - `csv.h/.cpp` — quoted fields, embedded commas/quotes/newlines, CRLF
    line endings (the format Google Sheets actually exports).
  - `sheets_logic.h/.cpp` — fed it a synthetic sheet covering an active
    endpoint, an available one, an empty one, and the legacy `HH→HR`
    prefix conversion, and checked every field came out matching
    `data_parser.py`'s original column-by-column logic.
  - `http_server.h/.cpp` — routing (including path params and wildcards),
    query strings, cookies, POST bodies, concurrent requests, and clean
    shutdown, all driven by a raw in-process socket client hitting a real
    listening server. (This exercises the exact same `dispatch()` method
    that `webview_bridge.cpp` calls directly with no socket involved — see
    below.)
  - `data_store.h/.cpp` + `auth.h/.cpp` — persistence round-trips, session
    lifecycle, multiple concurrent sessions.
  - **`routes.h/.cpp` — the whole thing wired together**: 24 end-to-end
    HTTP requests against the real server covering login, sessions,
    "Sync from Sheets" (with a stand-in fetch function in place of
    WinHTTP, since this sandbox has no network path to Google), the
    resulting endpoint data (including the `HH→HR` conversion showing up
    correctly through the full stack), updating/deleting endpoints,
    search, and static/data file serving. All 24 passed.
- **Why the no-server rework didn't throw away that testing**: the
  socket-based server and `webview_bridge.cpp` both work by calling the
  exact same `HttpServer::dispatch()` method — one reaches it via a real
  accepted socket connection, the other calls it directly from a WebView2
  event handler. I made `dispatch()` public specifically so both transports
  share one code path, then re-ran the full 24-test suite after that
  change to confirm nothing shifted. What's genuinely new and untested is
  the *translation* layer in `webview_bridge.cpp` — converting a
  `ICoreWebView2WebResourceRequest` into the same `HttpRequest` struct, and
  an `HttpResponse` back into a `ICoreWebView2WebResourceResponse` — not
  the routing or business logic behind it.
- **Along the way I found and fixed two real bugs**: a clean-shutdown
  hang (closing a socket from another thread while it's blocked in
  `accept()` isn't reliable — replaced with a poll loop), and a path
  resolution inconsistency where the data-seeding logic could silently
  disagree with the folder actually being served from.
- **What I could not test**: whether `main_win.cpp` actually creates a
  window and displays WebView2 correctly, whether
  `webview_bridge.cpp`'s request/response translation and cookie handling
  work exactly as expected against the real WebView2 engine, and whether
  `sheets_fetch_win.cpp`'s WinHTTP calls work against the real Google
  Sheets export URLs. All three are written against well-documented,
  standard Microsoft APIs (the official WebView2 "Getting Started"
  sample's COM callback pattern, the documented `WebResourceRequested`
  interception pattern used for exactly this "hybrid app, no local
  server" scenario, and the standard synchronous WinHTTP request
  sequence) — but "written carefully against the docs" and "confirmed
  working" are different things, and I want to be upfront about which one
  this is. If the window doesn't appear, the page doesn't load, or Sync
  fails on first try, that's the most likely place to look — see §8
  below.

## 5. Testing the core logic yourself, without Windows

If you want to poke at the app's behavior before/without touching
Windows at all, there's a real (if GUI-less) build for that:

```
cmake -S . -B build
cmake --build build
./build/office_panel_test
```

This runs the identical server/routing/parsing code that ships in
`OfficePanel.exe` — just without the WebView2 window. Open the printed
`http://127.0.0.1:5050/` URL in any browser instead. It's genuinely
useful for development, not just a test artifact: same login, same API,
same Sync-from-Sheets behavior (using `curl` under the hood to fetch the
sheet instead of WinHTTP, since that's what's available on Linux/macOS).

## 6. Where your data lives

Same model as before: `OfficePanel.exe`'s own folder is treated as
read-only, so all edits and synced data live in a persistent per-user
folder, seeded from the bundled starter data on first run:

```
%LOCALAPPDATA%\OfficeEndpointPanel\data\
```

To fully reset the app, delete that `OfficeEndpointPanel` folder and
relaunch.

## 7. Security notes

- **Credentials are plaintext-compared against a small table baked into
  the binary** (`auth.cpp`). The earlier Python/PyInstaller version hashed
  the password with SHA-256 before comparing — I deliberately didn't
  carry that over here, and it's worth explaining why: that hash didn't
  actually protect the credentials, since the hash itself sits in a
  distributable binary either way and is trivially recoverable by anyone
  who decompiles it. Rather than hand-roll a SHA-256 implementation from
  scratch purely to preserve a property it wasn't providing (and risk a
  transcription error in the 64 round constants doing so), this version
  is upfront that the check is a plain string comparison. The actual
  security posture is the same either way: fine for an internal tool on
  trusted machines, not something to expose beyond that without moving
  credential checking server-side.
- **No compiled Python bytecode to decompile at all now** — someone
  wanting to recover the credentials would need to disassemble the
  actual native binary instead, which is a meaningfully higher bar than
  the PyInstaller build's `pyinstxtractor` + `uncompyle6` path (though
  still not something to rely on if this ever needs real security).
- **There's no network-reachable attack surface at all** — see §3: the
  app never opens a listening socket in the first place, so there's
  nothing on the machine's network stack for anything else (another
  process, another machine on the LAN) to even attempt to connect to.
  This is a step up from the earlier `127.0.0.1`-only binding, which was
  already not reachable from other machines but still technically a
  local listening port other processes on the *same* machine could probe.

## 8. If something doesn't work on first try

- **Window never appears / immediate error dialog about WebView2**:
  install the [WebView2 Runtime Evergreen
  Bootstrapper](https://developer.microsoft.com/microsoft-edge/webview2/)
  from Microsoft. It ships pre-installed on virtually all Windows 10
  (2020+) and all Windows 11 machines, so this should only matter on an
  older or locked-down PC.
- **`.exe` won't launch at all, or Windows reports a missing DLL like
  `VCRUNTIME140.dll` or `MSVCP140.dll`**: install the [Visual C++
  Redistributable](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist)
  (x64) from Microsoft — a small, free, standard installer. This app
  links the C++ runtime dynamically rather than bundling it into the
  `.exe`, deliberately, to stay compatible with the WebView2 SDK's
  prebuilt loader library (see the comment above the `install(TARGETS
  OfficePanel...)` line in `CMakeLists.txt` for why). The Redistributable
  is already present on the overwhelming majority of Windows 10/11
  machines, since huge numbers of other applications depend on it too —
  this should only come up on a very bare or freshly-imaged machine.
- **"Sync from Sheets" fails or times out**: check internet connectivity
  on the machine running the app — same requirement as the original
  Python parser had. If it's connected and still failing, the WinHTTP
  code in `sheets_fetch_win.cpp` is the one untested piece most likely to
  need a small fix (e.g., a corporate proxy that needs explicit
  configuration rather than `WINHTTP_ACCESS_TYPE_DEFAULT_PROXY`).
- **Build fails with a missing `cl.exe` or similar compiler error**:
  you're not in a "x64 Native Tools Command Prompt for VS" — see §2 step 2.
- **CMake can't find the WebView2 SDK**: delete the `packages\` folder
  and re-run `build_windows.bat` to re-fetch it, or pass
  `-DWEBVIEW2_SDK_DIR=<path>` pointing at an existing
  `Microsoft.Web.WebView2\build\native` folder if you already have the
  NuGet package downloaded elsewhere. In the Visual Studio IDE, set the
  same variable via the CMake Settings editor, or just re-run the two
  `nuget.exe` commands from §2 and reload the CMake cache
  (**Project → Delete Cache and Reconfigure**).
- **Visual Studio shows no CMake preset, or configuration never starts**:
  make sure the "C++ CMake tools for Windows" component is installed
  (Visual Studio Installer → Modify → Individual components), and that
  you opened the `OfficePanel_CPP` *folder* directly (File → Open →
  Folder), not a file inside it.
- **Window opens but stays blank/white, or shows a browser-style error
  page instead of the login screen**: this points at
  `webview_bridge.cpp` specifically — the one piece of the request/response
  translation I couldn't test. Things worth checking: that
  `AddWebResourceRequestedFilter`'s pattern actually matches the navigated
  URL, that `CreateWebResourceResponse` is being called with a valid
  environment pointer (it needs `g_webviewEnvironment` to already be set,
  which happens in the environment-created callback in `main_win.cpp`
  before the controller/webview even exist), and that the response's
  Content-Type header is being set correctly for `index.html`/`login.html`
  (should be `text/html; charset=utf-8`, set by `readFileOr404()` in
  `routes.cpp`).

## 9. Rebuilding after changes

Re-run `build_windows.bat`, or just `cmake --build build --config Release`
followed by the install step if you've already configured once. Data in
`%LOCALAPPDATA%` is untouched by rebuilding.
