/**
 * @file main.c
 * @brief Native 68k implementation of the RetroTerm serial terminal.
 *
 * Loads the dialog resources, manages the Macintosh Serial Manager drivers,
 * and renders a bounded printable-ASCII terminal using monochrome QuickDraw.
 */
#include <Dialogs.h>
#include <Events.h>
#include <Files.h>
#include <Fonts.h>
#include <Menus.h>
#include <Memory.h>
#include <Multiverse.h>
#include <Processes.h>
#include <Quickdraw.h>
#include <Serial.h>
#include <Windows.h>
#include <stdio.h>
#include <string.h>

/** @name Dialog and control resource identifiers
 * @{
 */
#define RETROTERM_DIALOG_ID 128
#define RETROTERM_STATUS_ITEM 1
#define RETROTERM_PORT_ITEM 3
#define RETROTERM_BAUD_ITEM 5
#define RETROTERM_OPEN_ITEM 6
#define RETROTERM_ECHO_ITEM 7
#define RETROTERM_TERMINAL_ITEM 8
#define RETROTERM_QUIT_ITEM 10
#define RETROTERM_PORT_MENU_ID 259
#define RETROTERM_BAUD_MENU_ID 261
#define RETROTERM_BAUD_RATE_COUNT 14
#define RETROTERM_SERIAL_BUFFER_SIZE 2048
#define RETROTERM_READ_SIZE 256
#define RETROTERM_LOG_MAX_SIZE (32L * 1024L)
/** @} */

/** @brief Pascal-string filename used for application diagnostics. */
static const unsigned char gLogFileName[] = {
    13, 'R', 'e', 't', 'r', 'o', 'T', 'e', 'r', 'm', '.', 'l', 'o', 'g'
};

/** @name Macintosh Serial Manager driver names
 * @{
 */
static const unsigned char gModemInputDriverName[] = { 4, '.', 'A', 'I', 'n' };
static const unsigned char gModemOutputDriverName[] = { 5, '.', 'A', 'O', 'u', 't' };
static const unsigned char gPrinterInputDriverName[] = { 4, '.', 'B', 'I', 'n' };
static const unsigned char gPrinterOutputDriverName[] = { 5, '.', 'B', 'O', 'u', 't' };
/** @} */

/**
 * @brief Serial Manager baud configuration codes, indexed by popup selection.
 * @note These are bit-field codes, not printable numeric baud rates.
 */
static const short gBaudRates[RETROTERM_BAUD_RATE_COUNT] = {
    baud300,
    baud600,
    baud1200,
    baud1800,
    baud2400,
    baud3600,
    baud4800,
    baud7200,
    baud9600,
    baud14400,
    baud19200,
    baud28800,
    baud38400,
    baud57600
};

/** @brief Human-readable baud rates aligned with @ref gBaudRates. */
static const long gBaudRateValues[RETROTERM_BAUD_RATE_COUNT] = {
    300,
    600,
    1200,
    1800,
    2400,
    3600,
    4800,
    7200,
    9600,
    14400,
    19200,
    28800,
    38400,
    57600
};

/**
 * @name Application state
 * @brief Dialog resources, serial references, terminal buffer, and status.
 * @{
 */
static DialogPtr gDialog;
static MenuHandle gPortMenu;
static MenuHandle gBaudMenu;
static short gPortSelection = 1;
static short gBaudSelection = 9;
static short gSerialInputRef;
static short gSerialOutputRef;
static unsigned char gSerialBuffer[RETROTERM_SERIAL_BUFFER_SIZE];
static unsigned char gReadBuffer[RETROTERM_READ_SIZE];
static unsigned char gTerminalText[256];
static Boolean gPreviousByteWasCR;
static Boolean gTerminalFocused;
static Boolean gQuitRequested;
static Boolean gLocalEcho = true;
static Rect gTerminalViewBounds;
static FSSpec gLogFileSpec;
static Boolean gLogFileSpecReady;
static Boolean gLogUsesDefaultDirectory;
static Boolean gStartupComplete;
static OSErr gLogError;
static unsigned long gReceivedByteCount;
static unsigned long gSentByteCount;
/** @} */

static void TerminalRefreshView(void);
static void TerminalDrawTextView(void);
static void TerminalDrawPopupIndicator(short item);

/**
 * @brief Create the diagnostics file in a specified HFS directory if needed.
 * @param volumeRefNum HFS volume reference number.
 * @param directoryId HFS directory ID.
 * @return The File Manager result code.
 */
static OSErr TerminalPrepareLogFile(
    short volumeRefNum,
    long directoryId)
{
    OSErr error = FSMakeFSSpec(
        volumeRefNum,
        directoryId,
        (ConstStringPtr)gLogFileName,
        &gLogFileSpec);

    if (error == fnfErr)
    {
        error = FSpCreate(&gLogFileSpec, 'RTrm', 'TEXT', smSystemScript);
        if (error == dupFNErr)
            error = noErr;
    }
    return error;
}

/**
 * @brief Locate the application log, falling back to the default directory.
 * @return noErr when a usable log file specification is available.
 */
