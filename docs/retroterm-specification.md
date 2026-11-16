# RetroTerm Specification

## Purpose

RetroTerm is a simple interactive serial terminal for ClassicMacStudio clients.
The first implementation targets classic 68k Macintosh systems; other client
operating systems should implement the same user-visible behavior while using
their native serial-port and text-view APIs.

## Functional requirements

- Let the user select the Macintosh serial endpoint: **MODEM** or **PRINTER**.
- Default the endpoint to **MODEM**.
- Let the user select a baud rate. The initial Classic Mac implementation
  supports 300, 600, 1,200, 1,800, 2,400, 3,600, 4,800, 7,200, 9,600, 14,400,
  19,200, 28,800, 38,400, and 57,600 baud, defaulting to **9,600**.
- Provide an **Open/Close** control and a visible result/status for every
  open, close, read, and write failure.
- Report the selected numeric baud rate in status and diagnostics (for
  example, `9600`); Macintosh Serial Manager baud constants are configuration
  codes, not printable baud-rate numbers.
- Show received text in a terminal text area. Clicking the view gives it
  keyboard focus; accepted keystrokes are sent immediately, without waiting
  for Enter or a transmit buffer. Wrap text that exceeds the view width onto
  the next display line.
- Provide a **Local Echo** checkbox. When enabled, successfully sent characters
  are also shown locally. When disabled, the view shows received text only.
  Local Echo defaults to on; users may disable it when the remote endpoint
  already echoes characters.
- Use 8 data bits, no parity, one stop bit (8-N-1), and no flow control.
- The initial display handles printable ASCII, CR/LF, tab, and backspace.
  Return sends carriage return. Text received from the serial endpoint is
  displayed as it arrives.
- Keep the connection closed at startup. The user explicitly opens it after
  choosing settings.
- Show a native window close box. Closing the window terminates the app and
  closes any open serial drivers before the window is disposed.
- Provide a visible Quit button that uses the same shutdown path as the
  window close box.
- While a port is open, disable port and baud selection. The user must close
  the connection before changing settings.
- In the ClassicMacStudio project, model the port and baud selectors as
  `popup-menu` controls and Local Echo as a native `checkbox`; keep those
  controls interactive in the shipped app rather than approximating them with
  push buttons.

## Classic Macintosh implementation

The 68k app uses the Serial Manager `.AIn`/`.AOut` drivers for MODEM and
`.BIn`/`.BOut` for PRINTER. It configures both drivers to the selected rate and
8-N-1, with flow control disabled. The main event loop polls the receive
buffer while remaining responsive to UI events. Each supported key event is
written immediately to the output driver. The text field retains a bounded 255-byte text buffer.

The current UI defaults are MODEM, 9600 baud, and Local Echo on. Port and
baud choices are made before opening the connection. The app does not change
AppleTalk or other system services; in particular, AppleTalk/LocalTalk may
occupy the Printer port.

For troubleshooting, the Classic Mac app appends timestamped startup, resource,
memory, UI, and serial-operation diagnostics to `RetroTerm.log` beside the
application, falling back to the current directory if the Process Manager
cannot provide the application location. The log is capped at 32 KiB and does
not record terminal payloads or typed characters; it is not a terminal
transcript.

## Cross-platform behavior

Future implementations should preserve the functional requirements above and
separate the terminal view from the operating-system serial adapter. The
adapter should expose:

- Enumerated/selectable endpoints with a clear endpoint name.
- Open/close using the chosen rate and framing.
- Non-blocking receive delivery to the view.
- Immediate writes of entered bytes, with write failures surfaced in status.
- Explicit teardown when the user closes the port or the app exits.

Operating-system-specific endpoint names and UI conventions may differ. A
platform may expose the two classic endpoint names only when the target device
is a Macintosh; it must not imply that MODEM/PRINTER names exist on unrelated
hardware.

## Non-goals for the first version

- ANSI/VT100 terminal emulation, cursor addressing, or escape-sequence
  processing.
- File transfer, protocol framing, or remote shell/session management.
- Hardware/software flow control, parity selection, or stop-bit selection.
- Line editing or send-on-Enter buffering.
- Persistent settings or a terminal transcript export. Diagnostic logging in
  the Classic Mac implementation is described above and does not capture the
  terminal session contents.
- Claiming physical cable continuity based only on a successful driver open.

## Acceptance checks

1. Startup shows MODEM, 9600 baud, Local Echo on, and a closed port.
2. Opening a free port updates the status and changes the button to **Close**;
   the status shows the selected numeric rate (for example, `9600 baud`).
   Open failures are visible and leave the app usable for another attempt.
3. Received printable data appears in the scrollback; long lines wrap to the
   next display row, and CR/LF advances to a new line.
4. Typing while connected transmits characters immediately. Local Echo only
   affects whether those transmitted characters are locally displayed.
5. Closing the port releases both serial drivers and restores the settings
   controls. The window close box and Quit button terminate the application
   through the same cleanup path.
