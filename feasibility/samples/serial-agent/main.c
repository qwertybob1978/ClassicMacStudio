#include <string.h>

#include "AppleEvents.h"
#include "Dialogs.h"
#include "Events.h"
#include "Files.h"
#include "Fonts.h"
#include "Menus.h"
#include "OSUtils.h"
#include "Processes.h"
#include "Quickdraw.h"
#include "Serial.h"
#include "TextEdit.h"
#include "Windows.h"

#define AGENT_PROTOCOL_VERSION 1
#define AGENT_FRAME_HEADER_SIZE 26
#define AGENT_FRAME_CRC_SIZE 2
#define AGENT_MAX_PAYLOAD_SIZE 1024
#define AGENT_MAX_FRAME_SIZE \
    (AGENT_FRAME_HEADER_SIZE + AGENT_MAX_PAYLOAD_SIZE + AGENT_FRAME_CRC_SIZE)
#define AGENT_SERIAL_BUFFER_SIZE 4096
#define AGENT_SERIAL_READ_SIZE 256
#define AGENT_MAX_DEBUG_RECORD_SIZE 512
#define AGENT_APPLE_EVENT_PREFIX_SIZE 3
#define AGENT_DEBUG_RECORD_HEADER_SIZE 7
#define AGENT_MACBINARY_HEADER_SIZE 128
#define AGENT_XMODEM_BLOCK_SIZE 128
#define AGENT_XMODEM_PACKET_SIZE 133
#define AGENT_STORAGE_DIRECTORY_NAME_LENGTH 9
#define AGENT_DIAGNOSTIC_LOG_NAME_LENGTH 9
#define AGENT_DIAGNOSTIC_LOG_MAX_SIZE (64UL * 1024UL)
#define AGENT_DIAGNOSTIC_LINE_MAX_SIZE 640
#define AGENT_HOST_DIAGNOSTIC_MAX_SIZE 512
#define AGENT_STORAGE_DIRECTORY_ID 2L
#define AGENT_DIRECTORY_FLAG 0x10
#define AGENT_DEBUG_EVENT_CLASS 'MSAG'
#define AGENT_DEBUG_EVENT_ID 'DBUG'
#define AGENT_LAUNCH_EVENT_ID 'LNCH'
#define AGENT_HOST_DIAGNOSTICS_MESSAGE_TYPE 0x32
#define AGENT_RUN_RESULT_SUCCESS 0
#define AGENT_RUN_RESULT_NOT_INSTALLED 1
#define AGENT_RUN_RESULT_LAUNCH_FAILED 2
#define AGENT_RUN_RESULT_BUSY 3
#define AGENT_STOP_RESULT_SUCCESS 0
#define AGENT_STOP_RESULT_NOT_RUNNING 1
#define AGENT_STOP_RESULT_WRONG_BUILD 2
#define AGENT_STOP_RESULT_FAILED 3
#define AGENT_BAUD_RATE_FLAG_9600 (1UL << 0)
#define AGENT_BAUD_RATE_FLAG_19200 (1UL << 1)
#define AGENT_BAUD_RATE_FLAG_38400 (1UL << 2)
#define AGENT_BAUD_RATE_FLAG_57600 (1UL << 3)
#define AGENT_BAUD_RATE_FLAG_300 (1UL << 4)
#define AGENT_BAUD_RATE_FLAG_600 (1UL << 5)
#define AGENT_BAUD_RATE_FLAG_1200 (1UL << 6)
#define AGENT_BAUD_RATE_FLAG_1800 (1UL << 7)
#define AGENT_BAUD_RATE_FLAG_2400 (1UL << 8)
#define AGENT_BAUD_RATE_FLAG_3600 (1UL << 9)
#define AGENT_BAUD_RATE_FLAG_4800 (1UL << 10)
#define AGENT_BAUD_RATE_FLAG_7200 (1UL << 11)
#define AGENT_BAUD_RATE_FLAG_14400 (1UL << 12)
#define AGENT_BAUD_RATE_FLAG_28800 (1UL << 13)
#define AGENT_BAUD_RATE_MENU_ID 128
#define AGENT_PORT_MENU_ID 129
#define AGENT_AGENT_MENU_ID 130
#define AGENT_PORT_COUNT 2
#define AGENT_BAUD_RATE_COUNT 14
#define AGENT_SERIAL_BUSY_DIALOG_ID 128
#define AGENT_SERIAL_RESTORED_DIALOG_ID 129
#define AGENT_SERIAL_FATAL_DIALOG_ID 130
#define AGENT_SERIAL_SAVE_DIALOG_ID 131
#define AGENT_PORT_RESTORED_DIALOG_ID 132
#define AGENT_RESTART_FATAL_DIALOG_ID 133
#define AGENT_DIAGNOSTIC_LOG_FAILURE_DIALOG_ID 134
#define AGENT_PORT_DIAGNOSTICS_DIALOG_ID 135
#define AGENT_CONFIG_ERROR_DIALOG_ID 136
#define AGENT_STORAGE_ERROR_DIALOG_ID 137
#define AGENT_SUPPORTED_BAUD_RATE_FLAGS \
    (AGENT_BAUD_RATE_FLAG_9600 | AGENT_BAUD_RATE_FLAG_19200 | \
     AGENT_BAUD_RATE_FLAG_38400 | AGENT_BAUD_RATE_FLAG_57600 | \
     AGENT_BAUD_RATE_FLAG_300 | AGENT_BAUD_RATE_FLAG_600 | \
     AGENT_BAUD_RATE_FLAG_1200 | AGENT_BAUD_RATE_FLAG_1800 | \
     AGENT_BAUD_RATE_FLAG_2400 | AGENT_BAUD_RATE_FLAG_3600 | \
     AGENT_BAUD_RATE_FLAG_4800 | AGENT_BAUD_RATE_FLAG_7200 | \
     AGENT_BAUD_RATE_FLAG_14400 | AGENT_BAUD_RATE_FLAG_28800)

