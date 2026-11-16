# RetroTerm for Classic Macintosh

RetroTerm is a small 68k Macintosh serial terminal for testing the Modem and
Printer ports used by ClassicMacStudio clients. It defaults to MODEM at 9600
baud, 8-N-1, with Local Echo on. Select a port and baud rate, click **Open**,
click the terminal text area, and type; accepted characters are sent
immediately. Received printable ASCII, CR/LF, tab, and backspace are shown in
the terminal text view. The view is rendered with QuickDraw, while keyboard
input is handled by the app. The popup menus and Local Echo checkbox use the
standard ClassicMacStudio project widget definitions.
Long terminal lines wrap by measured font width, and the window close box
and **Quit** button terminate the application after closing any open serial
drivers.

The port and baud controls are disabled while the port is open. Close the
connection before changing them. The application does not implement a file
transfer protocol, software/hardware flow control, ANSI/VT terminal escapes,
or a transmit line buffer.

For diagnostics, RetroTerm appends timestamped startup stages, resource and
heap information, UI milestones, and serial-operation results to
`RetroTerm.log` beside the application. If the Process Manager cannot provide
the app location, it falls back to the current directory. The log rolls over
at 32 KiB and does not include transmitted or received text. Copy the file
from the HFS volume after a run to inspect it.

Build with the repository's Retro68 toolchain from PowerShell:

```powershell
wsl.exe -d Ubuntu -- bash <(sed 's/\r$//' /mnt/d/Repository/retro/ClassicMacStudio/scripts/wsl/build-hello-mac.sh) /mnt/d/Repository/retro/ClassicMacStudio/feasibility/samples/retroterm /mnt/d/Repository/retro/ClassicMacStudio/.local-tools/mac-image-build/retroterm RetroTerm
```

The build produces `RetroTerm.dsk`, an HFS application volume containing the
application. Open [RetroTerm.cmideproj](RetroTerm.cmideproj) in
ClassicMacStudio to inspect or edit the loadable project configuration. Its
`src/main.c` is also used by the standalone Retro68 CMake build. The
cross-platform requirements and target-independent behavior are documented in
[RetroTerm specification](../../../docs/retroterm-specification.md).
For a step-by-step beginner guide to the project, build, and hard-disk image,
see the [RetroTerm tutorial](../../../tutorial/guides/classic-mac-for-dummies-4-retroterm.md).
