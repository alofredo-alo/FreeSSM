/*
 * J2534_API implemented over the 32-bit broker, for 64-bit builds.
 *
 * This file REPLACES src/windows/J2534_API.cpp when the target is 64-bit. It is a
 * whole separate implementation of the same class rather than a set of #ifdef
 * branches inside the original, because the original has sixteen one-line methods
 * and putting a conditional in each would leave neither version readable.
 *
 * Everything above this class - J2534DiagInterface, the SSM protocol, the GUI - is
 * untouched. The class surface is identical; only the way a call reaches the vendor
 * library changes, from GetProcAddress to a pipe.
 *
 * WHY THIS HAS TO EXIST. op20pt32.dll and many other PassThru libraries on
 * Windows are 32-bit. A 64-bit process cannot load a 32-bit DLL - that is
 * an operating system rule and no rebuild of FreeSSM changes it. So the DLL is
 * loaded by a small 32-bit helper and this relays to it.
 *
 * The registry enumeration is relayed for the same reason and one more: a 64-bit
 * process reading HKLM\SOFTWARE without KEY_WOW64_32KEY does not find
 * PassThruSupport.04.04 at all, because 32-bit libraries register under
 * WOW6432Node. It would list no interfaces and give no reason why. The helper is
 * 32-bit, so it gets the correct view for free.
 */

#include "J2534_API.h"
#include "J2534_broker_client.h"
#include <cstring>

/* One broker per process. J2534_API is constructed once by the interface layer, but
 * a static keeps the helper alive across any transient copies rather than starting
 * and stopping a process each time. */
static J2534BrokerClient &broker()
{
    static J2534BrokerClient instance;
    return instance;
}

static std::vector<unsigned char> msgBytes(const PASSTHRU_MSG *m)
{
    if (!m) return std::vector<unsigned char>();
    unsigned long n = m->DataSize;
    if (n > sizeof(m->Data)) n = sizeof(m->Data);
    return std::vector<unsigned char>(m->Data, m->Data + n);
}

J2534_API::J2534_API()
{
    _J2534LIB = NULL;
    _api_version = J2534_API_version::v0404;   /* the broker only speaks 04.04 */
    _lib_path.clear();
    _last_error.clear();
}

J2534_API::~J2534_API()
{
    broker().stop();
}

std::vector<J2534Library> J2534_API::getAvailableJ2534Libs()
{
    std::vector<J2534Library> libs;
    if (!broker().start()) return libs;
    std::vector<BrokerLib> found = broker().list();
    for (size_t i = 0; i < found.size(); i++)
    {
        J2534Library l;
        l.name = found[i].name;
        l.path = found[i].path;
        /* J2534Library has no api_version member - the first cut of this assumed it
         * did. What it has is protocols, and FreeSSM uses it to decide which
         * transports the interface offers, so an empty mask reads as "supports
         * nothing" and the interface is silently useless. */
        l.protocols = static_cast<J2534_protocol_flags>(found[i].protocols);
        l.api = J2534_API_version::v0404;
        l.architecture = J2534_library_architecture::x86;
        /* A 64-bit FreeSSM reaches this 32-bit DLL through the broker. */
        l.compatibleWithApplication = true;
        libs.push_back(l);
    }
    return libs;
}

bool J2534_API::selectLibrary(std::string libPath)
{
    _last_error.clear();
    if (!libPath.size())
    {
        _last_error = "No J2534 library path was supplied.";
        return false;
    }
    if (!broker().start())
    {
        _last_error = broker().lastError();
        return false;
    }
    if (!broker().load(libPath))
    {
        _last_error = broker().lastError();
        return false;
    }
    _lib_path = libPath;
    _api_version = J2534_API_version::v0404;
    /* Non-null so every method's "is a library loaded" guard behaves as before.
     * It is never dereferenced on this path. */
    _J2534LIB = reinterpret_cast<HINSTANCE>(1);
    return true;
}

std::string J2534_API::library() { return _lib_path; }

J2534_API_version J2534_API::libraryAPIversion() { return _api_version; }