static const unsigned char gModemInputDriverName[] = { 4, '.', 'A', 'I', 'n' };
static const unsigned char gModemOutputDriverName[] = { 5, '.', 'A', 'O', 'u', 't' };
static const unsigned char gPrinterInputDriverName[] = { 4, '.', 'B', 'I', 'n' };
static const unsigned char gPrinterOutputDriverName[] = { 5, '.', 'B', 'O', 'u', 't' };
static const unsigned char gStorageDirectoryName[] = {
    AGENT_STORAGE_DIRECTORY_NAME_LENGTH,
    'M', 'a', 'c', 'S', 't', 'u', 'd', 'i', 'o'
};
static const unsigned char gDiagnosticLogName[] = {
    AGENT_DIAGNOSTIC_LOG_NAME_LENGTH,
    'A', 'g', 'e', 'n', 't', '.', 'l', 'o', 'g'
};
static const unsigned char gSerialPortConfigName[] = {
    9, 'A', 'g', 'e', 'n', 't', 'P', 'o', 'r', 't'
};
static const unsigned char gSerialBaudConfigName[] = {
    13, 'A', 'g', 'e', 'n', 't', 'B', 'a', 'u', 'd', 'R', 'a', 't', 'e'
};
static const unsigned char gModemPortConfigValue[] = {
    5, 'M', 'O', 'D', 'E', 'M'
};
static const unsigned char gPrinterPortConfigValue[] = {
    7, 'P', 'R', 'I', 'N', 'T', 'E', 'R'
};
static const unsigned char gBaudRateMenuTitle[] = {
    9, 'B', 'a', 'u', 'd', ' ', 'R', 'a', 't', 'e'
};
static const unsigned char gPortMenuTitle[] = { 4, 'P', 'o', 'r', 't' };
static const unsigned char gPortMenuItems[AGENT_PORT_COUNT][19] = {
    { 16, 'M', 'o', 'd', 'e', 'm', ' ', '(', 'S', 'e', 'r', 'i', 'a', 'l', ' ', 'A', ')' },
    { 18, 'P', 'r', 'i', 'n', 't', 'e', 'r', ' ', '(', 'S', 'e', 'r', 'i', 'a', 'l', ' ', 'B', ')' }
};
static const unsigned char gAgentMenuTitle[] = {
    5, 'A', 'g', 'e', 'n', 't'
};
static const unsigned char gAgentMenuItems[3][15] = {
    { 13, 'R', 'e', 's', 't', 'a', 'r', 't', ' ', 'A', 'g', 'e', 'n', 't' },
    { 14, 'D', 'i', 'a', 'g', 'n', 'o', 's', 'e', ' ', 'P', 'o', 'r', 't', 's' },
    { 4, 'E', 'x', 'i', 't' }
};
static const unsigned char gBaudRateMenuItems[AGENT_BAUD_RATE_COUNT][6] = {
    { 3, '3', '0', '0' },
    { 3, '6', '0', '0' },
    { 4, '1', '2', '0', '0' },
    { 4, '1', '8', '0', '0' },
    { 4, '2', '4', '0', '0' },
    { 4, '3', '6', '0', '0' },
    { 4, '4', '8', '0', '0' },
    { 4, '7', '2', '0', '0' },
    { 4, '9', '6', '0', '0' },
    { 5, '1', '4', '4', '0', '0' },
    { 5, '1', '9', '2', '0', '0' },
    { 5, '2', '8', '8', '0', '0' },
    { 5, '3', '8', '4', '0', '0' },
    { 5, '5', '7', '6', '0', '0' }
};
static const short gSupportedBaudRates[AGENT_BAUD_RATE_COUNT] = {
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
static const unsigned char gIncomingPackageName[] = {
    13, '.', 'M', 'S', 'A', 'G', 'T', 'r', 'a', 'n', 's', 'f', 'e', 'r'
};
static const unsigned char gStagedApplicationName[] = {
    8, '.', 'M', 'S', 'A', 'G', 'N', 'e', 'w'
};
static const unsigned char gPreviousApplicationName[] = {
    13, '.', 'M', 'S', 'A', 'G', 'P', 'r', 'e', 'v', 'i', 'o', 'u', 's'
};
static unsigned char gSerialInputBuffer[AGENT_SERIAL_BUFFER_SIZE];
static unsigned char gFrameBuffer[AGENT_MAX_FRAME_SIZE];
static unsigned char gOutputFrame[AGENT_MAX_FRAME_SIZE];
static unsigned char gXmodemPacket[AGENT_XMODEM_PACKET_SIZE];
static unsigned short gFrameLength;
static unsigned short gXmodemPacketLength;
static short gSerialInputRef;
static short gSerialOutputRef;
static short gIncomingFileRef;
static short gVolumeRefNum;
static long gStorageDirectoryId;
static unsigned char gSessionId[16];
static Boolean gSessionActive;
static Boolean gTransferActive;
static Boolean gIncomingFileOpen;
static Boolean gHasLastXmodemBlock;
static unsigned char gTransferBuildId[16];
static unsigned char gInstalledBuildId[16];
static unsigned char gRunningBuildId[16];
static unsigned char gDestinationName[32];
static unsigned long gPackageLength;
static unsigned long gExpectedPackageCrc32;
static unsigned long gReceivedPackageLength;
static unsigned long gRunningPackageCrc32;
static unsigned char gExpectedXmodemBlock;
static unsigned char gLastXmodemBlock;
static unsigned char gCancelByteCount;
static FSSpec gIncomingPackageSpec;
static FSSpec gInstalledApplicationSpec;
static ProcessSerialNumber gButProcess;
static Boolean gHasInstalledBuild;
static Boolean gButRunning;
static unsigned char gDebugRecord[AGENT_MAX_DEBUG_RECORD_SIZE];
static unsigned short gDebugRecordLength;
static unsigned char gDebugSeverity;
static unsigned char gDebugRecordPending;
static unsigned char gDebugRecordSent;
static unsigned long gDebugSequence;
static unsigned long gNextDebugSequence = 1;
static AEEventHandlerUPP gDebugHandler;
static AEEventHandlerUPP gLaunchHandler;
static MenuHandle gBaudRateMenu;
static MenuHandle gPortMenu;
static MenuHandle gAgentMenu;
static Boolean gShouldExit;
static Boolean gDiagnosticLogFailureReported;
static unsigned long gLastDiagnosticSampleTick;
static unsigned long gSerialBytesReceived;
static unsigned long gSerialBytesSent;
static unsigned long gValidFramesReceived;
static unsigned long gInvalidFrameBytes;
static unsigned long gInvalidFrameCrcs;

static void AgentLogEvent(const char *eventName, long value);
static void AgentLogMessage(const char *messageText);
static OSErr AgentLogHostDiagnostics(
    const unsigned char *payload,
    unsigned short payloadLength);

static unsigned short AgentReadUInt16(const unsigned char *value)
{
    return (unsigned short)(((unsigned short)value[0] << 8) | value[1]);
}

static unsigned long AgentReadUInt32(const unsigned char *value)
{
    return ((unsigned long)value[0] << 24) |
        ((unsigned long)value[1] << 16) |
        ((unsigned long)value[2] << 8) |
        (unsigned long)value[3];
}

static void AgentWriteUInt16(unsigned char *destination, unsigned short value)
{
    destination[0] = (unsigned char)(value >> 8);
    destination[1] = (unsigned char)value;
}

static void AgentWriteUInt32(unsigned char *destination, unsigned long value)
{
    destination[0] = (unsigned char)(value >> 24);
    destination[1] = (unsigned char)(value >> 16);
    destination[2] = (unsigned char)(value >> 8);
    destination[3] = (unsigned char)value;
}

static unsigned short AgentCrc16(const unsigned char *data, unsigned short length)
{
    unsigned short crc = 0xFFFF;
    unsigned short index;

    for (index = 0; index < length; ++index)
    {
        unsigned char bit;
        crc ^= (unsigned short)((unsigned short)data[index] << 8);
        for (bit = 0; bit < 8; ++bit)
        {
            if ((crc & 0x8000) != 0)
                crc = (unsigned short)((crc << 1) ^ 0x1021);
            else
                crc = (unsigned short)(crc << 1);
        }
    }

    return crc;
}

static OSErr AgentWriteFrame(
    unsigned char messageType,
    const unsigned char *sessionId,
    const unsigned char *payload,
    unsigned short payloadLength)
{
    unsigned short frameLength;
    unsigned short crc;
    LONGINT writeLength;
    OSErr error;

    if (payloadLength > AGENT_MAX_PAYLOAD_SIZE ||
        sessionId == NULL ||
        (payload == NULL && payloadLength != 0))
        return paramErr;

    gOutputFrame[0] = 'M';
    gOutputFrame[1] = 'S';
    gOutputFrame[2] = 'A';
    gOutputFrame[3] = 'G';
    gOutputFrame[4] = AGENT_PROTOCOL_VERSION;
    gOutputFrame[5] = messageType;
    gOutputFrame[6] = 0;
    gOutputFrame[7] = 0;
    memcpy(gOutputFrame + 8, sessionId, 16);
    AgentWriteUInt16(gOutputFrame + 24, payloadLength);
    if (payloadLength != 0)
        memcpy(gOutputFrame + AGENT_FRAME_HEADER_SIZE, payload, payloadLength);

    frameLength = (unsigned short)(AGENT_FRAME_HEADER_SIZE + payloadLength);
    crc = AgentCrc16(gOutputFrame, frameLength);
    AgentWriteUInt16(gOutputFrame + frameLength, crc);
    frameLength = (unsigned short)(frameLength + AGENT_FRAME_CRC_SIZE);

    writeLength = frameLength;
    error = FSWrite(gSerialOutputRef, &writeLength, gOutputFrame);
    if (error == noErr && writeLength != frameLength)
        error = ioErr;
    if (error != noErr)
    {
        AgentLogEvent("SERIAL_FRAME_WRITE_ERROR", error);
        return error;
    }
    gSerialBytesSent += frameLength;
    return error;
}

static OSErr AgentWriteRaw(const unsigned char *data, unsigned short length)
{
    LONGINT writeLength = length;
    OSErr error;

    if (data == NULL && length != 0)
        return paramErr;
    error = FSWrite(gSerialOutputRef, &writeLength, (Ptr)data);
    if (error == noErr && writeLength != length)
        error = ioErr;
    if (error != noErr)
    {
        AgentLogEvent("SERIAL_RAW_WRITE_ERROR", error);
        return error;
    }
    gSerialBytesSent += length;
    return error;
}

static void AgentUpdateCrc32(
    unsigned long *crc,
    const unsigned char *data,
    unsigned short length)
{
    unsigned short index;

    for (index = 0; index < length; ++index)
    {
        unsigned char bit;
        *crc ^= data[index];
        for (bit = 0; bit < 8; ++bit)
        {
            if ((*crc & 1) != 0)
                *crc = (*crc >> 1) ^ 0xEDB88320UL;
            else
                *crc >>= 1;
        }
    }
}

static unsigned long AgentGetCrc32(void)
{
    return ~gRunningPackageCrc32;
}

static OSErr AgentInitializeStorage(void)
{
    ProcessSerialNumber processNumber;
    ProcessInfoRec processInfo;
    FSSpec applicationSpec;
    FSSpec directorySpec;
    CInfoPBRec catalogInfo;
    OSErr error;

    error = GetCurrentProcess(&processNumber);
    if (error != noErr)
        return error;
    memset(&processInfo, 0, sizeof(processInfo));
    memset(&applicationSpec, 0, sizeof(applicationSpec));
    processInfo.processInfoLength = sizeof(processInfo);
    processInfo.processAppSpec = &applicationSpec;
    error = GetProcessInformation(&processNumber, &processInfo);
    if (error != noErr)
        return error;
    if (applicationSpec.vRefNum == 0)
        return fnfErr;

    gVolumeRefNum = applicationSpec.vRefNum;
    error = FSMakeFSSpec(
        gVolumeRefNum,
        AGENT_STORAGE_DIRECTORY_ID,
        (ConstStringPtr)gStorageDirectoryName,
        &directorySpec);
    if (error == fnfErr)
    {
        long createdDirectoryId = 0;
        error = FSpDirCreate(
            &directorySpec,
            smSystemScript,
            &createdDirectoryId);
        if (error != noErr)
            return error;
    }
    else if (error != noErr)
    {
        return error;
    }

    memset(&catalogInfo, 0, sizeof(catalogInfo));
    catalogInfo.dirInfo.ioNamePtr = (StringPtr)gStorageDirectoryName;
    catalogInfo.dirInfo.ioVRefNum = gVolumeRefNum;
    catalogInfo.dirInfo.ioFDirIndex = 0;
    catalogInfo.dirInfo.ioDrDirID = AGENT_STORAGE_DIRECTORY_ID;
    error = PBGetCatInfoSync(&catalogInfo);
    if (error != noErr)
        return error;
    if ((catalogInfo.dirInfo.ioFlAttrib & AGENT_DIRECTORY_FLAG) == 0 ||
        catalogInfo.dirInfo.ioDrDirID <= 0)
        return dupFNErr;

    gStorageDirectoryId = catalogInfo.dirInfo.ioDrDirID;
    return noErr;
}

static OSErr AgentGetFreeBytes(unsigned long *freeBytes)
{
    HParamBlockRec volumeInfo;
    unsigned long long bytes;
    OSErr error;

    if (freeBytes == NULL)
        return paramErr;
    memset(&volumeInfo, 0, sizeof(volumeInfo));
    volumeInfo.volumeParam.ioVRefNum = gVolumeRefNum;
    volumeInfo.volumeParam.ioVolIndex = 0;
    error = PBHGetVInfoSync((HParmBlkPtr)&volumeInfo);
    if (error != noErr)
        return error;

    bytes = (unsigned long long)volumeInfo.volumeParam.ioVFrBlk *
        (unsigned long long)volumeInfo.volumeParam.ioVAlBlkSiz;
    *freeBytes = bytes > 0xFFFFFFFFULL
        ? 0xFFFFFFFFUL
        : (unsigned long)bytes;
    return noErr;
}

static OSErr AgentDeleteStorageFile(const unsigned char *name)
{
    FSSpec fileSpec;
    OSErr error;

    error = FSMakeFSSpec(
        gVolumeRefNum,
        gStorageDirectoryId,
        (ConstStringPtr)name,
        &fileSpec);
    if (error == fnfErr)
        return noErr;
    if (error != noErr)
        return error;
    return FSpDelete(&fileSpec);
}

static Boolean AgentIsAsciiLetter(unsigned char value)
{
    return (Boolean)((value >= 'A' && value <= 'Z') ||
        (value >= 'a' && value <= 'z'));
}

static Boolean AgentIsValidDestinationName(
    const unsigned char *name,
    unsigned short length)
{
    unsigned short index;

    if (name == NULL || length == 0 || length > 31 ||
        !AgentIsAsciiLetter(name[0]))
        return false;
    for (index = 0; index < length; ++index)
    {
        unsigned char value = name[index];
        if (!AgentIsAsciiLetter(value) &&
            (value < '0' || value > '9') &&
            value != '_')
            return false;
    }
    return true;
}

static OSErr AgentSendReady(
    const unsigned char *sessionId,
    unsigned char resultCode)
{
    return AgentWriteFrame(0x11, sessionId, &resultCode, 1);
}

static OSErr AgentRefreshButState(Boolean *isRunning)
{
    ProcessInfoRec processInfo;
    OSErr error;

    if (isRunning == NULL)
        return paramErr;
    *isRunning = false;
    if (!gButRunning)
        return noErr;

    memset(&processInfo, 0, sizeof(processInfo));
    processInfo.processInfoLength = sizeof(processInfo);
    error = GetProcessInformation(&gButProcess, &processInfo);
    if (error == procNotFound)
    {
        gButRunning = false;
        return noErr;
    }
    if (error != noErr)
        return error;

    *isRunning = true;
    return noErr;
}

static OSErr AgentBeginPut(
    const unsigned char *sessionId,
    const unsigned char *payload,
    unsigned short payloadLength)
{
    unsigned char nameLength;
    unsigned char resultCode = 1;
    unsigned long freeBytes = 0;
    Boolean isButRunning = false;
    OSErr error;

    if (payloadLength < 27)
    {
        AgentLogEvent("PUT_REJECTED_SHORT_BEGIN_PAYLOAD", payloadLength);
        return AgentSendReady(sessionId, resultCode);
    }

    nameLength = payload[25];
    if (payloadLength != (unsigned short)(26 + nameLength) ||
        !AgentIsValidDestinationName(payload + 26, nameLength) ||
        AgentReadUInt32(payload + 16) == 0 ||
        AgentReadUInt32(payload + 16) > 0x7FFFFFFFUL ||
        payload[24] != 1)
    {
        AgentLogMessage("PUT_REJECTED_INVALID_BEGIN_METADATA");
        return AgentSendReady(sessionId, resultCode);
    }
    if (!gSessionActive ||
        memcmp(sessionId, gSessionId, sizeof(gSessionId)) != 0)
    {
        AgentLogMessage("PUT_REJECTED_SESSION_MISMATCH");
        return AgentSendReady(sessionId, 5);
    }
    error = AgentRefreshButState(&isButRunning);
    if (error != noErr)
    {
        AgentLogEvent("PUT_PROCESS_STATE_QUERY_ERROR", error);
        return AgentSendReady(sessionId, 4);
    }
    if (gTransferActive || isButRunning)
    {
        AgentLogMessage("PUT_REJECTED_AGENT_OR_APPLICATION_BUSY");
        return AgentSendReady(sessionId, 5);
    }

    error = AgentGetFreeBytes(&freeBytes);
    if (error != noErr)
    {
        AgentLogEvent("PUT_VOLUME_SPACE_QUERY_ERROR", error);
        return error;
    }
    if (AgentReadUInt32(payload + 16) > freeBytes)
    {
        AgentLogEvent("PUT_REJECTED_INSUFFICIENT_FREE_BYTES", (long)freeBytes);
        return AgentSendReady(sessionId, 2);
    }

    error = AgentDeleteStorageFile(gIncomingPackageName);
    if (error != noErr)
    {
        AgentLogEvent("PUT_TEMP_FILE_DELETE_ERROR", error);
        return error;
    }
    error = FSMakeFSSpec(
        gVolumeRefNum,
        gStorageDirectoryId,
        (ConstStringPtr)gIncomingPackageName,
        &gIncomingPackageSpec);
    if (error != noErr && error != fnfErr)
    {
        AgentLogEvent("PUT_TEMP_FILE_SPEC_ERROR", error);
        return error;
    }
    error = FSpCreate(&gIncomingPackageSpec, 'MSTA', 'TEMP', smSystemScript);
    if (error != noErr)
    {
        AgentLogEvent("PUT_TEMP_FILE_CREATE_ERROR", error);
        return AgentSendReady(sessionId, error == dskFulErr ? 2 : 4);
    }
    error = FSpOpenDF(&gIncomingPackageSpec, fsWrPerm, &gIncomingFileRef);
    if (error != noErr)
    {
        AgentLogEvent("PUT_TEMP_FILE_OPEN_ERROR", error);
        FSpDelete(&gIncomingPackageSpec);
        return AgentSendReady(sessionId, error == dskFulErr ? 2 : 4);
    }

    memcpy(gTransferBuildId, payload, sizeof(gTransferBuildId));
    memcpy(gDestinationName + 1, payload + 26, nameLength);
    gDestinationName[0] = nameLength;
    gPackageLength = AgentReadUInt32(payload + 16);
    gExpectedPackageCrc32 = AgentReadUInt32(payload + 20);
    gReceivedPackageLength = 0;
    gRunningPackageCrc32 = 0xFFFFFFFFUL;
    gExpectedXmodemBlock = 1;
    gLastXmodemBlock = 0;
    gHasLastXmodemBlock = false;
    gXmodemPacketLength = 0;
    gCancelByteCount = 0;
    gIncomingFileOpen = true;
    gTransferActive = true;
    AgentLogEvent("PUT_PACKAGE_SIZE_BYTES", (long)gPackageLength);
    AgentLogEvent("PUT_PACKAGE_EXPECTED_CRC32", (long)gExpectedPackageCrc32);
    AgentLogEvent("PUT_VOLUME_FREE_BYTES", (long)freeBytes);

    error = AgentSendReady(sessionId, 0);
    AgentLogEvent("PUT_READY_SEND_ERROR", error);
    if (error == noErr)
    {
        const unsigned char crcRequest = 'C';
        error = AgentWriteRaw(&crcRequest, 1);
    }
    if (error != noErr)
    {
        AgentLogEvent("PUT_INITIAL_XMODEM_REQUEST_ERROR", error);
        if (gIncomingFileOpen)
        {
            FSClose(gIncomingFileRef);
            gIncomingFileOpen = false;
        }
        FSpDelete(&gIncomingPackageSpec);
        gTransferActive = false;
        return error;
    }
    return noErr;
}

static unsigned short AgentXmodemCrc16(const unsigned char *data)
{
    unsigned short crc = 0;
    unsigned short index;

    for (index = 0; index < AGENT_XMODEM_BLOCK_SIZE; ++index)
    {
        unsigned char bit;
        crc ^= (unsigned short)((unsigned short)data[index] << 8);
        for (bit = 0; bit < 8; ++bit)
        {
            if ((crc & 0x8000) != 0)
                crc = (unsigned short)((crc << 1) ^ 0x1021);
            else
                crc = (unsigned short)(crc << 1);
        }
    }
    return crc;
}

static unsigned short AgentMacBinaryHeaderCrc16(const unsigned char *data)
{
    unsigned short crc = 0;
    unsigned short index;

    for (index = 0; index < 124; ++index)
    {
        unsigned char bit;
        crc ^= (unsigned short)((unsigned short)data[index] << 8);
        for (bit = 0; bit < 8; ++bit)
        {
            if ((crc & 0x8000) != 0)
                crc = (unsigned short)((crc << 1) ^ 0x1021);
            else
                crc = (unsigned short)(crc << 1);
        }
    }
    return crc;
}

static Boolean AgentMacBinaryHeaderIsValid(
    const unsigned char *header,
    unsigned long packageLength)
{
    unsigned char nameLength;
    unsigned long dataLength;
    unsigned long resourceLength;
    unsigned long long paddedDataLength;
    unsigned long long paddedResourceLength;
    unsigned long long expectedLength;
    unsigned short index;

    nameLength = header[1];
    if (header[0] != 0 || nameLength == 0 || nameLength > 63 ||
        memcmp(header + 65, "APPL", 4) != 0 ||
        AgentReadUInt16(header + 124) != AgentMacBinaryHeaderCrc16(header))
        return false;
    for (index = 0; index < nameLength; ++index)
    {
        if (header[2 + index] == 0)
            return false;
    }

    dataLength = AgentReadUInt32(header + 83);
    resourceLength = AgentReadUInt32(header + 87);
    if (dataLength == 0 && resourceLength == 0)
        return false;

    paddedDataLength =
        (((unsigned long long)dataLength + 127ULL) / 128ULL) * 128ULL;
    paddedResourceLength =
        (((unsigned long long)resourceLength + 127ULL) / 128ULL) * 128ULL;
    expectedLength =
        AGENT_MACBINARY_HEADER_SIZE + paddedDataLength + paddedResourceLength;
    return (Boolean)(expectedLength == packageLength);
}

static OSErr AgentReadFileBytes(
    short fileRef,
    unsigned char *data,
    unsigned long length)
{
    while (length != 0)
    {
        LONGINT requestLength = length > 512 ? 512 : (LONGINT)length;
        LONGINT readLength = requestLength;
        OSErr error = FSRead(fileRef, &readLength, data);
        if (error != noErr)
            return error;
        if (readLength != requestLength)
            return eofErr;
        data += readLength;
        length -= (unsigned long)readLength;
    }
    return noErr;
}

static OSErr AgentWriteFileBytes(
    short fileRef,
    const unsigned char *data,
    unsigned long length)
{
    while (length != 0)
    {
        LONGINT requestLength = length > 512 ? 512 : (LONGINT)length;
        LONGINT writeLength = requestLength;
        OSErr error = FSWrite(fileRef, &writeLength, (Ptr)data);
        if (error != noErr)
            return error;
        if (writeLength != requestLength)
            return ioErr;
        data += writeLength;
        length -= (unsigned long)writeLength;
    }
    return noErr;
}

static OSErr AgentCopyFileBytes(
    short sourceRef,
    short destinationRef,
    unsigned long length)
{
    unsigned char buffer[512];

    while (length != 0)
    {
        unsigned long chunkLength = length > sizeof(buffer)
            ? sizeof(buffer)
            : length;
        OSErr error = AgentReadFileBytes(sourceRef, buffer, chunkLength);
        if (error != noErr)
            return error;
        error = AgentWriteFileBytes(destinationRef, buffer, chunkLength);
        if (error != noErr)
            return error;
        length -= chunkLength;
    }
    return noErr;
}

static OSErr AgentInstallMacBinary(void)
{
    unsigned char header[AGENT_MACBINARY_HEADER_SIZE];
    FSSpec stagedApplicationSpec;
    FSSpec destinationSpec;
    short packageRef = 0;
    short dataForkRef = 0;
    short resourceForkRef = 0;
    unsigned long dataLength;
    unsigned long resourceLength;
    unsigned long long resourceOffset;
    FInfo finderInfo;
    OSErr error;
    OSErr closeError;
    Boolean stagedApplicationCreated = false;
    Boolean installed = false;
    Boolean preserveStagedFile = false;

    error = FSpOpenDF(&gIncomingPackageSpec, fsRdPerm, &packageRef);
    if (error != noErr)
        return error;
    error = AgentReadFileBytes(packageRef, header, sizeof(header));
    if (error != noErr)
        goto cleanup;
    if (!AgentMacBinaryHeaderIsValid(header, gPackageLength))
    {
        error = paramErr;
        goto cleanup;
    }

    error = AgentDeleteStorageFile(gStagedApplicationName);
    if (error != noErr)
        goto cleanup;
    error = FSMakeFSSpec(
        gVolumeRefNum,
        gStorageDirectoryId,
        (ConstStringPtr)gStagedApplicationName,
        &stagedApplicationSpec);
    if (error != noErr && error != fnfErr)
        goto cleanup;
    error = FSpCreate(
        &stagedApplicationSpec,
        AgentReadUInt32(header + 69),
        AgentReadUInt32(header + 65),
        smSystemScript);
    if (error != noErr)
        goto cleanup;
    stagedApplicationCreated = true;

    error = FSpOpenDF(&stagedApplicationSpec, fsWrPerm, &dataForkRef);
    if (error != noErr)
        goto cleanup;
    dataLength = AgentReadUInt32(header + 83);
    error = AgentCopyFileBytes(packageRef, dataForkRef, dataLength);
    closeError = FSClose(dataForkRef);
    dataForkRef = 0;
    if (error == noErr)
        error = closeError;
    if (error != noErr)
        goto cleanup;

    resourceLength = AgentReadUInt32(header + 87);
    resourceOffset =
        AGENT_MACBINARY_HEADER_SIZE +
        (((unsigned long long)dataLength + 127ULL) / 128ULL) * 128ULL;
    error = SetFPos(packageRef, fsFromStart, (LONGINT)resourceOffset);
    if (error != noErr)
        goto cleanup;
    error = FSpOpenRF(&stagedApplicationSpec, fsWrPerm, &resourceForkRef);
    if (error != noErr)
        goto cleanup;
    error = AgentCopyFileBytes(packageRef, resourceForkRef, resourceLength);
    closeError = FSClose(resourceForkRef);
    resourceForkRef = 0;
    if (error == noErr)
        error = closeError;
    if (error != noErr)
        goto cleanup;

    memset(&finderInfo, 0, sizeof(finderInfo));
    finderInfo.fdType = AgentReadUInt32(header + 65);
    finderInfo.fdCreator = AgentReadUInt32(header + 69);
    finderInfo.fdFlags = AgentReadUInt16(header + 73);
    finderInfo.fdLocation.v = (short)AgentReadUInt16(header + 75);
    finderInfo.fdLocation.h = (short)AgentReadUInt16(header + 77);
    error = FSpSetFInfo(&stagedApplicationSpec, &finderInfo);
    if (error != noErr)
        goto cleanup;

    error = FSMakeFSSpec(
        gVolumeRefNum,
        gStorageDirectoryId,
        (ConstStringPtr)gDestinationName,
        &destinationSpec);
    if (error == fnfErr)
    {
        error = FSpRename(
            &stagedApplicationSpec,
            (ConstStringPtr)gDestinationName);
        if (error == noErr)
            installed = true;
        goto cleanup;
    }
    if (error != noErr)
        goto cleanup;

    error = AgentDeleteStorageFile(gPreviousApplicationName);
    if (error != noErr)
        goto cleanup;
    error = FSpExchangeFiles(&stagedApplicationSpec, &destinationSpec);
    if (error != noErr)
        goto cleanup;
    error = FSpRename(
        &stagedApplicationSpec,
        (ConstStringPtr)gPreviousApplicationName);
    if (error == noErr)
    {
        installed = true;
        goto cleanup;
    }
    closeError = FSpExchangeFiles(&stagedApplicationSpec, &destinationSpec);
    if (closeError != noErr)
    {
        preserveStagedFile = true;
        error = closeError;
    }

cleanup:
    if (resourceForkRef != 0)
    {
        closeError = FSClose(resourceForkRef);
        if (error == noErr)
            error = closeError;
    }
    if (dataForkRef != 0)
    {
        closeError = FSClose(dataForkRef);
        if (error == noErr)
            error = closeError;
    }
    if (packageRef != 0)
    {
        closeError = FSClose(packageRef);
        if (error == noErr)
            error = closeError;
    }
    if (!installed && stagedApplicationCreated && !preserveStagedFile)
    {
        closeError = FSpDelete(&stagedApplicationSpec);
        if (error == noErr)
            error = closeError;
    }
    return error;
}

static OSErr AgentCleanupIncomingPackage(void)
{
    OSErr error = noErr;
    OSErr cleanupError;

    if (gIncomingFileOpen)
    {
        error = FSClose(gIncomingFileRef);
        gIncomingFileOpen = false;
    }
    cleanupError = AgentDeleteStorageFile(gIncomingPackageName);
    if (error == noErr)
        error = cleanupError;
    gTransferActive = false;
    gXmodemPacketLength = 0;
    gCancelByteCount = 0;
    return error;
}

static OSErr AgentSendPutComplete(
    const unsigned char *sessionId,
    unsigned char resultCode,
    unsigned long verifiedCrc32)
{
    unsigned char payload[21];

    memcpy(payload, gTransferBuildId, sizeof(gTransferBuildId));
    payload[16] = resultCode;
    AgentWriteUInt32(payload + 17, verifiedCrc32);
    return AgentWriteFrame(0x12, sessionId, payload, sizeof(payload));
}

static OSErr AgentFinishTransfer(void)
{
    const unsigned char acknowledge = 0x06;
    unsigned char resultCode = 0;
    unsigned long verifiedCrc32 = AgentGetCrc32();
    OSErr error;
    OSErr cleanupError;

    if (gIncomingFileOpen)
    {
        error = FSClose(gIncomingFileRef);
        gIncomingFileOpen = false;
        if (error != noErr)
            resultCode = 4;
    }
    if (resultCode == 0 && gReceivedPackageLength != gPackageLength)
        resultCode = 1;
    if (resultCode == 0 && verifiedCrc32 != gExpectedPackageCrc32)
        resultCode = 2;
    if (resultCode == 0)
    {
        error = AgentInstallMacBinary();
        if (error == paramErr)
            resultCode = 3;
        else if (error != noErr)
            resultCode = 4;
    }

    cleanupError = AgentCleanupIncomingPackage();
    if (cleanupError != noErr && resultCode == 0)
        resultCode = 4;
    if (resultCode == 0)
    {
        error = FSMakeFSSpec(
            gVolumeRefNum,
            gStorageDirectoryId,
            (ConstStringPtr)gDestinationName,
            &gInstalledApplicationSpec);
        if (error != noErr)
            resultCode = 4;
        else
        {
            memcpy(gInstalledBuildId, gTransferBuildId, sizeof(gInstalledBuildId));
            gHasInstalledBuild = true;
        }
    }
    AgentLogEvent("PUT_TRANSFER_RESULT_CODE", resultCode);
    AgentLogEvent("PUT_TRANSFER_RECEIVED_BYTES", (long)gReceivedPackageLength);
    AgentLogEvent("PUT_TRANSFER_VERIFIED_CRC32", (long)verifiedCrc32);
    error = AgentWriteRaw(&acknowledge, 1);
    if (error != noErr)
        return error;
    return AgentSendPutComplete(gSessionId, resultCode, verifiedCrc32);
}

static OSErr AgentCancelTransfer(void)
{
    const unsigned char cancel[2] = { 0x18, 0x18 };
    OSErr error = AgentWriteRaw(cancel, sizeof(cancel));
    OSErr cleanupError = AgentCleanupIncomingPackage();
    AgentLogEvent("PUT_TRANSFER_CANCELLED", error);
    AgentLogEvent("PUT_CANCEL_CLEANUP_ERROR", cleanupError);
    return error != noErr ? error : cleanupError;
}

static OSErr AgentProcessXmodemPacket(void)
{
    unsigned char blockNumber = gXmodemPacket[1];
    unsigned long remaining;
    unsigned short dataLength;
    unsigned short paddingIndex;
    unsigned short expectedCrc;
    unsigned short actualCrc;
    unsigned char response;
    OSErr error;

    expectedCrc = AgentReadUInt16(gXmodemPacket + 131);
    actualCrc = AgentXmodemCrc16(gXmodemPacket + 3);
    if (gXmodemPacket[2] != (unsigned char)(blockNumber ^ 0xFF) ||
        expectedCrc != actualCrc)
    {
        AgentLogEvent("XMODEM_BLOCK_HEADER_OR_CRC_ERROR", blockNumber);
        response = 0x15;
        return AgentWriteRaw(&response, 1);
    }

    if (gHasLastXmodemBlock && blockNumber == gLastXmodemBlock)
    {
        response = 0x06;
        return AgentWriteRaw(&response, 1);
    }
    if (blockNumber != gExpectedXmodemBlock ||
        gReceivedPackageLength >= gPackageLength)
    {
        response = 0x15;
        return AgentWriteRaw(&response, 1);
    }

    remaining = gPackageLength - gReceivedPackageLength;
    dataLength = remaining > AGENT_XMODEM_BLOCK_SIZE
        ? AGENT_XMODEM_BLOCK_SIZE
        : (unsigned short)remaining;
    for (paddingIndex = dataLength;
         paddingIndex < AGENT_XMODEM_BLOCK_SIZE;
         ++paddingIndex)
    {
        if (gXmodemPacket[3 + paddingIndex] != 0x1A)
        {
            response = 0x15;
            return AgentWriteRaw(&response, 1);
        }
    }

    error = AgentWriteFileBytes(
        gIncomingFileRef,
        gXmodemPacket + 3,
        dataLength);
    if (error != noErr)
        return AgentCancelTransfer();
    AgentUpdateCrc32(
        &gRunningPackageCrc32,
        gXmodemPacket + 3,
        dataLength);
    gReceivedPackageLength += dataLength;
    gLastXmodemBlock = blockNumber;
    gHasLastXmodemBlock = true;
    gExpectedXmodemBlock = (unsigned char)(gExpectedXmodemBlock + 1);
    response = 0x06;
    return AgentWriteRaw(&response, 1);
}

static OSErr AgentReceiveXmodemBytes(
    const unsigned char *data,
    unsigned short length)
{
    unsigned short index;

    for (index = 0; index < length; ++index)
    {
        unsigned char value = data[index];
        OSErr error;

        if (gXmodemPacketLength == 0)
        {
            if (value == 0x01)
            {
                gXmodemPacket[0] = value;
                gXmodemPacketLength = 1;
                gCancelByteCount = 0;
            }
            else if (value == 0x04)
            {
                return AgentFinishTransfer();
            }
            else if (value == 0x18)
            {
                if (++gCancelByteCount >= 2)
                    return AgentCancelTransfer();
            }
            else
            {
                gCancelByteCount = 0;
            }
            continue;
        }

        gXmodemPacket[gXmodemPacketLength++] = value;
        if (gXmodemPacketLength == AGENT_XMODEM_PACKET_SIZE)
        {
            error = AgentProcessXmodemPacket();
            gXmodemPacketLength = 0;
            if (error != noErr)
                return error;
            if (!gTransferActive)
                return noErr;
        }
    }
    return noErr;
}

static OSErr AgentSendHelloAck(const unsigned char *sessionId)
{
    unsigned char payload[20];
    long systemVersion = 0;
    long machineType = 0;
    unsigned long freeBytes;
    OSErr error;

    memset(payload, 0, sizeof(payload));
    payload[0] = AGENT_PROTOCOL_VERSION;
    payload[1] = 0;
    payload[2] = 1;
    payload[3] = 0;

    error = Gestalt(gestaltSystemVersion, &systemVersion);
    if (error != noErr)
    {
        AgentLogEvent("HELLO_GESTALT_SYSTEM_VERSION_ERROR", error);
        return error;
    }
    error = Gestalt(gestaltMachineType, &machineType);
    if (error != noErr)
    {
        AgentLogEvent("HELLO_GESTALT_MACHINE_TYPE_ERROR", error);
        return error;
    }
    error = AgentGetFreeBytes(&freeBytes);
    if (error != noErr)
    {
        AgentLogEvent("HELLO_VOLUME_SPACE_ERROR", error);
        return error;
    }

    payload[4] = (unsigned char)((systemVersion >> 8) & 0xFF);
    payload[5] = (unsigned char)((systemVersion >> 4) & 0x0F);
    AgentWriteUInt16(payload + 6, (unsigned short)machineType);
    AgentWriteUInt32(payload + 8, 7);
    AgentWriteUInt32(payload + 12, freeBytes);
    AgentWriteUInt32(payload + 16, AGENT_SUPPORTED_BAUD_RATE_FLAGS);
    error = AgentWriteFrame(0x02, sessionId, payload, sizeof(payload));
    if (error == noErr)
    {
        memcpy(gSessionId, sessionId, sizeof(gSessionId));
        gSessionActive = true;
        AgentLogEvent("HELLO_ACK_SYSTEM_VERSION", systemVersion);
        AgentLogEvent("HELLO_ACK_MACHINE_TYPE", machineType);
        AgentLogEvent("HELLO_ACK_VOLUME_FREE_BYTES", freeBytes);
        AgentLogEvent("HELLO_ACK_CAPABILITIES", 7);
        AgentLogEvent("HELLO_ACK_SUPPORTED_BAUD_FLAGS", AGENT_SUPPORTED_BAUD_RATE_FLAGS);
    }
    else
    {
        AgentLogEvent("HELLO_ACK_SERIAL_WRITE_ERROR", error);
    }
    return error;
}

static OSErr AgentSendProtocolError(
    const unsigned char *sessionId,
    unsigned char rejectedMessageType,
    const char *reason)
{
    unsigned char payload[132];
    unsigned short reasonLength = 0;

    if (reason == NULL)
        return paramErr;
    while (reasonLength < 128 && reason[reasonLength] != '\0')
        ++reasonLength;
    if (reason[reasonLength] != '\0')
        return paramErr;

    payload[0] = 0;
    payload[1] = 1;
    payload[2] = rejectedMessageType;
    payload[3] = (unsigned char)reasonLength;
    if (reasonLength != 0)
        memcpy(payload + 4, reason, reasonLength);
    return AgentWriteFrame(0x7F, sessionId, payload, reasonLength + 4);
}

static OSErr AgentSendBuildResult(
    unsigned char messageType,
    const unsigned char *sessionId,
    const unsigned char *buildId,
    unsigned char resultCode)
{
    unsigned char payload[17];

    memcpy(payload, buildId, 16);
    payload[16] = resultCode;
    return AgentWriteFrame(messageType, sessionId, payload, sizeof(payload));
}

static OSErr AgentRunInstalledBuild(
    const unsigned char *sessionId,
    const unsigned char *buildId,
    unsigned short payloadLength)
{
    LaunchParamBlockRec launchParameters;
    unsigned char resultCode = AGENT_RUN_RESULT_SUCCESS;
    Boolean isRunning = false;
    OSErr error = noErr;

    if (payloadLength != 16)
        return AgentSendProtocolError(sessionId, 0x20, "RUN requires a 16-byte Build ID");
    if (!gHasInstalledBuild ||
        memcmp(buildId, gInstalledBuildId, sizeof(gInstalledBuildId)) != 0)
        resultCode = AGENT_RUN_RESULT_NOT_INSTALLED;
    else
    {
        error = AgentRefreshButState(&isRunning);
        if (error != noErr)
            resultCode = AGENT_RUN_RESULT_LAUNCH_FAILED;
        else if (isRunning)
        {
            resultCode = memcmp(
                    buildId,
                    gRunningBuildId,
                    sizeof(gRunningBuildId)) == 0
                ? AGENT_RUN_RESULT_SUCCESS
                : AGENT_RUN_RESULT_BUSY;
        }
        else
        {
            error = FSMakeFSSpec(
                gVolumeRefNum,
                gStorageDirectoryId,
                (ConstStringPtr)gDestinationName,
                &gInstalledApplicationSpec);
            if (error == noErr)
            {
                memset(&launchParameters, 0, sizeof(launchParameters));
                launchParameters.launchBlockID = extendedBlock;
                launchParameters.launchEPBLength = extendedBlockLen;
                launchParameters.launchControlFlags = launchContinue;
                launchParameters.launchAppSpec = &gInstalledApplicationSpec;
                error = LaunchApplication(&launchParameters);
            }
            if (error != noErr)
                resultCode = AGENT_RUN_RESULT_LAUNCH_FAILED;
            else
            {
                gButProcess = launchParameters.launchProcessSN;
                gButRunning = true;
                memcpy(gRunningBuildId, buildId, sizeof(gRunningBuildId));
                error = AgentRefreshButState(&isRunning);
                if (error != noErr || !isRunning)
                {
                    gButRunning = false;
                    resultCode = AGENT_RUN_RESULT_LAUNCH_FAILED;
                }
            }
        }
    }

    AgentLogEvent("RUN_RESULT_CODE", resultCode);
    AgentLogEvent("RUN_PROCESS_QUERY_ERROR", error);
    return AgentSendBuildResult(0x21, sessionId, buildId, resultCode);
}

static OSErr AgentRequestButQuit(void)
{
    AEAddressDesc target;
    AppleEvent quitEvent;
    AppleEvent reply;
    OSErr error;
    OSErr disposeError;
    Boolean targetCreated = false;
    Boolean eventCreated = false;
    Boolean replyCreated = false;

    memset(&target, 0, sizeof(target));
    memset(&quitEvent, 0, sizeof(quitEvent));
    memset(&reply, 0, sizeof(reply));
    error = AECreateDesc(
        typeProcessSerialNumber,
        &gButProcess,
        sizeof(gButProcess),
        &target);
    if (error != noErr)
        goto cleanup;
    targetCreated = true;
    error = AECreateAppleEvent(
        kCoreEventClass,
        kAEQuitApplication,
        &target,
        kAutoGenerateReturnID,
        kAnyTransactionID,
        &quitEvent);
    if (error != noErr)
        goto cleanup;
    eventCreated = true;
    error = AESend(
        &quitEvent,
        &reply,
        kAEWaitReply,
        kAENormalPriority,
        kAEDefaultTimeout,
        (IdleUPP)0,
        (EventFilterUPP)0);
    if (error == noErr)
        replyCreated = true;

cleanup:
    if (replyCreated)
    {
        disposeError = AEDisposeDesc((AEDesc *)&reply);
        if (error == noErr)
            error = disposeError;
    }
    if (eventCreated)
    {
        disposeError = AEDisposeDesc((AEDesc *)&quitEvent);
        if (error == noErr)
            error = disposeError;
    }
    if (targetCreated)
    {
        disposeError = AEDisposeDesc((AEDesc *)&target);
        if (error == noErr)
            error = disposeError;
    }
    return error;
}

static OSErr AgentStopRunningBuild(
    const unsigned char *sessionId,
    const unsigned char *buildId,
    unsigned short payloadLength)
{
    unsigned char resultCode = AGENT_STOP_RESULT_SUCCESS;
    Boolean isRunning = false;
    OSErr error;

    if (payloadLength != 16)
        return AgentSendProtocolError(sessionId, 0x40, "STOP requires a 16-byte Build ID");

    error = AgentRefreshButState(&isRunning);
    if (error != noErr)
        resultCode = AGENT_STOP_RESULT_FAILED;
    else if (!isRunning)
        resultCode = AGENT_STOP_RESULT_NOT_RUNNING;
    else if (memcmp(buildId, gRunningBuildId, sizeof(gRunningBuildId)) != 0)
        resultCode = AGENT_STOP_RESULT_WRONG_BUILD;
    else
    {
        error = AgentRequestButQuit();
        if (error != noErr)
            resultCode = AGENT_STOP_RESULT_FAILED;
        else
            AgentRefreshButState(&isRunning);
    }

    AgentLogEvent("STOP_RESULT_CODE", resultCode);
    AgentLogEvent("STOP_PROCESS_QUERY_ERROR", error);
    return AgentSendBuildResult(0x41, sessionId, buildId, resultCode);
}

static OSErr AgentHandleFrame(const unsigned char *frame, unsigned short payloadLength)
{
    unsigned char minimumVersion;
    unsigned char maximumVersion;

    if (frame[5] == 0x01)
    {
        AgentLogEvent("HELLO_FRAME_RECEIVED", payloadLength);
        if (gTransferActive)
        {
            AgentLogMessage("HELLO_IGNORED_DURING_PACKAGE_TRANSFER");
            return noErr;
        }
        if (payloadLength != 4)
        {
            AgentLogEvent("HELLO_INVALID_PAYLOAD_LENGTH", payloadLength);
            return noErr;
        }

        minimumVersion = frame[AGENT_FRAME_HEADER_SIZE];
        maximumVersion = frame[AGENT_FRAME_HEADER_SIZE + 1];
        if (minimumVersion > maximumVersion ||
            minimumVersion > AGENT_PROTOCOL_VERSION ||
            maximumVersion < AGENT_PROTOCOL_VERSION)
        {
            AgentLogEvent("HELLO_PROTOCOL_RANGE_REJECTED", minimumVersion);
            return noErr;
        }

        AgentLogEvent("HOST_PROTOCOL_MINIMUM_VERSION", minimumVersion);
        AgentLogEvent("HOST_PROTOCOL_MAXIMUM_VERSION", maximumVersion);
        AgentLogEvent(
            "HOST_APPLICATION_MAJOR_VERSION",
            frame[AGENT_FRAME_HEADER_SIZE + 2]);
        AgentLogEvent(
            "HOST_APPLICATION_MINOR_VERSION",
            frame[AGENT_FRAME_HEADER_SIZE + 3]);
        {
            OSErr error = AgentSendHelloAck(frame + 8);
            AgentLogEvent("HELLO_ACK_SEND_ERROR", error);
            return error;
        }
    }

    if (!gSessionActive ||
        memcmp(frame + 8, gSessionId, sizeof(gSessionId)) != 0)
    {
        AgentLogEvent("FRAME_IGNORED_WITHOUT_ACTIVE_SESSION", frame[5]);
        return noErr;
    }

    if (frame[5] == AGENT_HOST_DIAGNOSTICS_MESSAGE_TYPE)
    {
        OSErr error = AgentLogHostDiagnostics(
            frame + AGENT_FRAME_HEADER_SIZE,
            payloadLength);
        AgentLogEvent("HOST_DIAGNOSTICS_ACCEPT_ERROR", error);
        return noErr;
    }

    if (frame[5] == 0x10)
    {
        AgentLogEvent("PUT_BEGIN_RECEIVED", payloadLength);
        return AgentBeginPut(
            frame + 8,
            frame + AGENT_FRAME_HEADER_SIZE,
            payloadLength);
    }
    if (frame[5] == 0x20)
    {
        AgentLogEvent("RUN_REQUEST_RECEIVED", payloadLength);
        return AgentRunInstalledBuild(
            frame + 8,
            frame + AGENT_FRAME_HEADER_SIZE,
            payloadLength);
    }
    if (frame[5] == 0x40)
    {
        AgentLogEvent("STOP_REQUEST_RECEIVED", payloadLength);
        return AgentStopRunningBuild(
            frame + 8,
            frame + AGENT_FRAME_HEADER_SIZE,
            payloadLength);
    }

    if (frame[5] != 0x31 ||
        payloadLength != 5 ||
        !gDebugRecordPending ||
        !gDebugRecordSent ||
        AgentReadUInt32(frame + AGENT_FRAME_HEADER_SIZE) != gDebugSequence)
        return noErr;

    if (frame[AGENT_FRAME_HEADER_SIZE + 4] == 0)
    {
        AgentLogEvent("DEBUG_RECORD_ACKNOWLEDGED", gDebugSequence);
        gDebugRecordPending = 0;
        gDebugRecordSent = 0;
        gDebugRecordLength = 0;
    }
    return noErr;
}

static pascal OSErr AgentHandleDebugWrite(
    const AppleEvent *event,
    AppleEvent *reply,
    int32_t refcon)
{
    unsigned char request[AGENT_APPLE_EVENT_PREFIX_SIZE + AGENT_MAX_DEBUG_RECORD_SIZE];
    DescType actualType = typeNull;
    Size requestSize = 0;
    unsigned short textLength;
    unsigned short index;
    OSErr error;

    (void)reply;
    (void)refcon;

    error = AEGetParamPtr(
        (AERecord *)event,
        keyDirectObject,
        typeChar,
        &actualType,
        (Ptr)request,
        sizeof(request),
        &requestSize);
    if (error != noErr)
    {
        AgentLogEvent("DEBUG_APPLEEVENT_READ_ERROR", error);
        return error;
    }
    if (actualType != typeChar || requestSize < AGENT_APPLE_EVENT_PREFIX_SIZE)
    {
        AgentLogEvent("DEBUG_APPLEEVENT_INVALID_TYPE_OR_LENGTH", requestSize);
        return paramErr;
    }

    textLength = AgentReadUInt16(request + 1);
    if (textLength > AGENT_MAX_DEBUG_RECORD_SIZE ||
        requestSize != AGENT_APPLE_EVENT_PREFIX_SIZE + textLength ||
        gDebugRecordPending)
    {
        AgentLogEvent("DEBUG_APPLEEVENT_REJECTED_LENGTH_OR_BUSY", textLength);
        return paramErr;
    }

    for (index = 0; index < textLength; ++index)
    {
        unsigned char value = request[AGENT_APPLE_EVENT_PREFIX_SIZE + index];
        if (value != '\t' && (value < 0x20 || value > 0x7E))
        {
            AgentLogEvent("DEBUG_APPLEEVENT_REJECTED_NONPRINTABLE_BYTE", index);
            return paramErr;
        }
    }

    gDebugSeverity = request[0];
    gDebugRecordLength = textLength;
    memcpy(gDebugRecord, request + AGENT_APPLE_EVENT_PREFIX_SIZE, textLength);
    gDebugSequence = gNextDebugSequence++;
    if (gNextDebugSequence == 0)
        gNextDebugSequence = 1;
    gDebugRecordPending = 1;
    gDebugRecordSent = 0;
    AgentLogEvent("DEBUG_APPLEEVENT_QUEUED_SEVERITY", gDebugSeverity);
    AgentLogEvent("DEBUG_APPLEEVENT_QUEUED_LENGTH", gDebugRecordLength);
    AgentLogEvent("DEBUG_APPLEEVENT_SEQUENCE", gDebugSequence);
    return noErr;
}

static OSErr AgentInstallDebugHandler(void)
{
    gDebugHandler = NewAEEventHandlerUPP(AgentHandleDebugWrite);
    return AEInstallEventHandler(
        AGENT_DEBUG_EVENT_CLASS,
        AGENT_DEBUG_EVENT_ID,
        gDebugHandler,
        0,
        (Boolean)0);
}

static pascal OSErr AgentHandleLaunchRequest(
    const AppleEvent *event,
    AppleEvent *reply,
    int32_t refcon)
{
    FSSpec applicationSpec;
    LaunchParamBlockRec launchParameters;
    DescType actualType = typeNull;
    Size parameterSize = 0;
    OSErr error;

    (void)reply;
    (void)refcon;

    error = AEGetParamPtr(
        (AERecord *)event,
        keyDirectObject,
        typeFSS,
        &actualType,
        (Ptr)&applicationSpec,
        sizeof(applicationSpec),
        &parameterSize);
    if (error != noErr)
        return error;
    if (actualType != typeFSS || parameterSize != sizeof(applicationSpec))
        return paramErr;

    memset(&launchParameters, 0, sizeof(launchParameters));
    launchParameters.launchBlockID = extendedBlock;
    launchParameters.launchEPBLength = extendedBlockLen;
    launchParameters.launchControlFlags = launchContinue;
    launchParameters.launchAppSpec = &applicationSpec;
    return LaunchApplication(&launchParameters);
}

static OSErr AgentInstallLaunchHandler(void)
{
    OSErr error;

    gLaunchHandler = NewAEEventHandlerUPP(AgentHandleLaunchRequest);
    error = AEInstallEventHandler(
        AGENT_DEBUG_EVENT_CLASS,
        'LNCH',
        gLaunchHandler,
        0,
        (Boolean)0);
    if (error != noErr)
    {
        DisposeAEEventHandlerUPP(gLaunchHandler);
        gLaunchHandler = NULL;
    }
    return error;
}

static OSErr AgentSendPendingDebugRecord(void)
{
    unsigned char payload[AGENT_DEBUG_RECORD_HEADER_SIZE + AGENT_MAX_DEBUG_RECORD_SIZE];
    OSErr error;

    if (!gSessionActive || gTransferActive ||
        !gDebugRecordPending || gDebugRecordSent)
        return noErr;

    AgentWriteUInt32(payload, gDebugSequence);
    payload[4] = gDebugSeverity;
    AgentWriteUInt16(payload + 5, gDebugRecordLength);
    if (gDebugRecordLength != 0)
        memcpy(payload + AGENT_DEBUG_RECORD_HEADER_SIZE, gDebugRecord, gDebugRecordLength);
    error = AgentWriteFrame(
        0x30,
        gSessionId,
        payload,
        (unsigned short)(AGENT_DEBUG_RECORD_HEADER_SIZE + gDebugRecordLength));
    if (error == noErr)
        gDebugRecordSent = 1;
    AgentLogEvent("DEBUG_RECORD_SERIAL_SEND_ERROR", error);
    return error;
}

static void AgentDropFramePrefix(unsigned short count)
{
    if (count >= gFrameLength)
    {
        gFrameLength = 0;
        return;
    }

    gFrameLength = (unsigned short)(gFrameLength - count);
    memmove(gFrameBuffer, gFrameBuffer + count, gFrameLength);
}

static OSErr AgentParseFrames(void)
{
    while (gFrameLength >= 4)
    {
        unsigned short payloadLength;
        unsigned short frameLength;
        unsigned short expectedCrc;
        unsigned short actualCrc;
        OSErr error;

        if (gFrameBuffer[0] != 'M' ||
            gFrameBuffer[1] != 'S' ||
            gFrameBuffer[2] != 'A' ||
            gFrameBuffer[3] != 'G')
        {
            ++gInvalidFrameBytes;
            AgentDropFramePrefix(1);
            continue;
        }

        if (gFrameLength < AGENT_FRAME_HEADER_SIZE)
            return noErr;

        payloadLength = AgentReadUInt16(gFrameBuffer + 24);
        if (gFrameBuffer[4] != AGENT_PROTOCOL_VERSION ||
            gFrameBuffer[6] != 0 ||
            gFrameBuffer[7] != 0 ||
            payloadLength > AGENT_MAX_PAYLOAD_SIZE)
        {
            ++gInvalidFrameBytes;
            AgentDropFramePrefix(1);
            continue;
        }

        frameLength = (unsigned short)(
            AGENT_FRAME_HEADER_SIZE + payloadLength + AGENT_FRAME_CRC_SIZE);
        if (gFrameLength < frameLength)
            return noErr;

        expectedCrc = AgentReadUInt16(gFrameBuffer + frameLength - AGENT_FRAME_CRC_SIZE);
        actualCrc = AgentCrc16(gFrameBuffer, (unsigned short)(frameLength - AGENT_FRAME_CRC_SIZE));
        if (expectedCrc != actualCrc)
        {
            ++gInvalidFrameCrcs;
            AgentDropFramePrefix(1);
            continue;
        }

        ++gValidFramesReceived;
        error = AgentHandleFrame(gFrameBuffer, payloadLength);
        AgentDropFramePrefix(frameLength);
        if (error != noErr)
        {
            AgentLogEvent("PROTOCOL_FRAME_HANDLER_ERROR", error);
            return error;
        }
    }
    return noErr;
}

static OSErr AgentReceiveBytes(const unsigned char *data, unsigned short length)
{
    unsigned short index;
    OSErr error;

    gSerialBytesReceived += length;
    if (gTransferActive)
        return AgentReceiveXmodemBytes(data, length);

    for (index = 0; index < length; ++index)
    {
        if (gFrameLength >= sizeof(gFrameBuffer))
            AgentDropFramePrefix(1);
        gFrameBuffer[gFrameLength++] = data[index];
        error = AgentParseFrames();
        if (error != noErr)
            return error;
    }
    return noErr;
}

static OSErr AgentPollSerial(void)
{
    unsigned char bytes[AGENT_SERIAL_READ_SIZE];
    long available = 0;
    LONGINT readLength;
    OSErr error;

    error = SerGetBuf(gSerialInputRef, &available);
    if (error != noErr)
    {
        AgentLogEvent("SERIAL_GET_BUFFER_ERROR", error);
        return error;
    }
    if (available <= 0)
        return error;

    AgentLogEvent("SERIAL_RX_AVAILABLE", available);
    readLength = available < sizeof(bytes) ? available : sizeof(bytes);
    error = FSRead(gSerialInputRef, &readLength, bytes);
    if (error != noErr)
    {
        AgentLogEvent("SERIAL_READ_ERROR", error);
        return error;
    }
    if (readLength <= 0)
    {
        AgentLogEvent("SERIAL_READ_NO_BYTES", readLength);
        return error;
    }
    AgentLogEvent("SERIAL_RX_CHUNK", readLength);
    return AgentReceiveBytes(bytes, (unsigned short)readLength);
}

static Boolean AgentIsConfigWhitespace(unsigned char value)
{
    return (Boolean)(value == ' ' || value == '\t' ||
        value == '\r' || value == '\n');
}

static Boolean AgentTextEqualsIgnoreCase(
    const unsigned char *text,
    unsigned short textLength,
    const char *expected)
{
    unsigned short index;
    unsigned short expectedLength = 0;

    if (text == NULL || expected == NULL)
        return false;
    while (expected[expectedLength] != '\0')
        ++expectedLength;
    if (textLength != expectedLength)
        return false;

    for (index = 0; index < textLength; ++index)
    {
        unsigned char value = text[index];
        unsigned char expectedValue = (unsigned char)expected[index];
        if (value >= 'a' && value <= 'z')
            value = (unsigned char)(value - ('a' - 'A'));
        if (expectedValue >= 'a' && expectedValue <= 'z')
            expectedValue = (unsigned char)(expectedValue - ('a' - 'A'));
        if (value != expectedValue)
            return false;
    }
    return true;
}

static OSErr AgentReadTextConfig(
    const unsigned char *configName,
    unsigned char *configText,
    unsigned short configCapacity,
    unsigned short *configLength)
{
    FSSpec configSpec;
    short fileRef = 0;
    long fileLength = 0;
    LONGINT readLength;
    OSErr error;
    OSErr closeError;

    if (configName == NULL || configText == NULL || configLength == NULL ||
        configCapacity == 0)
        return paramErr;
    *configLength = 0;
    error = FSMakeFSSpec(
        gVolumeRefNum,
        gStorageDirectoryId,
        (ConstStringPtr)configName,
        &configSpec);
    if (error != noErr)
        return error;

    error = FSpOpenDF(&configSpec, fsRdPerm, &fileRef);
    if (error != noErr)
        return error;
    error = GetEOF(fileRef, &fileLength);
    if (error == noErr && (fileLength <= 0 || fileLength > sizeof(configText)))
        error = paramErr;
    readLength = fileLength;
    if (error == noErr)
    {
        error = FSRead(fileRef, &readLength, (Ptr)configText);
        if (error == noErr && readLength != fileLength)
            error = ioErr;
    }
    closeError = FSClose(fileRef);
    if (error == noErr)
        error = closeError;
    if (error != noErr)
        return error;

    *configLength = (unsigned short)fileLength;
    return noErr;
}

static void AgentTrimConfigText(
    const unsigned char *configText,
    unsigned short configLength,
    unsigned short *start,
    unsigned short *end)
{
    *start = 0;
    *end = configLength;
    while (*start < *end && AgentIsConfigWhitespace(configText[*start]))
        ++*start;
    while (*end > *start && AgentIsConfigWhitespace(configText[*end - 1]))
        --*end;
}

static OSErr AgentReadSerialPortChoice(Boolean *usePrinterPort)
{
    unsigned char configText[16];
    unsigned short configLength;
    unsigned short start;
    unsigned short end;
    OSErr error;

    if (usePrinterPort == NULL)
        return paramErr;
    *usePrinterPort = false;
    error = AgentReadTextConfig(
        gSerialPortConfigName,
        configText,
        sizeof(configText),
        &configLength);
    if (error == fnfErr)
        return noErr;
    if (error != noErr)
        return error;

    AgentTrimConfigText(configText, configLength, &start, &end);
    if (AgentTextEqualsIgnoreCase(configText + start, end - start, "MODEM"))
        return noErr;
    if (AgentTextEqualsIgnoreCase(configText + start, end - start, "PRINTER"))
    {
        *usePrinterPort = true;
        return noErr;
    }
    return paramErr;
}

static OSErr AgentReadSerialBaudRate(short *baudRate)
{
    unsigned char configText[16];
    unsigned short configLength;
    unsigned short start;
    unsigned short end;
    OSErr error;

    if (baudRate == NULL)
        return paramErr;
    *baudRate = baud9600;
    error = AgentReadTextConfig(
        gSerialBaudConfigName,
        configText,
        sizeof(configText),
        &configLength);
    if (error == fnfErr)
        return noErr;
    if (error != noErr)
        return error;

    AgentTrimConfigText(configText, configLength, &start, &end);
    if (AgentTextEqualsIgnoreCase(configText + start, end - start, "300"))
        *baudRate = baud300;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "600"))
        *baudRate = baud600;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "1200"))
        *baudRate = baud1200;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "1800"))
        *baudRate = baud1800;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "2400"))
        *baudRate = baud2400;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "3600"))
        *baudRate = baud3600;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "4800"))
        *baudRate = baud4800;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "7200"))
        *baudRate = baud7200;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "9600"))
        *baudRate = baud9600;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "14400"))
        *baudRate = baud14400;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "19200"))
        *baudRate = baud19200;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "28800"))
        *baudRate = baud28800;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "38400"))
        *baudRate = baud38400;
    else if (AgentTextEqualsIgnoreCase(configText + start, end - start, "57600"))
        *baudRate = baud57600;
    else
        return paramErr;
    return noErr;
}

