# Building C Applications with Raven (Windows / MinGW-w64)

This guide walks you through building and running your own C applications using the **Raven** UI library on Windows with MinGW-w64.

---

## 1. Prerequisites and Toolchain

### Required Tools
Raven is built and consumed using the MinGW-w64 toolchain. Ensure you have the following installed:
- **MSYS2** (provides the MinGW-w64 environment)
- **GCC** (`mingw-w64-x86_64-gcc`)
- **CMake** (`mingw-w64-x86_64-cmake` >= 3.16)
- **Ninja** (`mingw-w64-x86_64-ninja`)
- **Git**

### Installation Commands
If you do not have MSYS2 or the tools installed, run from PowerShell:

```powershell
# Install MSYS2 via Windows Package Manager
winget install --id MSYS2.MSYS2 -e --accept-package-agreements --accept-source-agreements
```

Then in the **MSYS2 MINGW64** shell (or via PowerShell calling pacman):
```bash
pacman -S --needed --noconfirm git mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
```

> [!NOTE]
> **Windows 11 Smart App Control:**
> If you encounter `An Application Control policy has blocked this file` when running `gcc` or `ninja`, Windows 11 Smart App Control is blocking unsigned community binaries. To resolve this, open **Windows Security** -> **App & browser control** -> **Smart App Control settings** and set it to **Off** (or Evaluation).

### Setting Up Environment PATH
For convenience in PowerShell or Windows Command Prompt, add the MinGW64 binary directory to your `PATH`:

```powershell
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
```

---

## 2. Building and Installing the Raven Library

Before creating applications, the Raven library must be compiled and installed to generate `libraven.a` and the public C headers:

```powershell
# Navigate to the Raven root directory
cd C:\Users\khali\Code\Raven

# Configure the build tree with Ninja in Release mode
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build the library in parallel
cmake --build build --parallel

# Install to the prefix directory (creates include/, lib/, bin/)
cmake --install build --prefix install
```

After this step, your `install` folder contains:
- `install/include/Raven.h` (Main C header)
- `install/include/raven/*.h` (Subsystem headers)
- `install/lib/libraven.a` (Static C library)
- `install/lib/cmake/raven/` (CMake package definitions for `find_package(raven)`)

---

## 3. Raven Dependencies for Windows Applications

When linking statically with `libraven.a`, your application must link against the required Windows system APIs:

| Dependency | Purpose | Linker Flag |
| :--- | :--- | :--- |
| **libraven** | Raven UI library | `-lraven` |
| **UUID** | COM Interface GUIDs (UI Automation) | `-luuid` |
| **WinHTTP** | Web & network resources | `-lwinhttp` |
| **User32** | Windowing, input handling, message loop | `-luser32` |
| **GDI32** | Graphics device interface & raster fonts | `-lgdi32` |
| **Shell32** | Shell, tray icons, system dialogs | `-lshell32` |
| **ComDlg32** | Native file picker dialogs | `-lcomdlg32` |
| **Ole32** | COM & OLE subsystem | `-lole32` |

> [!TIP]
> If you use CMake (`find_package(raven)`), all these system libraries are imported and linked **automatically** via target `raven::raven`.

---

## 4. Creating Your Application

You can structure your Raven application in two ways:
1. **Programmatic C API** (pure C code creating windows, layout boxes, and controls).
2. **Declarative XML UI** (UI markup in `.xml`, logic bound in C).

### Approach A: Programmatic C API (Recommended for pure C)

Create a project directory `my_app` and add `main.c`:

```c
#include <Raven.h>
#include <stdio.h>

static Rid g_label;
static int g_count = 0;

static void on_button_click(Rid button, void* user_data) {
    (void)button;
    (void)user_data;
    g_count++;

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Clicked %d time%s", g_count, g_count == 1 ? "" : "s");
    Rlabel_set_text(g_label, buffer);
}

int main(void) {
    /* Initialize Raven with default rendering backend */
    if (!Rinit(R_BACKEND_DEFAULT)) {
        return 1;
    }

    /* Create the main window */
    Rid window = Rwindow_create("My Raven Application", 420, 240, R_WINDOW_DEFAULT);

    /* Vertical box container centered in the window */
    Rid vbox = Rwidget_create(R_WIDGET_VBOX);
    Rpanel_set_spacing(vbox, 14.0f);
    Rslot_set_halign(vbox, R_HALIGN_CENTER);
    Rslot_set_valign(vbox, R_VALIGN_CENTER);
    Rwindow_set_content(window, vbox);

    /* Add title label */
    Rid title = Rwidget_add_child(vbox, Rlabel_create("Welcome to Raven!"));
    Rlabel_set_text_size(title, 20.0f);
    Rslot_set_halign(title, R_HALIGN_CENTER);

    /* Add status label */
    g_label = Rwidget_add_child(vbox, Rlabel_create("Click the button below"));
    Rslot_set_halign(g_label, R_HALIGN_CENTER);

    /* Add interactive button */
    Rid button = Rwidget_add_child(vbox, Rbutton_create("Click Me"));
    Rslot_set_halign(button, R_HALIGN_CENTER);
    Rbutton_on_clicked(button, on_button_click, NULL);

    /* Main event / frame loop */
    while (Rdraw_frame()) {
        /* Runs until the user closes the window or calls Rquit() */
    }

    /* Clean up Raven resources */
    Rshutdown();
    return 0;
}
```