static OSErr TerminalFindLogFile(void)
{
    ProcessSerialNumber currentProcess;
    ProcessInfoRec processInfo;
    FSSpec applicationFile;
    unsigned char processName[64];
    OSErr error;

    if (gLogFileSpecReady)
        return noErr;

    memset(&currentProcess, 0, sizeof(currentProcess));
    memset(&processInfo, 0, sizeof(processInfo));
    memset(&applicationFile, 0, sizeof(applicationFile));
    memset(processName, 0, sizeof(processName));
    processInfo.processInfoLength = sizeof(processInfo);
    processInfo.processName = (StringPtr)processName;
    processInfo.processAppSpec = &applicationFile;

    error = GetCurrentProcess(&currentProcess);
    if (error == noErr)
        error = GetProcessInformation(&currentProcess, &processInfo);
    if (error == noErr)
        error = TerminalPrepareLogFile(
            applicationFile.vRefNum,
            applicationFile.parID);

    if (error != noErr)
    {
        error = TerminalPrepareLogFile(0, 0);
        if (error == noErr)
            gLogUsesDefaultDirectory = true;
    }
    if (error == noErr)
        gLogFileSpecReady = true;
    return error;
}

/**
 * @brief Append one timestamped line to the bounded diagnostic log.
 * @param message Printable diagnostic text without a line terminator.
 * @return The first File Manager error, or noErr on success.
 */
static OSErr TerminalAppendLog(const char *message)
{
    short fileRef = 0;
    long fileLength = 0;
    LONGINT writeLength;
    unsigned long timestamp;
    char line[256];
    int lineLength;
    OSErr error;
    OSErr closeError;

    if (message == NULL || strlen(message) > 220)
        return paramErr;

    error = TerminalFindLogFile();
    if (error != noErr)
        return error;
    error = FSpOpenDF(&gLogFileSpec, fsWrPerm, &fileRef);
    if (error != noErr)
        return error;

    error = GetEOF(fileRef, &fileLength);
    if (error == noErr && fileLength >= RETROTERM_LOG_MAX_SIZE)
        error = SetEOF(fileRef, 0);
    if (error == noErr)
        error = SetFPos(fileRef, fsFromLEOF, 0);
    if (error == noErr)
    {
        GetDateTime(&timestamp);
        lineLength = sprintf(line, "T=%lu %s\r", timestamp, message);
        writeLength = lineLength;
        error = FSWrite(fileRef, &writeLength, (Ptr)line);
        if (error == noErr && writeLength != lineLength)
            error = dskFulErr;
    }

    closeError = FSClose(fileRef);
    if (error == noErr)
        error = closeError;
    return error;
}

/**
 * @brief Append a diagnostic message until the first logging error occurs.
 * @param message Diagnostic text to write.
 */
static void TerminalLog(const char *message)
{
    OSErr error;

    if (gLogError != noErr)
        return;
    error = TerminalAppendLog(message);
    if (error != noErr)
        gLogError = error;
}

/**
 * @brief Log a named integer value.
 * @param name Diagnostic field name.
 * @param value Integer field value.
 */
static void TerminalLogValue(const char *name, long value)
{
    char message[128];

    sprintf(message, "%s=%ld", name, value);
    TerminalLog(message);
}

/**
 * @brief Log free heap and largest allocatable block at a startup stage.
 * @param stage Short name for the stage being measured.
 */
static void TerminalLogMemory(const char *stage)
{
    long grow = 0;
    long freeBytes = FreeMem();
    long largestBlock = MaxMem(&grow);
    char message[160];

    sprintf(
        message,
        "MEMORY stage=%s free=%ld largest=%ld grow=%ld",
        stage,
        freeBytes,
        largestBlock,
        grow);
    TerminalLog(message);
}

/**
 * @brief Log the type, handle presence, and bounds of a dialog item.
 * @param item One-based DITL item number.
 */
static void TerminalLogDialogItem(short item)
{
    short itemType;
    Handle itemHandle;
    Rect bounds;
    char message[160];

    GetDialogItem(gDialog, item, &itemType, &itemHandle, &bounds);
    sprintf(
        message,
        "DITL item=%d type=%d handle=%d bounds=%d,%d,%d,%d",
        (int)item,
        (int)itemType,
        itemHandle != NULL,
        (int)bounds.top,
        (int)bounds.left,
        (int)bounds.bottom,
        (int)bounds.right);
    TerminalLog(message);
}

/**
 * @brief Replace the status item's text and redraw the related controls.
 * @param text Null-terminated status message.
 */
static void TerminalSetStatus(const char *text)
{
    unsigned char pascalText[256];
    short itemType;
    Handle itemHandle;
    Rect bounds;
    size_t length = strlen(text);

    if (length > 255)
        length = 255;
    pascalText[0] = (unsigned char)length;
    memcpy(pascalText + 1, text, length);
    if (!gStartupComplete)
        TerminalLog("STARTUP_STATUS_SET_BEGIN");
    GetDialogItem(
        gDialog,
        RETROTERM_STATUS_ITEM,
        &itemType,
        &itemHandle,
        &bounds);
    (void)itemType;
    (void)bounds;
    if (itemHandle == NULL)
    {
        TerminalLog("STARTUP_STATUS_ITEM_NULL");
        return;
    }
    SetDialogItemText(itemHandle, (StringPtr)pascalText);
    if (!gStartupComplete)
        TerminalLog("STARTUP_STATUS_TEXT_SET");
    DrawDialog(gDialog);
    if (!gStartupComplete)
        TerminalLog("STARTUP_DIALOG_DRAW_DONE");
    TerminalDrawPopupIndicator(RETROTERM_PORT_ITEM);
    TerminalDrawPopupIndicator(RETROTERM_BAUD_ITEM);
    TerminalDrawTextView();
}