static short AgentFindBaudRateIndex(short baudRate)
{
    unsigned short index;

    for (index = 0; index < AGENT_BAUD_RATE_COUNT; ++index)
    {
        if (gSupportedBaudRates[index] == baudRate)
            return (short)index;
    }
    return -1;
}

static OSErr AgentInitializeUserInterface(
    Boolean usePrinterPort,
    short baudRate)
{
    short baudRateIndex = AgentFindBaudRateIndex(baudRate);
    unsigned short index;

    if (baudRateIndex < 0)
        return paramErr;

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    gBaudRateMenu = NewMenu(
        AGENT_BAUD_RATE_MENU_ID,
        (ConstStringPtr)gBaudRateMenuTitle);
    if (gBaudRateMenu == NULL)
        return memFullErr;
    for (index = 0; index < AGENT_BAUD_RATE_COUNT; ++index)
    {
        AppendMenu(
            gBaudRateMenu,
            (ConstStringPtr)gBaudRateMenuItems[index]);
        EnableItem(gBaudRateMenu, index + 1);
    }
    InsertMenu(gBaudRateMenu, 0);
    EnableItem(gBaudRateMenu, 0);
    CheckItem(gBaudRateMenu, baudRateIndex + 1, true);

    gPortMenu = NewMenu(
        AGENT_PORT_MENU_ID,
        (ConstStringPtr)gPortMenuTitle);
    if (gPortMenu == NULL)
        return memFullErr;
    for (index = 0; index < AGENT_PORT_COUNT; ++index)
    {
        AppendMenu(gPortMenu, (ConstStringPtr)gPortMenuItems[index]);
        EnableItem(gPortMenu, index + 1);
    }
    InsertMenu(gPortMenu, 0);
    EnableItem(gPortMenu, 0);
    CheckItem(gPortMenu, usePrinterPort ? 2 : 1, true);

    gAgentMenu = NewMenu(
        AGENT_AGENT_MENU_ID,
        (ConstStringPtr)gAgentMenuTitle);
    if (gAgentMenu == NULL)
        return memFullErr;
    for (index = 0; index < 3; ++index)
    {
        AppendMenu(gAgentMenu, (ConstStringPtr)gAgentMenuItems[index]);
        EnableItem(gAgentMenu, index + 1);
    }
    InsertMenu(gAgentMenu, 0);
    EnableItem(gAgentMenu, 0);
    DrawMenuBar();
    return noErr;
}

