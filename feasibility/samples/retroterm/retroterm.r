#include "Dialogs.r"

resource 'DLOG' (128) {
    { 18, 16, 308, 478 },
    movableDBoxProc,
    visible,
    goAway,
    0,
    128,
    "RetroTerm 0.2",
    centerMainScreen
};

resource 'DITL' (128) {
    {
        { 26, 12, 44, 450 }, StaticText { enabled, "Port closed. Select settings, then click Open." };
        { 56, 12, 74, 46 }, StaticText { enabled, "Port" };
        { 54, 50, 76, 142 }, Button { enabled, "MODEM" };
        { 56, 158, 74, 196 }, StaticText { enabled, "Baud" };
        { 54, 202, 76, 278 }, Button { enabled, "9600" };
        { 53, 286, 77, 350 }, Button { enabled, "Open" };
        { 56, 360, 74, 460 }, CheckBox { enabled, "Local Echo" };
        { 86, 12, 250, 450 }, StaticText { enabled, "" };
        { 258, 12, 276, 378 }, StaticText { enabled, "Click the terminal area to type. Echo shows sent characters." };
        { 258, 390, 278, 450 }, Button { enabled, "Quit" };
    }
};

resource 'MENU' (259) {
    259,
    textMenuProc,
    0x7FFFFFFF,
    enabled,
    "MODEM",
    {
        "MODEM", noIcon, noKey, noMark, plain;
        "PRINTER", noIcon, noKey, noMark, plain;
    }
};

resource 'MENU' (261) {
    261,
    textMenuProc,
    0x7FFFFFFF,
    enabled,
    "9600",
    {
        "300", noIcon, noKey, noMark, plain;
        "600", noIcon, noKey, noMark, plain;
        "1200", noIcon, noKey, noMark, plain;
        "1800", noIcon, noKey, noMark, plain;
        "2400", noIcon, noKey, noMark, plain;
        "3600", noIcon, noKey, noMark, plain;
        "4800", noIcon, noKey, noMark, plain;
        "7200", noIcon, noKey, noMark, plain;
        "9600", noIcon, noKey, noMark, plain;
        "14400", noIcon, noKey, noMark, plain;
        "19200", noIcon, noKey, noMark, plain;
        "28800", noIcon, noKey, noMark, plain;
        "38400", noIcon, noKey, noMark, plain;
        "57600", noIcon, noKey, noMark, plain;
    }
};

resource 'vers' (1) {
    0x00, 0x01, 0x00, 0x00,
    0,
    "0.2",
    "RetroTerm 0.2"
};

resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    doesActivateOnFGSwitch,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,
    isHighLevelEventAware,
    onlyLocalHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    reserved,
    reserved,
    reserved,
    256 * 1024,
    256 * 1024
};
