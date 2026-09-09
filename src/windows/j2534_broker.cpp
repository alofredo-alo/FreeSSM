// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 TomFLV
//
// A 32-bit J2534 broker, so a 64-bit application can use a 32-bit PassThru library.
//
// THE PROBLEM THIS EXISTS FOR. Many J2534 libraries on Windows are 32-bit - the
// Tactrix OpenPort's op20pt32.dll among them - and a 64-bit process
// cannot load a 32-bit DLL. That is an operating system rule, not a limitation of
// any application, and no amount of rebuilding FreeSSM changes it. The DLL has to be
// loaded by a 32-bit process, so this is that process: it does nothing but load the
// vendor library and relay calls.
//
// Communication is over stdin and stdout, one line per request and per reply. That
// choice is deliberate over a socket: no port to pick, nothing for a firewall to
// block, no listener exposed to the network, and the operating system reaps the
// broker through pipe closure when its parent exits. The Windows client additionally
// uses a kill-on-close job object where the parent process permits it.
//
// It is not Tactrix-specific. It loads whatever library path it is given, so any
// registered J2534 interface works.
//
//   i686-w64-mingw32-g++ -O2 -static -o j2534_broker.exe j2534_broker.cpp
//
// PROTOCOL - request lines, replies are OK/ERR/DATA
//
//   LIST                                 registered libraries, from the 32-bit view
//                                        each row: name <tab> path <tab> protocol mask
//   LOAD <path>                          load a PassThru DLL
//   OPEN                                 PassThruOpen
//   VBATT                                battery millivolts at the connector
//   CONNECT <protocol> <flags> <baud>    PassThruConnect
//   FILTER <type> <maskhex> <pathex> [<fchex>]
//   WRITE <hex> [txflags]                PassThruWriteMsgs, one message
//   READ <timeout_ms>                    PassThruReadMsgs, one message
//   IOCTL <id> [<param>:<value> ...]     SET_CONFIG when pairs are given, else
//                                        a null-input ioctl such as CLEAR_RX_BUFFER
//   STOPFILTER <id>                      PassThruStopMsgFilter
//   VERSION                              firmware / DLL / API strings
//   DISCONNECT / CLOSE / QUIT

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#define PT_MSG_DATA 4128
#define ERR_TIMEOUT 0x09
#define ERR_INVALID_MSG 0x0A
#define ERR_BUFFER_EMPTY 0x10
#define ERR_FAILED 0x07

typedef struct {
    unsigned long ProtocolID, RxStatus, TxFlags, Timestamp, DataSize, ExtraDataIndex;
    unsigned char Data[PT_MSG_DATA];
} PASSTHRU_MSG;

typedef long (WINAPI *fnOpen)(const void*, unsigned long*);
typedef long (WINAPI *fnClose)(unsigned long);
typedef long (WINAPI *fnConnect)(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long*);
typedef long (WINAPI *fnDisconnect)(unsigned long);
typedef long (WINAPI *fnReadMsgs)(unsigned long, PASSTHRU_MSG*, unsigned long*, unsigned long);
typedef long (WINAPI *fnWriteMsgs)(unsigned long, PASSTHRU_MSG*, unsigned long*, unsigned long);
typedef long (WINAPI *fnStartMsgFilter)(unsigned long, unsigned long, PASSTHRU_MSG*, PASSTHRU_MSG*, PASSTHRU_MSG*, unsigned long*);
typedef long (WINAPI *fnIoctl)(unsigned long, unsigned long, void*, void*);
typedef long (WINAPI *fnGetLastError)(char*);
typedef long (WINAPI *fnStopMsgFilter)(unsigned long, unsigned long);
typedef long (WINAPI *fnReadVersion)(unsigned long, char*, char*, char*);

/* SET_CONFIG carries a list of parameter/value pairs. That is the only ioctl input
 * shape FreeSSM uses - LOOPBACK, DATA_RATE, P1_MAX, P3_MIN and the rest - and it
 * relays as text without having to marshal an arbitrary struct across the pipe. */