static OSErr AgentWriteTextConfig(
    const unsigned char *configName,
    const unsigned char *configValue)
{
    FSSpec configSpec;
    short fileRef = 0;
    long writeLength;
    OSErr error;
    OSErr closeError;

    if (configName == NULL || configValue == NULL || configValue[0] == 0)
        return paramErr;
    error = FSMakeFSSpec(
        gVolumeRefNum,
        gStorageDirectoryId,
        (ConstStringPtr)configName,
        &configSpec);
    if (error == fnfErr)
    {
        error = FSpCreate(&configSpec, 'TEXT', 'ttxt', smSystemScript);
        if (error != noErr)
            return error;
    }
    else if (error != noErr)
    {
        return error;
    }

    error = FSpOpenDF(&configSpec, fsWrPerm, &fileRef);
    if (error != noErr)
        return error;
    error = SetEOF(fileRef, 0);
    if (error == noErr)
        error = SetFPos(fileRef, fsFromStart, 0);
    writeLength = configValue[0];
    if (error == noErr)
        error = FSWrite(
            fileRef,
            &writeLength,
            (Ptr)(configValue + 1));
    if (error == noErr && writeLength != configValue[0])
        error = ioErr;
    closeError = FSClose(fileRef);
    if (error == noErr)
        error = closeError;
    return error;
}