std::string J2534_API::lastError()
{
    return _last_error.empty() ? broker().lastError() : _last_error;
}

long J2534_API::PassThruOpen(void *pName, unsigned long *pDeviceID)
{
    (void)pName;
    if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
    return broker().open(pDeviceID);
}

long J2534_API::PassThruClose(unsigned long DeviceID)
{
    if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
    return broker().close(DeviceID);
}

long J2534_API::PassThruConnect(unsigned long ProtocolID, unsigned long Flags,
                                unsigned long *pChannelID)
{
    /* 02.02 signature. The helper always drives an 04.04 library, so this exists
     * only to satisfy the interface and reports the same "not supported" the
     * original does when the loaded library is the other vintage. */
    (void)ProtocolID; (void)Flags; (void)pChannelID;
    return J2534API_ERROR_FCN_NOT_SUPPORTED;
}

long J2534_API::PassThruConnect(unsigned long DeviceID, unsigned long ProtocolID,
                                unsigned long Flags, unsigned long BaudRate,
                                unsigned long *pChannelID)
{
    (void)DeviceID;
    if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
    return broker().connect(ProtocolID, Flags, BaudRate, pChannelID);
}

long J2534_API::PassThruDisconnect(unsigned long ChannelID)
{
    if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
    return broker().disconnect(ChannelID);
}

long J2534_API::PassThruReadVersion(char *pFirmwareVersion, char *pDllVersion,
                                    char *pApiVersion)
{
    (void)pFirmwareVersion; (void)pDllVersion; (void)pApiVersion;
    return J2534API_ERROR_FCN_NOT_SUPPORTED;   /* 02.02 only */
}

long J2534_API::PassThruReadVersion(unsigned long DeviceID, char *pFirmwareVersion,
                                    char *pDllVersion, char *pApiVersion)
{
    (void)DeviceID;
    if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
    std::string fw, dll, api;
    long r = broker().readVersion(&fw, &dll, &api);
    if (r != 0) return r;
    /* The caller supplies 80-byte buffers, as the specification requires. */
    if (pFirmwareVersion) { strncpy(pFirmwareVersion, fw.c_str(), 79); pFirmwareVersion[79] = 0; }
    if (pDllVersion)      { strncpy(pDllVersion, dll.c_str(), 79);     pDllVersion[79] = 0; }
    if (pApiVersion)      { strncpy(pApiVersion, api.c_str(), 79);     pApiVersion[79] = 0; }
    return 0;
}

long J2534_API::PassThruGetLastError(char *pErrorDescription)
{
    if (!pErrorDescription) return J2534API_ERROR_INVALID_LIBRARY;
    std::string e = broker().lastError();
    strncpy(pErrorDescription, e.c_str(), 79);
    pErrorDescription[79] = 0;
    return 0;
}

long J2534_API::PassThruReadMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg,
                                 unsigned long *pNumMsgs, unsigned long Timeout)
{
    (void)ChannelID;
    if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
    if (!pMsg || !pNumMsgs || *pNumMsgs == 0) return J2534API_ERROR_INVALID_LIBRARY;

    std::vector<unsigned char> data;
    unsigned long rxStatus = 0, extraDataIndex = 0, timestamp = 0;
    long r = broker().readMsg(&data, Timeout, &rxStatus, &extraDataIndex, &timestamp);
    if (r != 0) { *pNumMsgs = 0; return r; }
    if (data.empty()) { *pNumMsgs = 0; return 0; }   /* nothing arrived: not an error */

    memset(pMsg, 0, sizeof(PASSTHRU_MSG));
    unsigned long n = (unsigned long)data.size();
    if (n > sizeof(pMsg->Data)) n = sizeof(pMsg->Data);
    memcpy(pMsg->Data, &data[0], n);
    pMsg->DataSize = n;
    /* Without these the caller cannot tell a reply from its own echo, and does not
     * know where the payload ends - so it reads values from the wrong offset. */
    pMsg->RxStatus = rxStatus;
    pMsg->ExtraDataIndex = extraDataIndex;
    pMsg->Timestamp = timestamp;
    *pNumMsgs = 1;    /* the relay carries one message per call */
    return 0;
}