/**
 * @brief Initialize the terminal display buffer with a short message.
 * @param text Null-terminated printable text.
 */
static void TerminalSetTerminalText(const char *text)
{
    size_t length = strlen(text);

    if (length > 255)
        length = 255;
    gTerminalText[0] = (unsigned char)length;
    memcpy(gTerminalText + 1, text, length);
    if (!gStartupComplete)
        TerminalLog("STARTUP_TERMINAL_TEXT_SET_BEGIN");
    TerminalRefreshView();
}

/**
 * @brief Set the title of a dialog control.
 * @param item One-based DITL item number.
 * @param text Null-terminated control title.
 */
static void TerminalSetControlTitle(short item, const char *text)
{
    unsigned char pascalText[256];
    short itemType;
    Handle itemHandle;
    Rect bounds;
    size_t length = strlen(text);

    if (length > 255)
        length = 255;
    pascalText[0] = (unsigned char)length;
    memcpy(pascalText + 1, text, length);
    GetDialogItem(gDialog, item, &itemType, &itemHandle, &bounds);
    (void)itemType;
    (void)bounds;
    if (itemHandle == NULL)
    {
        TerminalLogValue("CONTROL_TITLE_ITEM_NULL", item);
        return;
    }
    SetControlTitle((ControlHandle)itemHandle, (StringPtr)pascalText);
}

/**
 * @brief Enable or disable port settings and update the open/close caption.
 */
static void TerminalUpdateControls(void)
{
    short itemType;
    Handle itemHandle;
    Rect bounds;

    GetDialogItem(gDialog, RETROTERM_PORT_ITEM, &itemType, &itemHandle, &bounds);
    (void)itemType;
    (void)bounds;
    if (gSerialInputRef != 0 || gSerialOutputRef != 0)
    {
        HiliteControl((ControlHandle)itemHandle, 255);
        GetDialogItem(gDialog, RETROTERM_BAUD_ITEM, &itemType, &itemHandle, &bounds);
        HiliteControl((ControlHandle)itemHandle, 255);
        TerminalSetControlTitle(RETROTERM_OPEN_ITEM, "Close");
    }
    else
    {
        HiliteControl((ControlHandle)itemHandle, 0);
        GetDialogItem(gDialog, RETROTERM_BAUD_ITEM, &itemType, &itemHandle, &bounds);
        HiliteControl((ControlHandle)itemHandle, 0);
        TerminalSetControlTitle(RETROTERM_OPEN_ITEM, "Open");
    }
}

/**
 * @brief Close both serial drivers, attempting both even if one close fails.
 * @return The first close error, or noErr if both drivers closed.
 */
static OSErr TerminalCloseDrivers(void)
{
    OSErr firstError = noErr;
    OSErr error;

    TerminalLog("SERIAL_CLOSE_BEGIN");
    if (gSerialOutputRef != 0)
    {
        error = CloseDriver(gSerialOutputRef);
        TerminalLogValue("SERIAL_CLOSE_OUTPUT", error);
        if (error == noErr)
            gSerialOutputRef = 0;
        else
            firstError = error;
    }
    if (gSerialInputRef != 0)
    {
        error = CloseDriver(gSerialInputRef);
        TerminalLogValue("SERIAL_CLOSE_INPUT", error);
        if (error == noErr)
            gSerialInputRef = 0;
        else if (firstError == noErr)
            firstError = error;
    }
    TerminalUpdateControls();
    return firstError;
}

/**
 * @brief Open the selected input/output drivers and configure serial framing.
 * @return The Serial Manager result code; noErr indicates the port is open.
 */
static OSErr TerminalOpenPort(void)
{
    Boolean usePrinter = (Boolean)(gPortSelection == 2);
    const unsigned char *inputName = usePrinter
        ? gPrinterInputDriverName
        : gModemInputDriverName;
    const unsigned char *outputName = usePrinter
        ? gPrinterOutputDriverName
        : gModemOutputDriverName;
    short configuration = gBaudRates[gBaudSelection - 1] + stop10 + data8 + noParity;
    SerShk handshake;
    OSErr error;
    OSErr closeError;
    char status[96];
    LONGINT bufferSize = RETROTERM_SERIAL_BUFFER_SIZE;

    TerminalLogValue(
        gPortSelection == 2 ? "SERIAL_OPEN_PRINTER_BAUD" : "SERIAL_OPEN_MODEM_BAUD",
        gBaudRateValues[gBaudSelection - 1]);
    error = OpenDriver((ConstStringPtr)inputName, &gSerialInputRef);
    TerminalLogValue("SERIAL_OPEN_INPUT", error);
    if (error == noErr)
    {
        error = OpenDriver((ConstStringPtr)outputName, &gSerialOutputRef);
        TerminalLogValue("SERIAL_OPEN_OUTPUT", error);
    }
    if (error == noErr)
    {
        error = SerReset(gSerialInputRef, configuration);
        TerminalLogValue("SERIAL_RESET_INPUT", error);
    }
    if (error == noErr)
    {
        error = SerReset(gSerialOutputRef, configuration);
        TerminalLogValue("SERIAL_RESET_OUTPUT", error);
    }
    if (error == noErr)
    {
        error = SerSetBuf(
            gSerialInputRef,
            (Ptr)gSerialBuffer,
            (short)bufferSize);
        TerminalLogValue("SERIAL_SET_BUFFER", error);
    }
    memset(&handshake, 0, sizeof(handshake));
    if (error == noErr)
    {
        error = SerHShake(gSerialInputRef, &handshake);
        TerminalLogValue("SERIAL_SET_HANDSHAKE", error);
    }

    if (error != noErr)
    {
        TerminalLogValue("SERIAL_OPEN_FAILURE", error);
        closeError = TerminalCloseDrivers();
        TerminalLogValue("SERIAL_OPEN_CLEANUP", closeError);
        if (closeError != noErr)
            sprintf(status, "Open error %d; close error %d.", (int)error, (int)closeError);
        else
            sprintf(status, "Open failed (serial error %d).", (int)error);
        TerminalSetStatus(status);
        return error;
    }

    TerminalLog("SERIAL_OPEN_SUCCESS");
    TerminalUpdateControls();
    gTerminalFocused = true;
    sprintf(
        status,
        "Opened %s at %ld baud, 8-N-1.",
        usePrinter ? "PRINTER" : "MODEM",
        gBaudRateValues[gBaudSelection - 1]);
    TerminalSetStatus(status);
    return noErr;
}