static OSErr AgentWriteSerialPortChoice(Boolean usePrinterPort)
{
    return AgentWriteTextConfig(
        gSerialPortConfigName,
        usePrinterPort ? gPrinterPortConfigValue : gModemPortConfigValue);
}

static OSErr AgentWriteSerialBaudRate(short baudRate)
{
    short baudRateIndex = AgentFindBaudRateIndex(baudRate);

    if (baudRateIndex < 0)
        return paramErr;
    return AgentWriteTextConfig(
        gSerialBaudConfigName,
        gBaudRateMenuItems[baudRateIndex]);
}

static void AgentShowSerialDialog(short dialogId)
{
    DialogPtr dialog = GetNewDialog(dialogId, NULL, (WindowPtr)-1);
    short item = 0;

    if (dialog == NULL)
    {
        SysBeep(1);
        return;
    }
    ModalDialog(NULL, &item);
    DisposeDialog(dialog);
}

static unsigned short AgentAppendCString(
    unsigned char *buffer,
    unsigned short offset,
    const char *text)
{
    while (*text != '\0')
        buffer[offset++] = (unsigned char)*text++;
    return offset;
}

static unsigned short AgentAppendUnsignedDecimal(
    unsigned char *buffer,
    unsigned short offset,
    unsigned long value)
{
    unsigned char digits[10];
    unsigned short count = 0;

    do
    {
        digits[count++] = (unsigned char)('0' + (value % 10));
        value /= 10;
    } while (value != 0 && count < sizeof(digits));

    while (count > 0)
        buffer[offset++] = digits[--count];
    return offset;
}

