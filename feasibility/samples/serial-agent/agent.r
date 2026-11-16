#include "Processes.r"

resource 'vers' (1) {
    0x00, 0x01, 0x80, 0x00,
    0,
    "0.1",
    "MacStudio Agent 0.1"
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
    96 * 1024,
    96 * 1024
};

resource 'DLOG' (128) {
    { 60, 40, 150, 460 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    128,
    "MacStudio Agent 0.1",
    centerMainScreen
};

resource 'DITL' (128) {
    {
        { 56, 170, 76, 250 },
        Button { enabled, "OK" };

        { 20, 20, 40, 400 },
        StaticText { enabled, "A package transfer is active. Try again when it finishes." };
    }
};

resource 'DLOG' (129) {
    { 60, 40, 150, 460 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    129,
    "MacStudio Agent 0.1",
    centerMainScreen
};

resource 'DITL' (129) {
    {
        { 56, 170, 76, 250 },
        Button { enabled, "OK" };

        { 20, 20, 40, 400 },
        StaticText { enabled, "Could not open that setting. The previous setting was restored." };
    }
};

resource 'DLOG' (130) {
    { 60, 40, 150, 460 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    130,
    "MacStudio Agent 0.1",
    centerMainScreen
};

resource 'DITL' (130) {
    {
        { 56, 170, 76, 250 },
        Button { enabled, "OK" };

        { 20, 20, 40, 400 },
        StaticText { enabled, "The serial port could not be reopened. The Agent is stopping." };
    }
};

resource 'DLOG' (131) {
    { 60, 40, 150, 460 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    131,
    "MacStudio Agent 0.1",
    centerMainScreen
};

resource 'DITL' (131) {
    {
        { 56, 170, 76, 250 },
        Button { enabled, "OK" };

        { 20, 20, 40, 400 },
        StaticText { enabled, "The changed setting could not be saved." };
    }
};

resource 'DLOG' (132) {
    { 60, 40, 150, 460 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    132,
    "MacStudio Agent 0.1",
    centerMainScreen
};

resource 'DITL' (132) {
    {
        { 56, 170, 76, 250 },
        Button { enabled, "OK" };

        { 20, 20, 40, 400 },
        StaticText { enabled, "Could not open that port. The previous port was restored." };
    }
};

resource 'DLOG' (133) {
    { 60, 40, 150, 460 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    133,
    "MacStudio Agent 0.1",
    centerMainScreen
};

resource 'DITL' (133) {
    {
        { 56, 170, 76, 250 },
        Button { enabled, "OK" };

        { 20, 20, 40, 400 },
        StaticText { enabled, "Could not restart the serial connection. The Agent is stopping." };
    }
};

resource 'DLOG' (134) {
    { 60, 40, 150, 460 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    134,
    "MacStudio Agent 0.1",
    centerMainScreen
};

resource 'DITL' (134) {
    {
        { 56, 170, 76, 250 },
        Button { enabled, "OK" };

        { 20, 20, 40, 400 },
        StaticText { enabled, "Could not write MacStudio/Agent.log. Check free space and access." };
    }
};

resource 'DLOG' (135) {
    { 60, 40, 150, 460 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    135,
    "MacStudio Agent 0.1",
    centerMainScreen
};

resource 'DITL' (135) {
    {
        { 56, 170, 76, 250 },
        Button { enabled, "OK" };

        { 20, 20, 40, 400 },
        StaticText { enabled, "Port probe results are in MacStudio/Agent.log. No test bytes were sent." };
    }
};

resource 'DLOG' (136) {
    { 60, 40, 150, 460 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    136,
    "MacStudio Agent 0.1",
    centerMainScreen
};

resource 'DITL' (136) {
    {
        { 56, 170, 76, 250 },
        Button { enabled, "OK" };

        { 20, 20, 40, 400 },
        StaticText { enabled, "AgentPort or AgentBaudRate is invalid. See MacStudio/Agent.log." };
    }
};

resource 'DLOG' (137) {
    { 60, 40, 150, 460 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    137,
    "MacStudio Agent 0.1",
    centerMainScreen
};

resource 'DITL' (137) {
    {
        { 56, 170, 76, 250 },
        Button { enabled, "OK" };

        { 20, 20, 40, 400 },
        StaticText { enabled, "Could not access the MacStudio folder on the Agent volume." };
    }
};