/**
 * @brief Close the active port, update counters, and refresh the dialog.
 * @return The first driver close error, or noErr on successful shutdown.
 */
static OSErr TerminalClosePort(void)
{
    OSErr error = TerminalCloseDrivers();
    char status[80];

    TerminalLogValue("SERIAL_CLOSE_RESULT", error);
    TerminalLogValue("SERIAL_TOTAL_RX_BYTES", (long)gReceivedByteCount);
    TerminalLogValue("SERIAL_TOTAL_TX_BYTES", (long)gSentByteCount);
    gTerminalFocused = false;
    if (error == noErr)
        TerminalSetStatus("Port closed.");
    else
    {
        sprintf(status, "Close failed (serial error %d).", (int)error);
        TerminalSetStatus(status);
    }
    return error;
}

/**
 * @brief Append a line break, scrolling the oldest text when the buffer is full.
 */
static void TerminalAdvanceLine(void)
{
    if (gTerminalText[0] == 255)
    {
        memmove(gTerminalText + 1, gTerminalText + 2, 254);
        gTerminalText[0] = 254;
    }
    gTerminalText[++gTerminalText[0]] = '\r';
    gPreviousByteWasCR = true;
}

/**
 * @brief Append a supported received byte to the terminal display.
 * @param value Received serial byte.
 * @note Printable 7-bit ASCII and selected editing controls are supported;
 *       unsupported control and high-bit bytes are ignored.
 */
static void TerminalAppendByte(unsigned char value)
{
    short spaces;

    if (value == '\r')
    {
        TerminalAdvanceLine();
        return;
    }
    if (value == '\n')
    {
        if (!gPreviousByteWasCR)
            TerminalAdvanceLine();
        gPreviousByteWasCR = false;
        return;
    }
    gPreviousByteWasCR = false;

    if (value == '\b' || value == 0x7F)
    {
        if (gTerminalText[0] > 0)
            --gTerminalText[0];
        return;
    }
    if (value == '\t')
    {
        spaces = 4;
        while (spaces-- > 0)
            TerminalAppendByte(' ');
        return;
    }
    if (value < 0x20 || value > 0x7E)
        return;
    if (gTerminalText[0] == 255)
    {
        memmove(gTerminalText + 1, gTerminalText + 2, 254);
        gTerminalText[0] = 254;
    }
    gTerminalText[++gTerminalText[0]] = value;
}

/**
 * @brief Redraw the terminal text region after its buffer changes.
 */
static void TerminalRefreshView(void)
{
    if (gDialog == NULL)
        return;
    if (!gStartupComplete)
        TerminalLog("STARTUP_TERMINAL_REFRESH_BEGIN");
    TerminalDrawTextView();
    if (!gStartupComplete)
        TerminalLog("STARTUP_TERMINAL_DRAW_DONE");
}

/**
 * @brief Read available serial bytes and append them to the terminal buffer.
 */
static void TerminalPollSerial(void)
{
    LONGINT available = 0;
    LONGINT count;
    OSErr error;
    short index;
    Boolean changed = false;

    if (gSerialInputRef == 0)
        return;
    error = SerGetBuf(gSerialInputRef, &available);
    if (error != noErr)
    {
        char status[72];
        TerminalLogValue("SERIAL_GET_BUFFER_ERROR", error);
        sprintf(status, "Read status failed (serial error %d).", (int)error);
        TerminalSetStatus(status);
        return;
    }
    if (available <= 0)
        return;
    count = available > RETROTERM_READ_SIZE ? RETROTERM_READ_SIZE : available;
    error = FSRead(gSerialInputRef, &count, gReadBuffer);
    if (error != noErr)
    {
        char status[72];
        TerminalLogValue("SERIAL_READ_ERROR", error);
        sprintf(status, "Read failed (serial error %d).", (int)error);
        TerminalSetStatus(status);
        return;
    }
    gReceivedByteCount += (unsigned long)count;
    TerminalLogValue("SERIAL_RX_BATCH_BYTES", count);
    for (index = 0; index < count; ++index)
    {
        TerminalAppendByte(gReadBuffer[index]);
        changed = true;
    }
    if (changed)
        TerminalRefreshView();
}