static unsigned short AgentAppendSignedDecimal(
    unsigned char *buffer,
    unsigned short offset,
    long value)
{
    unsigned long magnitude;

    if (value < 0)
    {
        buffer[offset++] = '-';
        magnitude = (unsigned long)(-(value + 1)) + 1;
    }
    else
    {
        magnitude = (unsigned long)value;
    }
    return AgentAppendUnsignedDecimal(buffer, offset, magnitude);
}

static OSErr AgentAppendDiagnostic(
    const unsigned char *message,
    unsigned short messageLength)
{
    FSSpec logSpec;
    short fileRef = 0;
    long fileLength = 0;
    LONGINT writeLength;
    unsigned long timestamp;
    unsigned char line[AGENT_DIAGNOSTIC_LINE_MAX_SIZE];
    unsigned short lineLength = 0;
    OSErr error;
    OSErr closeError;

    if (message == NULL ||
        messageLength > AGENT_DIAGNOSTIC_LINE_MAX_SIZE - 32)
        return paramErr;

    error = FSMakeFSSpec(
        gVolumeRefNum,
        gStorageDirectoryId,
        (ConstStringPtr)gDiagnosticLogName,
        &logSpec);
    if (error == fnfErr)
    {
        error = FSpCreate(&logSpec, 'TEXT', 'ttxt', smSystemScript);
        if (error != noErr)
            goto reportFailure;
    }
    else if (error != noErr)
    {
        goto reportFailure;
    }

    error = FSpOpenDF(&logSpec, fsWrPerm, &fileRef);
    if (error != noErr)
        goto reportFailure;

    error = GetEOF(fileRef, &fileLength);
    if (error == noErr && (unsigned long)fileLength >= AGENT_DIAGNOSTIC_LOG_MAX_SIZE)
        error = SetEOF(fileRef, 0);
    if (error == noErr)
        error = SetFPos(fileRef, fsFromLEOF, 0);
    if (error != noErr)
        goto closeAndReport;

    GetDateTime(&timestamp);
    lineLength = AgentAppendCString(line, lineLength, "T=");
    lineLength = AgentAppendUnsignedDecimal(line, lineLength, timestamp);
    line[lineLength++] = ' ';
    memcpy(line + lineLength, message, messageLength);
    lineLength = (unsigned short)(lineLength + messageLength);
    line[lineLength++] = '\r';
    writeLength = lineLength;
    error = FSWrite(fileRef, &writeLength, (Ptr)line);
    if (error == noErr && writeLength != lineLength)
        error = ioErr;

closeAndReport:
    closeError = FSClose(fileRef);
    if (error == noErr)
        error = closeError;
    if (error == noErr)
        return noErr;

reportFailure:
    if (!gDiagnosticLogFailureReported)
    {
        gDiagnosticLogFailureReported = true;
        AgentShowSerialDialog(AGENT_DIAGNOSTIC_LOG_FAILURE_DIALOG_ID);
    }
    return error;
}

static void AgentLogEvent(const char *eventName, long value)
{
    unsigned char message[160];
    unsigned short messageLength = 0;

    messageLength = AgentAppendCString(message, messageLength, eventName);
    messageLength = AgentAppendCString(message, messageLength, " value=");
    messageLength = AgentAppendSignedDecimal(message, messageLength, value);
    AgentAppendDiagnostic(message, messageLength);
}

static void AgentLogPortState(
    const char *eventName,
    Boolean usePrinterPort,
    short baudRate,
    OSErr error)
{
    unsigned char message[160];
    unsigned short messageLength = 0;

    messageLength = AgentAppendCString(message, messageLength, eventName);
    messageLength = AgentAppendCString(message, messageLength, " port=");
    messageLength = AgentAppendCString(
        message,
        messageLength,
        usePrinterPort ? "PRINTER" : "MODEM");
    messageLength = AgentAppendCString(message, messageLength, " baud=");
    messageLength = AgentAppendUnsignedDecimal(
        message,
        messageLength,
        (unsigned long)baudRate);
    messageLength = AgentAppendCString(message, messageLength, " error=");
    messageLength = AgentAppendSignedDecimal(message, messageLength, error);
    AgentAppendDiagnostic(message, messageLength);
}

