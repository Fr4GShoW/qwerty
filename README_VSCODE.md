# Building in Visual Studio Code

This project includes ready-made VS Code configuration (`.vscode/` +
`CMakePresets.json`), so opening the folder gets you a working build setup
without hand-writing any config yourself. This doc covers the VS Code
workflow specifically; `README_BUILD_WINDOWS.md` covers the plain-batch-file
workflow and has more detail on the app itself, architecture, and
troubleshooting — start there if something below doesn't make sense.

**This still has to run on Windows** — VS Code itself is cross-platform,
but the compiler (MSVC) and the WebView2 SDK this app needs are
Windows-only, same constraint as always.

## 1. One-time setup

Install:
- **[VS Code](https://code.visualstudio.com/)**
- **[Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)** (free — you don't need the full Visual Studio IDE, just the "Build Tools" installer) with the **"Desktop development with C++"** workload checked. This is what actually provides the `cl.exe` compiler VS Code will call.
- **[CMake](https://cmake.org/download/)** (check "Add to PATH" during setup) — the Build Tools installer above can also install this for you if you tick "C++ CMake tools for Windows" in its component list, in which case you can skip this step.

Open the `OfficePanel_CPP` folder in VS Code (`File > Open Folder…`). It'll
prompt you to install the recommended extensions — accept that, or install
manually:
- **C/C++** (`ms-vscode.cpptools`)
- **CMake Tools** (`ms-vscode.cmake-tools`)

The first time CMake Tools asks you to **select a kit**, choose the one
that says something like `Visual Studio Build Tools 2022 Release - x86_amd64`
(or whichever VS version you installed) — this is what points CMake Tools
at the right compiler.

## 2. Fetch the WebView2 SDK (one-time, before the first build)

`CMakeLists.txt` needs the WebView2 SDK's headers to exist before it'll
configure successfully — it isn't fetched automatically by CMake itself
(see `README_BUILD_WINDOWS.md` §2 for why). Run this once:

- Press **`Ctrl+Shift+P`** → type **"Tasks: Run Task"** → choose
  **"1. Fetch WebView2 SDK (one-time)"**.

That downloads `nuget.exe` if you don't have it, then fetches the SDK into
a `packages\` folder next to this README. You only need to do this once
per copy of the project (it's skipped automatically on future runs if
already present).

## 3. Build it

You have two equally valid ways to do this — pick whichever you prefer:

### Option A: CMake Tools extension (recommended)
1. `Ctrl+Shift+P` → **"CMake: Select Configure Preset"** → choose **"Windows x64 (Release)"**.
2. `Ctrl+Shift+P` → **"CMake: Configure"**.
3. `Ctrl+Shift+P` → **"CMake: Build"** (or click **Build** in the status bar at the bottom).
4. `Ctrl+Shift+P` → **"CMake: Install"** to copy everything into `dist\OfficePanel\`.

### Option B: Tasks (no CMake Tools UI, just tasks + terminal)
Press **`Ctrl+Shift+B`** — this runs the **"Full build"** task, which
chains fetch → configure → build → install automatically. Individual
steps are also available separately via **"Tasks: Run Task"** if you want
to run just one (e.g. rebuilding after a code change without re-fetching
the SDK or re-running CMake configure).

Either way, you end up with:
```
dist\OfficePanel\OfficePanel.exe
dist\OfficePanel\web\...
```

## 4. Run / Debug it

- **Just run it**: double-click `dist\OfficePanel\OfficePanel.exe` in File Explorer, or `Ctrl+`` (backtick) to open VS Code's terminal and run `.\dist\OfficePanel\OfficePanel.exe`.
- **Debug it with breakpoints**: open the **Run and Debug** panel (`Ctrl+Shift+D`) and choose **"Debug OfficePanel.exe"**, then press **F5**. This automatically runs the full build task first (via `preLaunchTask`), then launches the app under the debugger — set breakpoints in any `.cpp` file beforehand and they'll be hit normally.

## 5. IntelliSense (autocomplete, go-to-definition, etc.)

Once you've run **CMake: Configure** at least once (via either option
above), IntelliSense should already work correctly — `.vscode/settings.json`
tells the C/C++ extension to get its include paths and compiler flags
directly from CMake Tools, rather than needing a hand-maintained
`c_cpp_properties.json`. If IntelliSense looks wrong (red squiggles on
things that should compile fine), run **"C/C++: Reset IntelliSense
Database"** from the command palette.

## 6. Rebuilding after you change code

Just **`Ctrl+Shift+B`** again (Option B), or **CMake: Build** again
(Option A) — no need to re-run the SDK fetch step, and CMake Tools/the
tasks will only recompile what changed.