/**
 * @brief Check whether a byte is supported by the terminal's send path.
 * @param value Candidate byte.
 * @return true for printable 7-bit ASCII and supported control keys.
 */
static Boolean TerminalCanSend(unsigned char value)
{
    return (Boolean)(
        (value >= 0x20 && value <= 0x7E) ||
        value == '\r' ||
        value == '\n' ||
        value == '\t' ||
        value == '\b' ||
        value == 0x7F);
}

/**
 * @brief Send a supported character and optionally display local echo.
 * @param value Character to transmit.
 */
static void TerminalSendCharacter(unsigned char value)
{
    LONGINT count = 1;
    OSErr error;
    char status[72];

    if (!TerminalCanSend(value))
        return;
    if (gSerialOutputRef == 0)
    {
        TerminalSetStatus("Port is closed. Open it before typing.");
        return;
    }
    error = FSWrite(gSerialOutputRef, &count, &value);
    if (error != noErr || count != 1)
    {
        TerminalLogValue("SERIAL_WRITE_ERROR", error);
        sprintf(status, "Write failed (serial error %d).", (int)error);
        TerminalSetStatus(status);
        return;
    }
    ++gSentByteCount;
    if (gLocalEcho)
    {
        TerminalAppendByte(value);
        TerminalRefreshView();
    }
}

/**
 * @brief Display a popup menu and store the chosen one-based item.
 * @param item One-based DITL item number of the popup control.
 * @param menu Menu resource handle.
 * @param selection In/out selected menu item; zero means no selection.
 * @note The selected item is written through @p selection when chosen.
 */
static void TerminalShowPopup(
    short item,
    MenuHandle menu,
    short *selection)
{
    short itemType;
    Handle itemHandle;
    Rect bounds;
    Point popupPoint;
    GrafPtr previousPort;
    long result;
    short selectedItem;
    Str255 title;

    GetPort(&previousPort);
    SetPort((GrafPtr)gDialog);
    GetMouse(&popupPoint);
    LocalToGlobal(&popupPoint);
    SetPort(previousPort);
    result = PopUpMenuSelect(menu, popupPoint.v, popupPoint.h, *selection);
    selectedItem = (short)(result & 0xFFFF);
    if (selectedItem <= 0)
        return;
    *selection = selectedItem;
    TerminalLogValue("POPUP_SELECTED_ITEM", selectedItem);
    GetMenuItemText(menu, selectedItem, title);
    GetDialogItem(gDialog, item, &itemType, &itemHandle, &bounds);
    (void)itemType;
    (void)bounds;
    SetControlTitle((ControlHandle)itemHandle, title);
    InvalRect(&bounds);
}

/**
 * @brief Draw the monochrome disclosure indicator for a popup control.
 * @param item One-based DITL item number.
 */
static void TerminalDrawPopupIndicator(short item)
{
    short itemType;
    Handle itemHandle;
    Rect bounds;
    GrafPtr previousPort;
    short x;
    short y;

    GetDialogItem(gDialog, item, &itemType, &itemHandle, &bounds);
    if (itemHandle == NULL)
        return;
    x = (short)(bounds.right - 9);
    y = (short)((bounds.top + bounds.bottom) / 2);
    GetPort(&previousPort);
    SetPort((GrafPtr)gDialog);
    PenNormal();
    MoveTo((short)(x - 3), (short)(y - 1));
    LineTo((short)(x + 3), (short)(y - 1));
    LineTo(x, (short)(y + 2));
    LineTo((short)(x - 3), (short)(y - 1));
    SetPort(previousPort);
}

/**
 * @brief Draw wrapped terminal-buffer text in the dialog's text region.
 */