---

### Approach B: Declarative XML UI + C Logic

Create `main.xml`:
```xml
<Window Title="XML Raven App" Width="400" Height="220">
    <VBox Spacing="12" HAlign="Center" VAlign="Center">
        <Label Text="Raven XML Interface" TextSize="20" HAlign="Center"/>
        <Label Name="status" Text="Waiting for click..." HAlign="Center"/>
        <Button Text="Press Me" OnClicked="btn.clicked" HAlign="Center"/>
    </VBox>
</Window>
```

Create `main.c`:
```c
#include <Raven.h>
#include <stdio.h>

static Rid g_window = R_INVALID_ID;
static int g_clicks = 0;

static void on_click(Rid sender, const char* action, void* user_data) {
    (void)sender; (void)action; (void)user_data;
    g_clicks++;

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Pressed %d times", g_clicks);
    Rlabel_set_text(Rwidget_find(g_window, "status"), buffer);
}

int main(void) {
    if (!Rinit(R_BACKEND_DEFAULT)) return 1;

    /* Bind action name defined in XML */
    Raction_bind("btn.clicked", on_click, NULL);

    /* Load window layout from XML file */
    g_window = Rwindow_create_from_xml_file("main.xml");
    if (g_window == R_INVALID_ID) {
        Rshutdown();
        return 1;
    }

    while (Rdraw_frame()) {}

    Rshutdown();
    return 0;
}
```

---

## 5. How to Build Your Application

### Method 1: Using CMake (Recommended)

1. Create a `CMakeLists.txt` in your project folder:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app C)

# Find the installed Raven package
find_package(raven REQUIRED)

# For GUI applications without an extra CMD console window, use WIN32:
add_executable(my_app WIN32 main.c)

# Link against Raven (automatically pulls in Windows system dependencies)
target_link_libraries(my_app PRIVATE raven::raven uuid)
```

2. Configure and build:

```powershell
# Set MinGW in PATH
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"

# Configure pointing to Raven install prefix
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Users/khali/Code/Raven/install"

# Compile
cmake --build build

# Run
./build/my_app.exe

# Run unit, integration, and startup tests (when BUILD_TESTING is enabled)
ctest --test-dir build --output-on-failure
```

The C-AIR project includes the Raven source checkout in `Raven/`. Running
`build.bat` configures, builds, and installs Raven into `Raven/install/`, then
builds C-AIR against that library and runs the C-AIR tests. Raven's optional
examples, tests, tools, and web component are disabled in this workflow.

Each C-AIR build gets a timestamped executable name in `build/bin`. Close any
running C-AIR application before running the script because it removes the
C-AIR `build/` folder, and Windows will not delete a running executable.
Raven's build and install folders are kept for incremental builds. A plain
CMake build uses `build/bin/cair.exe` unless you set `CAIR_OUTPUT_NAME` when
configuring.

---

### Method 2: Direct Compilation with GCC

If you prefer building without CMake, use GCC directly:

```powershell
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"

# Compile and link
gcc main.c `
  -I"C:/Users/khali/Code/Raven/install/include" `
  -L"C:/Users/khali/Code/Raven/install/lib" `
  -lraven -luuid -lwinhttp -luser32 -lgdi32 -lshell32 -lcomdlg32 -lole32 `
  -mwindows `
  -o my_app.exe

# Run your executable
./my_app.exe
```

> [!TIP]
> - `-mwindows`: Specifies a GUI application (hides the background command prompt console).
> - Remove `-mwindows` during development if you want `printf()` output in the console.
> - Link `-luuid` for Raven's Windows UI Automation interface identifiers.

---

### Method 3: Ready-to-Use `build.bat` Script

You can drop a simple `build.bat` in your project directory:

```bat
@echo off
setlocal

set "PATH=C:\msys64\mingw64\bin;%PATH%"
set "RAVEN_DIR=C:\Users\khali\Code\Raven\install"

if not exist build mkdir build

cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="%RAVEN_DIR%"
if errorlevel 1 exit /b 1

cmake --build build
if errorlevel 1 exit /b 1

echo Build succeeded! Running executable...
build\my_app.exe
```

---

## 6. Project Directory Layout Example

A typical standalone Raven C project structure looks like this:

```
my_app/
├── CMakeLists.txt         # CMake build configuration
├── build.bat              # Turnkey build script
├── main.c                 # C source code
├── main.xml               # (Optional) Declarative UI XML
└── build/                 # Generated build output
    └── my_app.exe         # Your compiled application
```

---

## 7. Troubleshooting

- **Undefined reference to `IID_ISelectionProvider` or COM symbols:**
  - Make sure `-luuid` and `-lole32` are included in your linker arguments when compiling directly with GCC.
- **Undefined reference to `Rinit` or `Rwindow_create`:**
  - Ensure `-L<path-to-install>/lib` precedes `-lraven` and that `libraven.a` exists in that directory.
- **Console window pops up with GUI window:**
  - Add `WIN32` to `add_executable(my_app WIN32 main.c)` in CMake, or add `-mwindows` to your GCC command line.
- **File not found (`Raven.h`):**
  - Check that `-I<path-to-install>/include` is passed to the compiler.