typedef struct { unsigned long Parameter, Value; } SCONFIG;
typedef struct { unsigned long NumOfParams; SCONFIG *ConfigPtr; } SCONFIG_LIST;

static HMODULE g_lib = NULL;
static fnOpen pOpen; static fnClose pClose; static fnConnect pConnect;
static fnDisconnect pDisconnect; static fnReadMsgs pRead; static fnWriteMsgs pWrite;
static fnStartMsgFilter pFilter; static fnIoctl pIoctl; static fnGetLastError pErr;
static fnStopMsgFilter pStopFilter; static fnReadVersion pVersion;
static unsigned long g_dev = 0, g_chan = 0;
/* The protocol the channel was connected with. Every message on a channel has
 * to carry it; a zero ProtocolID is rejected with ERR_INVALID_PROTOCOL_ID. */
static unsigned long g_prot = 0;

template<typename T>
static T resolve(HMODULE library, const char *name)
{
    FARPROC raw = GetProcAddress(library, name);
    T result = NULL;
    static_assert(sizeof(result) == sizeof(raw), "unexpected Windows function pointer size");
    memcpy(&result, &raw, sizeof(result));
    return result;
}

static std::string lastError()
{
    char buf[512] = {0};
    if (pErr) pErr(buf);
    buf[sizeof(buf) - 1] = 0;
    std::string message = buf[0] ? buf : "no description";
    for (size_t i = 0; i < message.size(); i++)
        if (message[i] == '\r' || message[i] == '\n' || message[i] == '\t')
            message[i] = ' ';
    return message;
}

static std::string singleField(std::string value)
{
    for (size_t i = 0; i < value.size(); i++)
        if (value[i] == '\r' || value[i] == '\n' || value[i] == '\t')
            value[i] = ' ';
    return value;
}

static void ok(const std::string &s = "") { std::cout << "OK" << (s.empty() ? "" : " ") << s << "\n" << std::flush; }
static void err(long code, const std::string &what)
{
    std::cout << "ERR " << code << " " << what << " (" << lastError() << ")\n" << std::flush;
}

static std::string toHex(const unsigned char *d, size_t n)
{
    static const char *H = "0123456789ABCDEF";
    std::string s;
    for (size_t i = 0; i < n; i++) { s += H[d[i] >> 4]; s += H[d[i] & 0xF]; }
    return s;
}

static std::vector<unsigned char> fromHex(const std::string &s)
{
    std::vector<unsigned char> v;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((unsigned char)strtoul(s.substr(i, 2).c_str(), NULL, 16));
    return v;
}

static std::string registryString(HKEY key, const char *name)
{
    DWORD type = REG_NONE, size = 0;
    if (RegQueryValueExA(key, name, NULL, &type, NULL, &size) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) || size == 0)
        return std::string();

    std::vector<char> value(size + 1, '\0');
    if (RegQueryValueExA(key, name, NULL, &type,
                         reinterpret_cast<LPBYTE>(&value[0]), &size) != ERROR_SUCCESS)
        return std::string();
    value[value.size() - 1] = '\0';
    if (type != REG_EXPAND_SZ)
        return std::string(&value[0]);

    const DWORD expandedSize = ExpandEnvironmentStringsA(&value[0], NULL, 0);
    if (!expandedSize)
        return std::string(&value[0]);
    std::vector<char> expanded(expandedSize, '\0');
    return ExpandEnvironmentStringsA(&value[0], &expanded[0], expandedSize)
            ? std::string(&expanded[0]) : std::string(&value[0]);
}