static void TerminalDrawTextView(void)
{
    short itemType;
    Handle itemHandle;
    Rect bounds;
    GrafPtr previousPort;
    GrafPtr dialogPort = (GrafPtr)gDialog;
    FontInfo fontInfo;
    short previousFont;
    short previousSize;
    short previousFace;
    short textLength = gTerminalText[0];
    short lineStarts[256];
    short lineLengths[256];
    short lineCount = 0;
    short visibleRows;
    short firstVisibleLine;
    short lineStart = 0;
    short row = 0;
    short index;
    short lineHeight;
    short availableWidth;

    GetDialogItem(
        gDialog,
        RETROTERM_TERMINAL_ITEM,
        &itemType,
        &itemHandle,
        &bounds);
    if (itemHandle == NULL)
    {
        TerminalLog("TEXT_VIEW_ITEM_NULL");
        return;
    }

    if (!gStartupComplete)
        TerminalLog("STARTUP_TEXT_VIEW_DRAW_BEGIN");
    GetPort(&previousPort);
    SetPort(dialogPort);
    previousFont = dialogPort->txFont;
    previousSize = dialogPort->txSize;
    previousFace = dialogPort->txFace;
    TextFont(systemFont);
    TextSize(9);
    TextFace(0);
    GetFontInfo(&fontInfo);

    lineHeight = (short)(fontInfo.ascent + fontInfo.descent + fontInfo.leading);
    if (lineHeight <= 0)
        lineHeight = 12;
    visibleRows = (short)((bounds.bottom - bounds.top - 8) / lineHeight);
    if (visibleRows < 1)
        visibleRows = 1;
    availableWidth = (short)(bounds.right - bounds.left - 8);

    for (index = 0; index <= textLength; ++index)
    {
        if (index == textLength || gTerminalText[index + 1] == '\r')
        {
            short logicalLineLength = (short)(index - lineStart);
            short offset = 0;

            if (logicalLineLength == 0)
            {
                lineStarts[lineCount] = lineStart;
                lineLengths[lineCount++] = 0;
            }
            while (offset < logicalLineLength)
            {
                short wrappedLength = 0;

                while (offset + wrappedLength < logicalLineLength &&
                    TextWidth(
                        (Ptr)(gTerminalText + 1 + lineStart + offset),
                        0,
                        (short)(wrappedLength + 1)) <= availableWidth)
                    ++wrappedLength;
                if (wrappedLength == 0)
                    wrappedLength = 1;
                lineStarts[lineCount] = (short)(lineStart + offset);
                lineLengths[lineCount++] = wrappedLength;
                offset = (short)(offset + wrappedLength);
            }
            lineStart = (short)(index + 1);
        }
    }

    firstVisibleLine = lineCount > visibleRows
        ? (short)(lineCount - visibleRows)
        : 0;

    EraseRect(&bounds);
    FrameRect(&bounds);
    for (index = firstVisibleLine; index < lineCount && row < visibleRows; ++index)
    {
        short lineLength = lineLengths[index];
        if (lineLength > 0)
        {
            short baseline = (short)(
                bounds.top + 4 + row * lineHeight + fontInfo.ascent);
            Ptr lineText = (Ptr)(gTerminalText + 1 + lineStarts[index]);
            MoveTo((short)(bounds.left + 4), baseline);
            DrawText(lineText, 0, lineLength);
        }
        ++row;
    }
    TextFont(previousFont);
    TextSize(previousSize);
    TextFace(previousFace);
    SetPort(previousPort);
    if (!gStartupComplete)
        TerminalLog("STARTUP_TEXT_VIEW_DRAW_DONE");
}

/**
 * @brief Track a click on the local-echo checkbox.
 * @param globalMouse Mouse location in global coordinates.
 * @return true if the checkbox was hit and toggled; otherwise false.
 */
static Boolean TerminalTrackLocalEchoCheckbox(Point globalMouse)
{
    Point localMouse = globalMouse;
    GrafPtr previousPort;
    short itemType;
    Handle itemHandle;
    Rect bounds;

    GetPort(&previousPort);
    SetPort((GrafPtr)gDialog);
    GlobalToLocal(&localMouse);
    GetDialogItem(
        gDialog,
        RETROTERM_ECHO_ITEM,
        &itemType,
        &itemHandle,
        &bounds);
    if (itemHandle == NULL ||
        (itemType & ctrlItem) == 0 ||
        !PtInRect(localMouse, &bounds))
    {
        SetPort(previousPort);
        return false;
    }

    TrackControl(
        (ControlHandle)itemHandle,
        globalMouse,
        (ControlActionUPP)0);
    SetPort((GrafPtr)gDialog);
    Draw1Control((ControlHandle)itemHandle);
    SetPort(previousPort);

    gLocalEcho = (Boolean)(GetControlValue((ControlHandle)itemHandle) != 0);
    TerminalLogValue("LOCAL_ECHO", gLocalEcho);
    TerminalSetStatus(gLocalEcho ? "Local Echo is on." : "Local Echo is off.");
    return true;
}

/**
 * @brief Track the port or baud popup menu under the mouse.
 * @param globalMouse Mouse location in global coordinates.
 * @return true if a popup was hit and processed; otherwise false.
 */
static Boolean TerminalTrackPopupMenu(Point globalMouse)
{
    Point localMouse = globalMouse;
    GrafPtr previousPort;
    short items[2] = { RETROTERM_PORT_ITEM, RETROTERM_BAUD_ITEM };
    short index;
    short itemType;
    Handle itemHandle;
    Rect bounds;

    if (gSerialInputRef != 0 || gSerialOutputRef != 0)
        return false;
    GetPort(&previousPort);
    SetPort((GrafPtr)gDialog);
    GlobalToLocal(&localMouse);
    for (index = 0; index < 2; ++index)
    {
        short item = items[index];
        GetDialogItem(gDialog, item, &itemType, &itemHandle, &bounds);
        if (PtInRect(localMouse, &bounds))
        {
            SetPort(previousPort);
            TerminalShowPopup(
                item,
                item == RETROTERM_PORT_ITEM ? gPortMenu : gBaudMenu,
                item == RETROTERM_PORT_ITEM ? &gPortSelection : &gBaudSelection);
            TerminalLogValue(
                item == RETROTERM_PORT_ITEM ? "PORT_SELECTION" : "BAUD_SELECTION",
                item == RETROTERM_PORT_ITEM ? gPortSelection : gBaudSelection);
            if (item == RETROTERM_PORT_ITEM)
                TerminalSetStatus("Port selection changed. Open the port to connect.");
            else
                TerminalSetStatus("Baud rate changed. Open the port to connect.");
            TerminalDrawPopupIndicator(item);
            return true;
        }
    }
    SetPort(previousPort);
    return false;
}

/**
 * @brief Dispatch a dialog item selection to its application action.
 * @param item One-based DITL item number reported by ModalDialog.
 */