static void AgentLogSerialStatus(
    const char *label,
    short driverRef,
    Boolean isInputDriver)
{
    SerStaRec status;
    long available = 0;
    OSErr statusError;
    OSErr bufferError = noErr;
    unsigned char message[256];
    unsigned short messageLength = 0;

    memset(&status, 0, sizeof(status));
    statusError = SerStatus(driverRef, &status);
    if (isInputDriver)
        bufferError = SerGetBuf(driverRef, &available);

    messageLength = AgentAppendCString(message, messageLength, label);
    messageLength = AgentAppendCString(message, messageLength, " ref=");
    messageLength = AgentAppendSignedDecimal(message, messageLength, driverRef);
    messageLength = AgentAppendCString(message, messageLength, " statusError=");
    messageLength = AgentAppendSignedDecimal(message, messageLength, statusError);
    if (statusError == noErr)
    {
        messageLength = AgentAppendCString(message, messageLength, " cumulativeErrors=");
        messageLength = AgentAppendUnsignedDecimal(message, messageLength, status.cumErrs);
        messageLength = AgentAppendCString(message, messageLength, " xOffSent=");
        messageLength = AgentAppendUnsignedDecimal(message, messageLength, status.xOffSent);
        messageLength = AgentAppendCString(message, messageLength, " readPending=");
        messageLength = AgentAppendUnsignedDecimal(message, messageLength, status.rdPend);
        messageLength = AgentAppendCString(message, messageLength, " writePending=");
        messageLength = AgentAppendUnsignedDecimal(message, messageLength, status.wrPend);
        messageLength = AgentAppendCString(message, messageLength, " ctsHold=");
        messageLength = AgentAppendUnsignedDecimal(message, messageLength, status.ctsHold);
        messageLength = AgentAppendCString(message, messageLength, " xOffHold=");
        messageLength = AgentAppendUnsignedDecimal(message, messageLength, status.xOffHold);
        messageLength = AgentAppendCString(message, messageLength, " dsrHold=");
        messageLength = AgentAppendUnsignedDecimal(message, messageLength, status.dsrHold);
        messageLength = AgentAppendCString(message, messageLength, " modemStatus=");
        messageLength = AgentAppendUnsignedDecimal(message, messageLength, status.modemStatus);
    }
    if (isInputDriver)
    {
        messageLength = AgentAppendCString(message, messageLength, " rxBufferError=");
        messageLength = AgentAppendSignedDecimal(message, messageLength, bufferError);
        messageLength = AgentAppendCString(message, messageLength, " rxAvailable=");
        messageLength = AgentAppendSignedDecimal(message, messageLength, available);
    }
    AgentAppendDiagnostic(message, messageLength);
}

static void AgentLogMessage(const char *messageText)
{
    unsigned char message[AGENT_DIAGNOSTIC_LINE_MAX_SIZE - 32];
    unsigned short messageLength = 0;

    messageLength = AgentAppendCString(message, messageLength, messageText);
    AgentAppendDiagnostic(message, messageLength);
}

static void AgentLogSystemSnapshot(void)
{
    long value = 0;
    unsigned long freeBytes = 0;
    OSErr error;

    error = Gestalt(gestaltSystemVersion, &value);
    AgentLogEvent(
        error == noErr ? "SYSTEM_VERSION" : "SYSTEM_VERSION_GESTALT_ERROR",
        error == noErr ? value : error);
    error = Gestalt(gestaltMachineType, &value);
    AgentLogEvent(
        error == noErr ? "MACHINE_TYPE" : "MACHINE_TYPE_GESTALT_ERROR",
        error == noErr ? value : error);
    error = Gestalt(gestaltProcessorType, &value);
    AgentLogEvent(
        error == noErr ? "PROCESSOR_TYPE" : "PROCESSOR_TYPE_GESTALT_ERROR",
        error == noErr ? value : error);
    error = Gestalt(gestaltROMVersion, &value);
    AgentLogEvent(
        error == noErr ? "ROM_VERSION" : "ROM_VERSION_GESTALT_ERROR",
        error == noErr ? value : error);
    error = Gestalt(gestaltROMSize, &value);
    AgentLogEvent(
        error == noErr ? "ROM_SIZE" : "ROM_SIZE_GESTALT_ERROR",
        error == noErr ? value : error);
    error = AgentGetFreeBytes(&freeBytes);
    AgentLogEvent(
        error == noErr ? "APPLICATION_VOLUME_FREE_BYTES" : "VOLUME_SPACE_ERROR",
        error == noErr ? (long)freeBytes : error);
    AgentLogEvent("APPLICATION_VOLUME_REFNUM", gVolumeRefNum);
    AgentLogEvent("STORAGE_DIRECTORY_ID", gStorageDirectoryId);
    AgentLogMessage(
        "SERIAL_DRIVER_STATUS_DOES_NOT_PROVE_CABLE_OR_REMOTE_PEER; "
        "MODEM_STATUS_FIELDS_ARE_UNIMPLEMENTED_BY_THE_CLASSIC_DRIVER");
}

static OSErr AgentDiagnosePort(
    Boolean usePrinterPort,
    Boolean isActivePort,
    short baudRate)
{
    const unsigned char *inputDriverName = usePrinterPort
        ? gPrinterInputDriverName
        : gModemInputDriverName;
    const unsigned char *outputDriverName = usePrinterPort
        ? gPrinterOutputDriverName
        : gModemOutputDriverName;
    short inputRef = 0;
    short outputRef = 0;
    OSErr error;
    OSErr closeError;

    AgentLogPortState(
        "PORT_DIAGNOSTIC_BEGIN",
        usePrinterPort,
        baudRate,
        noErr);
    if (isActivePort)
    {
        AgentLogSerialStatus(
            usePrinterPort ? "ACTIVE_PRINTER_INPUT" : "ACTIVE_MODEM_INPUT",
            gSerialInputRef,
            true);
        AgentLogSerialStatus(
            usePrinterPort ? "ACTIVE_PRINTER_OUTPUT" : "ACTIVE_MODEM_OUTPUT",
            gSerialOutputRef,
            false);
        return noErr;
    }

    error = OpenDriver((ConstStringPtr)inputDriverName, &inputRef);
    AgentLogEvent(
        usePrinterPort ? "DIAG_PRINTER_INPUT_OPEN_ERROR" : "DIAG_MODEM_INPUT_OPEN_ERROR",
        error);
    if (error == noErr)
    {
        AgentLogSerialStatus(
            usePrinterPort ? "PROBED_PRINTER_INPUT" : "PROBED_MODEM_INPUT",
            inputRef,
            true);
        error = OpenDriver((ConstStringPtr)outputDriverName, &outputRef);
        AgentLogEvent(
            usePrinterPort ? "DIAG_PRINTER_OUTPUT_OPEN_ERROR" : "DIAG_MODEM_OUTPUT_OPEN_ERROR",
            error);
        if (error == noErr)
        {
            AgentLogSerialStatus(
                usePrinterPort ? "PROBED_PRINTER_OUTPUT" : "PROBED_MODEM_OUTPUT",
                outputRef,
                false);
        }
    }

    if (outputRef != 0)
    {
        closeError = CloseDriver(outputRef);
        AgentLogEvent(
            usePrinterPort ? "DIAG_PRINTER_OUTPUT_CLOSE_ERROR" : "DIAG_MODEM_OUTPUT_CLOSE_ERROR",
            closeError);
        if (error == noErr)
            error = closeError;
    }
    if (inputRef != 0)
    {
        closeError = CloseDriver(inputRef);
        AgentLogEvent(
            usePrinterPort ? "DIAG_PRINTER_INPUT_CLOSE_ERROR" : "DIAG_MODEM_INPUT_CLOSE_ERROR",
            closeError);
        if (error == noErr)
            error = closeError;
    }
    AgentLogPortState(
        "PORT_DIAGNOSTIC_END",
        usePrinterPort,
        baudRate,
        error);
    return error;
}

static void AgentDiagnosePorts(Boolean currentUsePrinterPort, short baudRate)
{
    AgentLogMessage("PORT_PROBE_OPENS_AND_CLOSES_DRIVERS_ONLY; IT_SENDS_NO_TEST_BYTES");
    AgentDiagnosePort(currentUsePrinterPort, true, baudRate);
    AgentDiagnosePort(!currentUsePrinterPort, false, baudRate);
    AgentLogMessage(
        "A_SUCCESSFUL_DRIVER_PROBE_ONLY_MEANS_OPEN_SUCCEEDED; "
        "CABLE_SIGNAL_INTEGRITY_REQUIRES_A_HOST_HELLO_HANDSHAKE");
    AgentLogEvent("PORT_DIAGNOSTICS_COMPLETE", noErr);
}

static void AgentLogPeriodicSerialStatus(Boolean usePrinterPort, short baudRate)
{
    unsigned long currentTick = TickCount();

    if (currentTick - gLastDiagnosticSampleTick < 3600)
        return;
    gLastDiagnosticSampleTick = currentTick;
    AgentLogPortState(
        "PERIODIC_AGENT_HEALTH",
        usePrinterPort,
        baudRate,
        noErr);
    AgentLogEvent("SERIAL_BYTES_RECEIVED", (long)gSerialBytesReceived);
    AgentLogEvent("SERIAL_BYTES_SENT", (long)gSerialBytesSent);
    AgentLogEvent("VALID_FRAMES_RECEIVED", (long)gValidFramesReceived);
    AgentLogEvent("NON_FRAME_BYTES_SKIPPED", (long)gInvalidFrameBytes);
    AgentLogEvent("FRAME_CRC_ERRORS", (long)gInvalidFrameCrcs);
    AgentLogSerialStatus(
        usePrinterPort ? "ACTIVE_PRINTER_INPUT" : "ACTIVE_MODEM_INPUT",
        gSerialInputRef,
        true);
    AgentLogSerialStatus(
        usePrinterPort ? "ACTIVE_PRINTER_OUTPUT" : "ACTIVE_MODEM_OUTPUT",
        gSerialOutputRef,
        false);
}

static OSErr AgentLogHostDiagnostics(
    const unsigned char *data,
    unsigned short length)
{
    unsigned char message[AGENT_DIAGNOSTIC_LINE_MAX_SIZE - 32];
    unsigned short messageLength = 0;
    unsigned short index;

    if (data == NULL || length == 0 ||
        length > AGENT_HOST_DIAGNOSTIC_MAX_SIZE)
        return paramErr;
    for (index = 0; index < length; ++index)
    {
        if (data[index] < 0x20 || data[index] > 0x7E)
            return paramErr;
    }
    messageLength = AgentAppendCString(
        message,
        messageLength,
        "HOST_MACHINE_DIAGNOSTICS ");
    memcpy(message + messageLength, data, length);
    messageLength = (unsigned short)(messageLength + length);
    return AgentAppendDiagnostic(message, messageLength);
}

static OSErr AgentOpenSerialPort(Boolean usePrinterPort, short baudRate)
{
    short configuration = baudRate + stop10 + data8 + noParity;
    SerShk handshake;
    const unsigned char *inputDriverName = usePrinterPort
        ? gPrinterInputDriverName
        : gModemInputDriverName;
    const unsigned char *outputDriverName = usePrinterPort
        ? gPrinterOutputDriverName
        : gModemOutputDriverName;
    OSErr error;

    AgentLogPortState(
        "SERIAL_OPEN_BEGIN",
        usePrinterPort,
        baudRate,
        noErr);
    gSerialInputRef = 0;
    gSerialOutputRef = 0;
    error = OpenDriver((ConstStringPtr)inputDriverName, &gSerialInputRef);
    if (error != noErr)
    {
        AgentLogEvent("SERIAL_INPUT_DRIVER_OPEN_ERROR", error);
        return error;
    }
    AgentLogEvent("SERIAL_INPUT_DRIVER_OPEN_ERROR", noErr);

    error = OpenDriver((ConstStringPtr)outputDriverName, &gSerialOutputRef);
    if (error != noErr)
    {
        AgentLogEvent("SERIAL_OUTPUT_DRIVER_OPEN_ERROR", error);
        AgentLogEvent("SERIAL_INPUT_DRIVER_CLOSE_ERROR", CloseDriver(gSerialInputRef));
        gSerialInputRef = 0;
        return error;
    }
    AgentLogEvent("SERIAL_OUTPUT_DRIVER_OPEN_ERROR", noErr);

    error = SerReset(gSerialInputRef, configuration);
    AgentLogEvent("SERIAL_INPUT_RESET_ERROR", error);
    if (error == noErr)
    {
        error = SerReset(gSerialOutputRef, configuration);
        AgentLogEvent("SERIAL_OUTPUT_RESET_ERROR", error);
    }
    if (error == noErr)
    {
        error = SerSetBuf(
            gSerialInputRef,
            (Ptr)gSerialInputBuffer,
            (short)sizeof(gSerialInputBuffer));
        AgentLogEvent("SERIAL_INPUT_BUFFER_SETUP_ERROR", error);
    }
    memset(&handshake, 0, sizeof(handshake));
    if (error == noErr)
    {
        error = SerHShake(gSerialInputRef, &handshake);
        AgentLogEvent("SERIAL_HANDSHAKE_SETUP_ERROR", error);
    }

    if (error != noErr)
    {
        AgentLogEvent("SERIAL_OUTPUT_DRIVER_CLOSE_ERROR", CloseDriver(gSerialOutputRef));
        AgentLogEvent("SERIAL_INPUT_DRIVER_CLOSE_ERROR", CloseDriver(gSerialInputRef));
        gSerialInputRef = 0;
        gSerialOutputRef = 0;
        AgentLogPortState("SERIAL_OPEN_FAILED", usePrinterPort, baudRate, error);
        return error;
    }
    AgentLogPortState("SERIAL_OPEN_SUCCEEDED", usePrinterPort, baudRate, noErr);
    AgentLogSerialStatus(
        usePrinterPort ? "ACTIVE_PRINTER_INPUT" : "ACTIVE_MODEM_INPUT",
        gSerialInputRef,
        true);
    AgentLogSerialStatus(
        usePrinterPort ? "ACTIVE_PRINTER_OUTPUT" : "ACTIVE_MODEM_OUTPUT",
        gSerialOutputRef,
        false);
    return noErr;
}