static bool registryDwordEnabled(HKEY key, const char *name)
{
    DWORD type = REG_NONE, value = 0, size = sizeof(value);
    return RegQueryValueExA(key, name, NULL, &type,
                            reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS &&
           type == REG_DWORD && size == sizeof(value) && value != 0;
}

// Enumerate registered interfaces. Reading HKLM\SOFTWARE from a 32-bit process is
// redirected to WOW6432Node automatically, which is exactly where 32-bit PassThru
// libraries register - and precisely what a 64-bit process cannot see without
// asking for the 32-bit view explicitly. Being 32-bit, this gets it for free.
static void cmdList()
{
    HKEY root;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\PassThruSupport.04.04", 0,
                      KEY_READ | KEY_WOW64_32KEY, &root) != ERROR_SUCCESS) {
        std::cout << "OK 0\n" << std::flush;
        return;
    }

    DWORD subKeyCount = 0, maxSubKeyLength = 0;
    if (RegQueryInfoKeyA(root, NULL, NULL, NULL, &subKeyCount, &maxSubKeyLength,
                         NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
        RegCloseKey(root);
        std::cout << "OK 0\n" << std::flush;
        return;
    }
    std::vector<std::string> lines;
    std::vector<char> keyName(maxSubKeyLength + 2, '\0');
    for (DWORD i = 0; i < subKeyCount; ++i) {
        DWORD len = static_cast<DWORD>(keyName.size() - 1);
        if (RegEnumKeyExA(root, i, &keyName[0], &len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
            continue;
        keyName[len] = '\0';
        HKEY sub;
        if (RegOpenKeyExA(root, &keyName[0], 0, KEY_READ | KEY_WOW64_32KEY, &sub)
                != ERROR_SUCCESS)
            continue;
        const std::string vendor = registryString(sub, "Name");
        const std::string path = registryString(sub, "FunctionLibrary");

        /* Each protocol is a DWORD under the same key. The bit values match
         * J2534_protocol_flags exactly, so neither end has to translate. */
        static const struct { const char *value; unsigned int bit; } PROTO[] = {
            { "J1850VPW", 0x01 }, { "J1850PWM", 0x02 }, { "ISO9141", 0x04 },
            { "ISO14230", 0x08 }, { "CAN", 0x10 }, { "ISO15765", 0x20 },
            { "SCI_A_ENGINE", 0x40 }, { "SCI_A_TRANS", 0x80 },
            { "SCI_B_ENGINE", 0x100 }, { "SCI_B_TRANS", 0x200 },
        };
        unsigned int mask = 0;
        for (size_t q = 0; q < sizeof(PROTO) / sizeof(PROTO[0]); q++)
            if (registryDwordEnabled(sub, PROTO[q].value))
                mask |= PROTO[q].bit;
        RegCloseKey(sub);
        if (!path.empty()) {
            std::ostringstream row;
            row << singleField(vendor.empty() ? std::string(&keyName[0]) : vendor)
                << "\t" << singleField(path) << "\t" << mask;
            lines.push_back(row.str());
        }
    }
    RegCloseKey(root);
    std::cout << "OK " << lines.size() << "\n";
    for (size_t k = 0; k < lines.size(); k++) std::cout << lines[k] << "\n";
    std::cout << std::flush;
}

static bool bind(const std::string &path)
{
    HMODULE candidate = LoadLibraryA(path.c_str());
    if (!candidate) return false;
    g_lib = candidate;
    pOpen = resolve<fnOpen>(g_lib, "PassThruOpen");
    pClose = resolve<fnClose>(g_lib, "PassThruClose");
    pConnect = resolve<fnConnect>(g_lib, "PassThruConnect");
    pDisconnect = resolve<fnDisconnect>(g_lib, "PassThruDisconnect");
    pRead = resolve<fnReadMsgs>(g_lib, "PassThruReadMsgs");
    pWrite = resolve<fnWriteMsgs>(g_lib, "PassThruWriteMsgs");
    pFilter = resolve<fnStartMsgFilter>(g_lib, "PassThruStartMsgFilter");
    pIoctl = resolve<fnIoctl>(g_lib, "PassThruIoctl");
    pErr = resolve<fnGetLastError>(g_lib, "PassThruGetLastError");
    pStopFilter = resolve<fnStopMsgFilter>(g_lib, "PassThruStopMsgFilter");
    pVersion = resolve<fnReadVersion>(g_lib, "PassThruReadVersion");
    const bool complete = pOpen && pClose && pConnect && pDisconnect && pRead &&
                          pWrite && pFilter && pIoctl && pErr && pStopFilter && pVersion;
    if (!complete) {
        FreeLibrary(g_lib);
        g_lib = NULL;
    }
    return complete;
}

int main()
{
    // Unbuffered, or a reply can sit in a pipe buffer while the caller waits for it
    // and times out - which reads as the interface being unresponsive.
    setvbuf(stdout, NULL, _IONBF, 0);
    std::cout << "READY j2534-broker " << (int)(sizeof(void*) * 8) << "-bit\n" << std::flush;

    std::string line;
    while (std::getline(std::cin, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        if (cmd.empty()) continue;

        if (cmd == "QUIT") break;
        if (cmd == "LIST") { cmdList(); continue; }

        if (cmd == "LOAD") {
            std::string path;
            std::getline(in >> std::ws, path);
            if (!bind(path)) { std::cout << "ERR " << ERR_FAILED << " LoadLibrary/GetProcAddress failed for " << path << "\n" << std::flush; continue; }
            ok(path);
            continue;
        }
        if (!g_lib) { std::cout << "ERR " << ERR_FAILED << " no library loaded\n" << std::flush; continue; }

        if (cmd == "OPEN") {
            long r = pOpen(NULL, &g_dev);
            if (r) err(r, "PassThruOpen"); else { std::ostringstream o; o << g_dev; ok(o.str()); }
        }
        else if (cmd == "VBATT") {
            // READ_VBATT is ioctl 3 on the DEVICE handle, with an OUTPUT pointer and
            // no input. Passing an input pointer instead faults inside the vendor
            // library rather than returning an error.
            unsigned long mv = 0;
            long r = pIoctl ? pIoctl(g_dev, 3, NULL, &mv) : -1;
            if (r) err(r, "READ_VBATT"); else { std::ostringstream o; o << mv; ok(o.str()); }
        }
        else if (cmd == "CONNECT") {
            unsigned long prot = 0, flags = 0, baud = 0;
            in >> prot >> flags >> baud;
            long r = pConnect(g_dev, prot, flags, baud, &g_chan);
            if (!r) g_prot = prot;
            if (r) err(r, "PassThruConnect"); else { std::ostringstream o; o << g_chan; ok(o.str()); }
        }
        else if (cmd == "FILTER") {
            unsigned long type = 0;
            std::string mh, ph, fh;
            in >> type >> mh >> ph;
            in >> fh;
            PASSTHRU_MSG m, p, f;
            memset(&m, 0, sizeof m); memset(&p, 0, sizeof p); memset(&f, 0, sizeof f);
            std::vector<unsigned char> mv = fromHex(mh), pv = fromHex(ph), fv = fromHex(fh);
            if (mv.size() > PT_MSG_DATA || pv.size() > PT_MSG_DATA || fv.size() > PT_MSG_DATA) {
                err(ERR_INVALID_MSG, "PassThruStartMsgFilter payload too large");
                continue;
            }
            m.ProtocolID = p.ProtocolID = f.ProtocolID = g_prot;
            memcpy(m.Data, mv.data(), mv.size()); m.DataSize = (unsigned long)mv.size();
            memcpy(p.Data, pv.data(), pv.size()); p.DataSize = (unsigned long)pv.size();
            memcpy(f.Data, fv.data(), fv.size()); f.DataSize = (unsigned long)fv.size();
            unsigned long fid = 0;
            long r = pFilter(g_chan, type, &m, &p, fv.empty() ? NULL : &f, &fid);
            if (r) err(r, "PassThruStartMsgFilter"); else { std::ostringstream o; o << fid; ok(o.str()); }
        }
        else if (cmd == "WRITE") {
            std::string hex; unsigned long tx = 0;
            in >> hex; in >> tx;
            PASSTHRU_MSG m; memset(&m, 0, sizeof m);
            std::vector<unsigned char> d = fromHex(hex);
            if (d.size() > PT_MSG_DATA) {
                err(ERR_INVALID_MSG, "PassThruWriteMsgs payload too large");
                continue;
            }
            m.ProtocolID = g_prot;
            m.TxFlags = tx;
            memcpy(m.Data, d.data(), d.size());
            m.DataSize = (unsigned long)d.size();
            unsigned long n = 1;
            long r = pWrite(g_chan, &m, &n, 1000);
            if (r) err(r, "PassThruWriteMsgs"); else ok();
        }
        else if (cmd == "READ") {
            unsigned long timeout = 500;
            in >> timeout;
            PASSTHRU_MSG m; memset(&m, 0, sizeof m);
            m.ProtocolID = g_prot;
            unsigned long n = 1;
            long r = pRead(g_chan, &m, &n, timeout);
            if (r == ERR_TIMEOUT || r == ERR_BUFFER_EMPTY || n == 0) {
                std::cout << "DATA\n" << std::flush;
            }
            else if (r) {
                err(r, "PassThruReadMsgs");
            }
            else {
                /* RxStatus and ExtraDataIndex are not decoration: the first says
                 * whether this is a real reply or our own echo, the second where
                 * the payload ends. Dropping them made every value read from the
                 * wrong offset. */
                const size_t dataSize = m.DataSize > PT_MSG_DATA ? PT_MSG_DATA : m.DataSize;
                std::cout << "DATA " << toHex(m.Data, dataSize)
                          << " " << m.RxStatus
                          << " " << m.ExtraDataIndex
                          << " " << m.Timestamp << "\n" << std::flush;
            }
        }
        else if (cmd == "IOCTL") {
            unsigned long id = 0;
            in >> id;
            std::vector<SCONFIG> cfg;
            std::string pair;
            while (in >> pair) {
                size_t c = pair.find(':');
                if (c == std::string::npos) continue;
                SCONFIG sc;
                sc.Parameter = strtoul(pair.substr(0, c).c_str(), NULL, 0);
                sc.Value = strtoul(pair.substr(c + 1).c_str(), NULL, 0);
                cfg.push_back(sc);
            }
            long r;
            if (!cfg.empty()) {
                SCONFIG_LIST list;
                list.NumOfParams = (unsigned long)cfg.size();
                list.ConfigPtr = &cfg[0];
                r = pIoctl(g_chan, id, &list, NULL);
                if (r) err(r, "PassThruIoctl SET_CONFIG"); else ok();
            } else if (id == 3) {
                /* READ_VBATT is the one ioctl taken on the DEVICE handle, with an
                 * output pointer and no input. Handing it an input pointer faults
                 * inside the vendor library instead of returning an error. */
                unsigned long mv = 0;
                r = pIoctl(g_dev, 3, NULL, &mv);
                if (r) err(r, "READ_VBATT"); else { std::ostringstream o; o << mv; ok(o.str()); }
            } else {
                r = pIoctl(g_chan, id, NULL, NULL);
                if (r) err(r, "PassThruIoctl"); else ok();
            }
        }
        else if (cmd == "STOPFILTER") {
            unsigned long fid = 0;
            in >> fid;
            long r = pStopFilter ? pStopFilter(g_chan, fid) : -1;
            if (r) err(r, "PassThruStopMsgFilter"); else ok();
        }
        else if (cmd == "VERSION") {
            char fw[128] = {0}, dll[128] = {0}, api[128] = {0};
            long r = pVersion ? pVersion(g_dev, fw, dll, api) : -1;
            if (r) err(r, "PassThruReadVersion");
            else {
                fw[sizeof(fw) - 1] = 0;
                dll[sizeof(dll) - 1] = 0;
                api[sizeof(api) - 1] = 0;
                ok(singleField(fw) + "	" + singleField(dll) + "	" + singleField(api));
            }
        }
        else if (cmd == "DISCONNECT") { long r = g_chan ? pDisconnect(g_chan) : 0; g_chan = 0; g_prot = 0; if (r) err(r, "PassThruDisconnect"); else ok(); }
        else if (cmd == "CLOSE") { long r = g_dev ? pClose(g_dev) : 0; g_dev = 0; if (r) err(r, "PassThruClose"); else ok(); }
        else std::cout << "ERR " << ERR_FAILED << " unknown command " << cmd << "\n" << std::flush;
    }
    if (g_chan && pDisconnect) pDisconnect(g_chan);
    if (g_dev && pClose) pClose(g_dev);
    return 0;
}