long J2534_API::PassThruStartMsgFilter(unsigned long ChannelID, unsigned long FilterType,
                                       PASSTHRU_MSG *pMaskMsg, PASSTHRU_MSG *pPatternMsg,
                                       PASSTHRU_MSG *pFlowControlMsg, unsigned long *pMsgID)
{
    (void)ChannelID;
    if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
    return broker().startMsgFilter(FilterType, msgBytes(pMaskMsg), msgBytes(pPatternMsg),
                                   msgBytes(pFlowControlMsg), pMsgID);
}

long J2534_API::PassThruStopMsgFilter(unsigned long ChannelID, unsigned long MsgID)
{
    (void)ChannelID;
    if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
    return broker().stopMsgFilter(MsgID);
}

long J2534_API::PassThruWriteMsgs(unsigned long ChannelID, PASSTHRU_MSG *pMsg,
                                  unsigned long *pNumMsgs, unsigned long Timeout)
{
    (void)ChannelID; (void)Timeout;
    if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;
    if (!pMsg || !pNumMsgs) return J2534API_ERROR_INVALID_LIBRARY;

    unsigned long wanted = *pNumMsgs, sent = 0;
    for (unsigned long i = 0; i < wanted; i++)
    {
        long r = broker().writeMsg(msgBytes(&pMsg[i]), pMsg[i].TxFlags);
        if (r != 0) { *pNumMsgs = sent; return r; }
        sent++;
    }
    *pNumMsgs = sent;
    return 0;
}

long J2534_API::PassThruStartPeriodicMsg(unsigned long ChannelID, PASSTHRU_MSG *pMsg,
                                         unsigned long *pMsgID, unsigned long TimeInterval)
{
    /* Not relayed. FreeSSM does not use periodic messages for SSM, and a periodic
     * message driven across a pipe would not keep its timing anyway - better to
     * refuse than to send something late and call it success. */
    (void)ChannelID; (void)pMsg; (void)pMsgID; (void)TimeInterval;
    return J2534API_ERROR_FCN_NOT_SUPPORTED;
}

long J2534_API::PassThruStopPeriodicMsg(unsigned long ChannelID, unsigned long MsgID)
{
    (void)ChannelID; (void)MsgID;
    return J2534API_ERROR_FCN_NOT_SUPPORTED;
}

long J2534_API::PassThruIoctl(unsigned long ChannelID, unsigned long IoctlID,
                              void *pInput, void *pOutput)
{
    (void)ChannelID;
    if (!_J2534LIB) return J2534API_ERROR_INVALID_LIBRARY;

    std::vector<std::pair<unsigned long, unsigned long> > cfg;
    if (pInput && (IoctlID == SET_CONFIG || IoctlID == GET_CONFIG))
    {
        SCONFIG_LIST *list = reinterpret_cast<SCONFIG_LIST *>(pInput);
        for (unsigned long i = 0; i < list->NumOfParams; i++)
            cfg.push_back(std::make_pair(list->ConfigPtr[i].Parameter,
                                         list->ConfigPtr[i].Value));
    }
    unsigned long out = 0;
    long r = broker().ioctl(IoctlID, cfg, pOutput ? &out : NULL);
    if (r == 0 && pOutput) *reinterpret_cast<unsigned long *>(pOutput) = out;
    return r;
}

long J2534_API::PassThruSetProgrammingVoltage(unsigned long PinNumber, unsigned long Voltage)
{
    (void)PinNumber; (void)Voltage;
    return J2534API_ERROR_FCN_NOT_SUPPORTED;
}

long J2534_API::PassThruSetProgrammingVoltage(unsigned long DeviceID, unsigned long PinNumber,
                                              unsigned long Voltage)
{
    /* Deliberately not relayed. Programming voltage drives real volts onto a pin of
     * a connected controller; it is not needed for SSM and is not something to
     * expose through a relay that has had no testing at doing it. */
    (void)DeviceID; (void)PinNumber; (void)Voltage;
    return J2534API_ERROR_FCN_NOT_SUPPORTED;
}