static void TerminalHandleDialogItem(short item)
{
    short itemType;
    Handle itemHandle;
    Rect bounds;

    TerminalLogValue("DIALOG_ITEM_HIT", item);
    if (item == RETROTERM_PORT_ITEM)
    {
        TerminalShowPopup(item, gPortMenu, &gPortSelection);
        TerminalSetStatus("Port selection changed. Open the port to connect.");
    }
    else if (item == RETROTERM_BAUD_ITEM)
    {
        TerminalShowPopup(item, gBaudMenu, &gBaudSelection);
        TerminalSetStatus("Baud rate changed. Open the port to connect.");
    }
    else if (item == RETROTERM_OPEN_ITEM)
    {
        if (gSerialInputRef != 0 || gSerialOutputRef != 0)
            TerminalClosePort();
        else
            TerminalOpenPort();
    }
    else if (item == RETROTERM_ECHO_ITEM)
    {
        GetDialogItem(
            gDialog,
            item,
            &itemType,
            &itemHandle,
            &bounds);
        (void)itemType;
        (void)bounds;
        gLocalEcho = (Boolean)(GetControlValue((ControlHandle)itemHandle) != 0);
        TerminalLogValue("LOCAL_ECHO", gLocalEcho);
        TerminalSetStatus(gLocalEcho ? "Local Echo is on." : "Local Echo is off.");
    }
    else if (item == RETROTERM_QUIT_ITEM)
    {
        TerminalLog("APP_QUIT_BUTTON_CLICKED");
        gQuitRequested = true;
    }
}

/**
 * @brief Focus the dialog text view when the user clicks its content region.
 * @param event Mouse-down event in global coordinates.
 * @return true if the event closed the window; otherwise false.
 */
static Boolean TerminalFocusViewOnMouseDown(const EventRecord *event)
{
    WindowPtr clickedWindow = NULL;
    short windowPart = FindWindow(event->where, &clickedWindow);

    if (clickedWindow != (WindowPtr)gDialog)
        return false;
    if (windowPart == inDrag)
    {
        DragWindow((WindowPtr)gDialog, event->where, &qd.screenBits.bounds);
        return false;
    }
    if (windowPart == inGoAway)
    {
        if (TrackGoAway((WindowPtr)gDialog, event->where))
        {
            TerminalLog("APP_CLOSE_BOX_CLICKED");
            return true;
        }
        return false;
    }
    if (windowPart != inContent)
        return false;

    {
        Point localPoint = event->where;
        SetPort((GrafPtr)gDialog);
        GlobalToLocal(&localPoint);
        if (PtInRect(localPoint, &gTerminalViewBounds))
        {
            gTerminalFocused = true;
            return false;
        }
    }
    gTerminalFocused = false;
    return false;
}

/**
 * @brief Initialize RetroTerm and run its native Macintosh event loop.
 * @return Zero after normal shutdown; nonzero if required resources are absent.
 */