static OSErr AgentCloseSerialPort(void)
{
    OSErr error = noErr;
    OSErr closeError;

    if (gSerialOutputRef != 0)
    {
        closeError = CloseDriver(gSerialOutputRef);
        AgentLogEvent("SERIAL_OUTPUT_DRIVER_CLOSE_ERROR", closeError);
        if (closeError == noErr)
            gSerialOutputRef = 0;
        else
            error = closeError;
    }
    if (gSerialInputRef != 0)
    {
        closeError = CloseDriver(gSerialInputRef);
        AgentLogEvent("SERIAL_INPUT_DRIVER_CLOSE_ERROR", closeError);
        if (closeError == noErr)
            gSerialInputRef = 0;
        else if (error == noErr)
            error = closeError;
    }
    return error;
}

static void AgentResetSerialSession(void)
{
    gFrameLength = 0;
    gSessionActive = false;
    memset(gSessionId, 0, sizeof(gSessionId));
}

static OSErr AgentChangeSerialSettings(
    Boolean currentUsePrinterPort,
    short currentBaudRate,
    Boolean requestedUsePrinterPort,
    short requestedBaudRate)
{
    OSErr error;
    OSErr restoreError;

    error = AgentCloseSerialPort();
    AgentLogEvent("SERIAL_RECONFIGURE_CLOSE_ERROR", error);
    if (error != noErr)
    {
        if (gSerialInputRef == 0 && gSerialOutputRef == 0)
        {
            restoreError = AgentOpenSerialPort(
                currentUsePrinterPort,
                currentBaudRate);
            AgentLogPortState(
                "SERIAL_RECONFIGURE_RESTORE_AFTER_CLOSE_FAILURE",
                currentUsePrinterPort,
                currentBaudRate,
                restoreError);
            if (restoreError != noErr)
                return restoreError;
        }
        return error;
    }

    error = AgentOpenSerialPort(requestedUsePrinterPort, requestedBaudRate);
    if (error == noErr)
    {
        AgentResetSerialSession();
        AgentLogMessage("SERIAL_SESSION_RESET_AFTER_RECONFIGURATION");
        return noErr;
    }

    restoreError = AgentOpenSerialPort(
        currentUsePrinterPort,
        currentBaudRate);
    AgentLogPortState(
        "SERIAL_RECONFIGURE_RESTORE",
        currentUsePrinterPort,
        currentBaudRate,
        restoreError);
    return restoreError == noErr ? error : restoreError;
}

static OSErr AgentHandleMenu(
    const EventRecord *event,
    Boolean *usePrinterPort,
    short *baudRate)
{
    unsigned long menuChoice;
    short menuId;
    short item;
    OSErr error;

    if (usePrinterPort == NULL || baudRate == NULL)
        return paramErr;
    menuChoice = (unsigned long)MenuSelect(event->where);
    HiliteMenu(0);
    menuId = (short)(menuChoice >> 16);
    item = (short)(menuChoice & 0xFFFF);
    if (menuId == AGENT_BAUD_RATE_MENU_ID)
    {
        short requestedBaudRate;
        short previousBaudRateIndex;
        short requestedBaudRateIndex;

        if (item < 1 || item > AGENT_BAUD_RATE_COUNT)
            return noErr;
        if (gTransferActive || gIncomingFileOpen)
        {
            AgentShowSerialDialog(AGENT_SERIAL_BUSY_DIALOG_ID);
            return noErr;
        }

        requestedBaudRate = gSupportedBaudRates[item - 1];
        if (requestedBaudRate == *baudRate)
            return noErr;
        AgentLogPortState(
            "BAUD_MENU_REQUEST",
            *usePrinterPort,
            requestedBaudRate,
            noErr);
        error = AgentChangeSerialSettings(
            *usePrinterPort,
            *baudRate,
            *usePrinterPort,
            requestedBaudRate);
        if (error != noErr)
        {
            if (gSerialInputRef != 0 && gSerialOutputRef != 0)
            {
                AgentShowSerialDialog(AGENT_SERIAL_RESTORED_DIALOG_ID);
                return noErr;
            }
            AgentShowSerialDialog(AGENT_SERIAL_FATAL_DIALOG_ID);
            return error;
        }

        previousBaudRateIndex = AgentFindBaudRateIndex(*baudRate);
        requestedBaudRateIndex = AgentFindBaudRateIndex(requestedBaudRate);
        if (previousBaudRateIndex >= 0)
            CheckItem(gBaudRateMenu, previousBaudRateIndex + 1, false);
        CheckItem(gBaudRateMenu, requestedBaudRateIndex + 1, true);
        *baudRate = requestedBaudRate;

        error = AgentWriteSerialBaudRate(requestedBaudRate);
        AgentLogEvent("BAUD_SETTING_SAVE_ERROR", error);
        if (error != noErr)
            AgentShowSerialDialog(AGENT_SERIAL_SAVE_DIALOG_ID);
        return noErr;
    }

    if (menuId == AGENT_PORT_MENU_ID)
    {
        Boolean requestedUsePrinterPort;

        if (item < 1 || item > AGENT_PORT_COUNT)
            return noErr;
        if (gTransferActive || gIncomingFileOpen)
        {
            AgentShowSerialDialog(AGENT_SERIAL_BUSY_DIALOG_ID);
            return noErr;
        }

        requestedUsePrinterPort = (Boolean)(item == 2);
        if (requestedUsePrinterPort == *usePrinterPort)
            return noErr;
        AgentLogPortState(
            "PORT_MENU_REQUEST",
            requestedUsePrinterPort,
            *baudRate,
            noErr);
        error = AgentChangeSerialSettings(
            *usePrinterPort,
            *baudRate,
            requestedUsePrinterPort,
            *baudRate);
        if (error != noErr)
        {
            if (gSerialInputRef != 0 && gSerialOutputRef != 0)
            {
                AgentShowSerialDialog(AGENT_PORT_RESTORED_DIALOG_ID);
                return noErr;
            }
            AgentShowSerialDialog(AGENT_SERIAL_FATAL_DIALOG_ID);
            return error;
        }

        CheckItem(gPortMenu, *usePrinterPort ? 2 : 1, false);
        CheckItem(gPortMenu, requestedUsePrinterPort ? 2 : 1, true);
        *usePrinterPort = requestedUsePrinterPort;
        AgentLogPortState(
            "PORT_MENU_CHANGE_SUCCEEDED",
            requestedUsePrinterPort,
            *baudRate,
            noErr);

        error = AgentWriteSerialPortChoice(requestedUsePrinterPort);
        AgentLogEvent("PORT_SETTING_SAVE_ERROR", error);
        if (error != noErr)
            AgentShowSerialDialog(AGENT_SERIAL_SAVE_DIALOG_ID);
        return noErr;
    }

    if (menuId != AGENT_AGENT_MENU_ID)
        return noErr;
    if (gTransferActive || gIncomingFileOpen)
    {
        AgentShowSerialDialog(AGENT_SERIAL_BUSY_DIALOG_ID);
        return noErr;
    }

    if (item == 3)
    {
        AgentLogMessage("AGENT_EXIT_REQUESTED_FROM_MENU");
        gShouldExit = true;
        return noErr;
    }
    if (item == 2)
    {
        AgentDiagnosePorts(*usePrinterPort, *baudRate);
        AgentShowSerialDialog(AGENT_PORT_DIAGNOSTICS_DIALOG_ID);
        return noErr;
    }
    if (item != 1)
        return noErr;

    AgentLogPortState(
        "AGENT_RESTART_REQUESTED",
        *usePrinterPort,
        *baudRate,
        noErr);
    error = AgentChangeSerialSettings(
        *usePrinterPort,
        *baudRate,
        *usePrinterPort,
        *baudRate);
    if (error != noErr)
    {
        if (gSerialInputRef != 0 && gSerialOutputRef != 0)
        {
            AgentShowSerialDialog(AGENT_SERIAL_RESTORED_DIALOG_ID);
            return noErr;
        }
        AgentShowSerialDialog(AGENT_RESTART_FATAL_DIALOG_ID);
        return error;
    }
    AgentLogPortState(
        "AGENT_RESTART_SUCCEEDED",
        *usePrinterPort,
        *baudRate,
        noErr);
    return noErr;
}

int main(void)
{
    EventRecord event;
    Boolean usePrinterPort = false;
    short baudRate = baud9600;
    OSErr error;
    OSErr portConfigError;
    OSErr baudConfigError;

    error = AgentInitializeUserInterface(false, baud9600);
    if (error != noErr)
        return 1;
    error = AgentInitializeStorage();
    if (error != noErr)
    {
        AgentShowSerialDialog(AGENT_STORAGE_ERROR_DIALOG_ID);
        return 1;
    }
    portConfigError = AgentReadSerialPortChoice(&usePrinterPort);
    if (portConfigError != noErr)
        usePrinterPort = false;
    baudConfigError = AgentReadSerialBaudRate(&baudRate);
    if (baudConfigError != noErr)
        baudRate = baud9600;
    CheckItem(gPortMenu, 1, false);
    CheckItem(gPortMenu, usePrinterPort ? 2 : 1, true);
    CheckItem(
        gBaudRateMenu,
        AgentFindBaudRateIndex(baud9600) + 1,
        false);
    CheckItem(
        gBaudRateMenu,
        AgentFindBaudRateIndex(baudRate) + 1,
        true);
    gLastDiagnosticSampleTick = TickCount();
    AgentLogSystemSnapshot();
    AgentLogPortState(
        "STARTUP_CONFIGURATION",
        usePrinterPort,
        baudRate,
        noErr);
    AgentLogEvent("AGENT_PORT_CONFIG_READ_ERROR", portConfigError);
    AgentLogEvent("AGENT_BAUD_CONFIG_READ_ERROR", baudConfigError);
    if (portConfigError != noErr || baudConfigError != noErr)
    {
        AgentLogMessage(
            "INVALID_OR_UNREADABLE_AGENTPORT_OR_AGENTBAUDRATE_CONFIG");
        AgentShowSerialDialog(AGENT_CONFIG_ERROR_DIALOG_ID);
        return 1;
    }

    error = AgentOpenSerialPort(usePrinterPort, baudRate);
    if (error != noErr)
    {
        AgentLogPortState(
            "AGENT_STARTUP_SERIAL_OPEN_FAILED",
            usePrinterPort,
            baudRate,
            error);
        AgentShowSerialDialog(AGENT_SERIAL_FATAL_DIALOG_ID);
        return 1;
    }
    error = AgentInstallDebugHandler();
    if (error != noErr)
    {
        AgentLogEvent("DEBUG_APPLEEVENT_HANDLER_INSTALL_ERROR", error);
        AgentCloseSerialPort();
        return 1;
    }
    error = AgentInstallLaunchHandler();
    if (error != noErr)
    {
        AERemoveEventHandler(
            AGENT_DEBUG_EVENT_CLASS,
            AGENT_DEBUG_EVENT_ID,
            gDebugHandler,
            (Boolean)0);
        DisposeAEEventHandlerUPP(gDebugHandler);
        AgentLogEvent("LAUNCH_APPLEEVENT_HANDLER_INSTALL_ERROR", error);
        AgentCloseSerialPort();
        return 1;
    }
    AgentLogMessage("AGENT_READY; WAITING_FOR_HOST_HELLO");

    while (!gShouldExit)
    {
        error = AgentPollSerial();
        if (error != noErr)
            break;
        WaitNextEvent(everyEvent, &event, 1, (RgnHandle)0);
        if (event.what == mouseDown)
            error = AgentHandleMenu(&event, &usePrinterPort, &baudRate);
        else if (event.what == kHighLevelEvent)
            AEProcessAppleEvent(&event);
        if (error != noErr || gShouldExit)
            break;
        error = AgentSendPendingDebugRecord();
        if (error != noErr)
            break;
        AgentLogPeriodicSerialStatus(usePrinterPort, baudRate);
    }

    AgentLogEvent(
        gShouldExit ? "AGENT_EXITED_FROM_MENU" : "AGENT_LOOP_EXIT_ERROR",
        error);
    AERemoveEventHandler(
        AGENT_DEBUG_EVENT_CLASS,
        AGENT_DEBUG_EVENT_ID,
        gDebugHandler,
        (Boolean)0);
    DisposeAEEventHandlerUPP(gDebugHandler);
    AERemoveEventHandler(
        AGENT_DEBUG_EVENT_CLASS,
        'LNCH',
        gLaunchHandler,
        (Boolean)0);
    DisposeAEEventHandlerUPP(gLaunchHandler);
    if (AgentCloseSerialPort() != noErr)
        error = ioErr;
    return error == noErr ? 0 : 1;
}