int main(void)
{
    Boolean running = true;
    EventRecord event;

    TerminalLog("APP_MAIN_ENTER version=0.2");
    if (gLogUsesDefaultDirectory)
        TerminalLog("LOG_LOCATION=default-directory-fallback");
    TerminalLog("INIT_GRAF_BEGIN");
    InitGraf(&qd.thePort);
    TerminalLog("INIT_GRAF_DONE");
    TerminalLogMemory("init-graf");
    TerminalLog("INIT_FONTS_BEGIN");
    InitFonts();
    TerminalLog("INIT_FONTS_DONE");
    TerminalLog("INIT_WINDOWS_BEGIN");
    InitWindows();
    TerminalLog("INIT_WINDOWS_DONE");
    TerminalLog("INIT_MENUS_BEGIN");
    InitMenus();
    TerminalLog("INIT_MENUS_DONE");
    TerminalLog("INIT_DIALOGS_BEGIN");
    InitDialogs(NULL);
    TerminalLog("INIT_DIALOGS_DONE");
    TerminalLog("INIT_CURSOR_BEGIN");
    InitCursor();
    TerminalLog("INIT_CURSOR_DONE");

    TerminalLog("GET_DIALOG_BEGIN id=128");
    gDialog = GetNewDialog(RETROTERM_DIALOG_ID, NULL, (WindowPtr)-1);
    TerminalLogValue("GET_DIALOG_RESULT", gDialog != NULL);
    if (gDialog != NULL)
        TerminalLogMemory("dialog-loaded");
    TerminalLog("GET_PORT_MENU_BEGIN id=259");
    gPortMenu = GetMenu(RETROTERM_PORT_MENU_ID);
    TerminalLogValue("GET_PORT_MENU_RESULT", gPortMenu != NULL);
    TerminalLog("GET_BAUD_MENU_BEGIN id=261");
    gBaudMenu = GetMenu(RETROTERM_BAUD_MENU_ID);
    TerminalLogValue("GET_BAUD_MENU_RESULT", gBaudMenu != NULL);
    if (gDialog == NULL || gPortMenu == NULL || gBaudMenu == NULL)
    {
        TerminalLog("STARTUP_RESOURCE_FAILURE");
        return 1;
    }

    TerminalLogDialogItem(RETROTERM_STATUS_ITEM);
    TerminalLogDialogItem(RETROTERM_PORT_ITEM);
    TerminalLogDialogItem(RETROTERM_BAUD_ITEM);
    TerminalLogDialogItem(RETROTERM_OPEN_ITEM);
    TerminalLogDialogItem(RETROTERM_ECHO_ITEM);
    TerminalLogDialogItem(RETROTERM_TERMINAL_ITEM);

    {
        short itemType;
        Handle itemHandle;
        TerminalLog("GET_TERMINAL_BOUNDS_BEGIN");
        GetDialogItem(gDialog, RETROTERM_TERMINAL_ITEM, &itemType, &itemHandle, &gTerminalViewBounds);
        TerminalLogValue("GET_TERMINAL_BOUNDS_HANDLE", itemHandle != NULL);
        TerminalLogValue("TERMINAL_ITEM_TYPE", itemType);
    }
    {
        short itemType;
        Handle itemHandle;
        Rect bounds;
        Str255 title;

        GetDialogItem(
            gDialog,
            RETROTERM_PORT_ITEM,
            &itemType,
            &itemHandle,
            &bounds);
        TerminalLog("PORT_MENU_TITLE_BEGIN");
        GetMenuItemText(gPortMenu, gPortSelection, title);
        TerminalLog("PORT_MENU_TITLE_DONE");
        SetControlTitle((ControlHandle)itemHandle, title);
        TerminalLog("PORT_CONTROL_TITLE_SET");

        GetDialogItem(
            gDialog,
            RETROTERM_BAUD_ITEM,
            &itemType,
            &itemHandle,
            &bounds);
        TerminalLog("BAUD_MENU_TITLE_BEGIN");
        GetMenuItemText(gBaudMenu, gBaudSelection, title);
        TerminalLog("BAUD_MENU_TITLE_DONE");
        SetControlTitle((ControlHandle)itemHandle, title);
        TerminalLog("BAUD_CONTROL_TITLE_SET");

        GetDialogItem(gDialog, RETROTERM_ECHO_ITEM, &itemType, &itemHandle, &bounds);
        TerminalLogValue("ECHO_CONTROL_HANDLE", itemHandle != NULL);
        SetControlValue((ControlHandle)itemHandle, gLocalEcho ? 1 : 0);
        TerminalLogValue("LOCAL_ECHO_INITIAL", gLocalEcho);
        TerminalLog("ECHO_CONTROL_INITIALIZED");
    }

    TerminalLog("SHOW_WINDOW_BEGIN");
    ShowWindow((WindowPtr)gDialog);
    TerminalLog("SHOW_WINDOW_DONE");
    TerminalLog("UPDATE_CONTROLS_BEGIN");
    TerminalUpdateControls();
    TerminalLog("UPDATE_CONTROLS_DONE");
    TerminalLog("INITIAL_STATUS_BEGIN");
    TerminalSetStatus("Port closed. Select settings, then click Open.");
    TerminalLog("INITIAL_STATUS_DONE");
    TerminalSetTerminalText("Terminal ready. Open the port, click here, and type.");
    TerminalLog("INITIAL_TERMINAL_TEXT_DONE");
    TerminalDrawPopupIndicator(RETROTERM_PORT_ITEM);
    TerminalLog("INITIAL_PORT_INDICATOR_DONE");
    TerminalDrawPopupIndicator(RETROTERM_BAUD_ITEM);
    TerminalLog("INITIAL_BAUD_INDICATOR_DONE");
    TerminalLogMemory("startup-complete");
    TerminalLog("STARTUP_COMPLETE");
    if (gLogError != noErr)
    {
        char status[80];
        sprintf(status, "Debug log unavailable (error %d).", (int)gLogError);
        TerminalSetStatus(status);
    }
    gStartupComplete = true;

    while (running && !gQuitRequested)
    {
        short itemHit = 0;
        DialogPtr selectedDialog = gDialog;

        WaitNextEvent(everyEvent, &event, 1, (RgnHandle)0);
        TerminalPollSerial();

        if (event.what == updateEvt &&
            (WindowPtr)event.message == (WindowPtr)gDialog)
        {
            BeginUpdate((WindowPtr)gDialog);
            DrawDialog(gDialog);
            TerminalDrawPopupIndicator(RETROTERM_PORT_ITEM);
            TerminalDrawPopupIndicator(RETROTERM_BAUD_ITEM);
            TerminalDrawTextView();
            EndUpdate((WindowPtr)gDialog);
            continue;
        }

        if (event.what == mouseDown)
        {
            if (TerminalFocusViewOnMouseDown(&event))
            {
                running = false;
                continue;
            }
            if (TerminalTrackPopupMenu(event.where) ||
                TerminalTrackLocalEchoCheckbox(event.where))
                continue;
        }
        if (event.what == keyDown || event.what == autoKey)
        {
            if (gTerminalFocused)
            {
                unsigned char value = (unsigned char)(event.message & charCodeMask);
                TerminalSendCharacter(value);
                continue;
            }
        }
        if (IsDialogEvent(&event))
        {
            DialogSelect(&event, &selectedDialog, &itemHit);
            if (selectedDialog == gDialog &&
                itemHit > 0)
                TerminalHandleDialogItem(itemHit);
        }
    }

    TerminalLog("APP_EXIT_BEGIN");
    TerminalClosePort();
    TerminalLog("APP_EXIT");
    DisposeDialog(gDialog);
    ExitToShell();
    return 0;
}

/**
 * @brief Satisfy the generated ClassicMacStudio form-runtime callback.
 * @param form Dialog loaded by the generated form runtime, if invoked.
 * @note RetroTerm uses its own @c main event loop rather than that runtime.
 */
void MacStudioFormDidLoad(DialogPtr form)
{
    (void)form;
}
