#include "J2534_broker_client.h"
#include "../J2534misc.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>

static const char *HEXD = "0123456789ABCDEF";

static std::string toHex(const std::vector<unsigned char> &v)
{
    std::string s;
    for (size_t i = 0; i < v.size(); i++) { s += HEXD[v[i] >> 4]; s += HEXD[v[i] & 0xF]; }
    return s;
}

static std::vector<unsigned char> fromHex(const std::string &s)
{
    std::vector<unsigned char> v;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((unsigned char)strtoul(s.substr(i, 2).c_str(), NULL, 16));
    return v;
}

J2534BrokerClient::J2534BrokerClient()
    : _proc(NULL), _job(NULL), _inWr(NULL), _outRd(NULL) {}

J2534BrokerClient::~J2534BrokerClient() { stop(); }

bool J2534BrokerClient::start(const std::string &brokerPath)
{
    if (_proc) return true;
    _lastError.clear();
    _pending.clear();

    std::string exe = brokerPath;
    if (exe.empty()) {
        /* Beside the application, not on PATH: a helper found on PATH could be
         * anything, and this one is handed the address of every subsequent call. */
        char self[MAX_PATH] = {0};
        GetModuleFileNameA(NULL, self, MAX_PATH);
        std::string dir(self);
        size_t slash = dir.find_last_of("\\/");
        dir = (slash == std::string::npos) ? std::string(".") : dir.substr(0, slash);
        exe = dir + "\\j2534_broker.exe";
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE inRd = NULL, outWr = NULL;
    if (!CreatePipe(&inRd, &_inWr, &sa, 0)) {
        _lastError = "CreatePipe failed";
        return false;
    }
    if (!CreatePipe(&_outRd, &outWr, &sa, 0)) {
        _lastError = "CreatePipe failed";
        CloseHandle(inRd);
        CloseHandle(_inWr);
        _inWr = NULL;
        return false;
    }
    /* Our ends must not be inherited, or the child holds a copy of the write handle
     * and the pipe never reports end-of-file when it exits - a read then blocks for
     * ever instead of failing. */
    if (!SetHandleInformation(_inWr, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(_outRd, HANDLE_FLAG_INHERIT, 0)) {
        _lastError = "SetHandleInformation failed";
        CloseHandle(inRd); CloseHandle(outWr);
        CloseHandle(_inWr); CloseHandle(_outRd);
        _inWr = _outRd = NULL;
        return false;
    }

    STARTUPINFOA si;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;      /* no console window flashing up */
    si.hStdInput = inRd;
    si.hStdOutput = outWr;
    HANDLE nullErr = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (nullErr == INVALID_HANDLE_VALUE) {
        _lastError = "cannot open NUL for the J2534 broker stderr";
        CloseHandle(inRd); CloseHandle(outWr);
        CloseHandle(_inWr); CloseHandle(_outRd);
        _inWr = _outRd = NULL;
        return false;
    }
    si.hStdError = nullErr;

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof pi);
    std::vector<char> cmd(exe.begin(), exe.end());
    cmd.push_back('\0');

    /* lpApplicationName is explicit: an unquoted path under Program Files must
     * never be reparsed as C:\\Program.exe. */
    BOOL okStart = CreateProcessA(exe.c_str(), &cmd[0], NULL, NULL, TRUE,
                                  CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, NULL, &si, &pi);
    CloseHandle(inRd);
    CloseHandle(outWr);
    CloseHandle(nullErr);
    if (!okStart) {
        _lastError = "cannot start the 32-bit broker: " + exe;
        CloseHandle(_inWr); CloseHandle(_outRd);
        _inWr = _outRd = NULL;
        return false;
    }
    _proc = pi.hProcess;

    /* KILL_ON_JOB_CLOSE covers a parent crash while the helper is blocked inside
     * a vendor DLL. Assignment can fail if the parent is inside a restrictive job
     * on older Windows; pipe EOF remains the fallback in that case. */
    HANDLE job = CreateJobObjectA(NULL, NULL);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
        memset(&limits, 0, sizeof limits);
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                    &limits, sizeof limits) &&
            AssignProcessToJobObject(job, _proc))
            _job = job;
        else
            CloseHandle(job);
    }
    if (ResumeThread(pi.hThread) == (DWORD)-1) {
        _lastError = "cannot resume the J2534 broker process";
        TerminateProcess(_proc, 1);
        CloseHandle(pi.hThread);
        CloseHandle(_proc); _proc = NULL;
        if (_job) { CloseHandle(_job); _job = NULL; }
        CloseHandle(_inWr); CloseHandle(_outRd);
        _inWr = _outRd = NULL;
        return false;
    }
    CloseHandle(pi.hThread);

    std::string banner;
    if (!recv(&banner)) {
        const std::string receiveError = _lastError;
        stop();
        _lastError = receiveError;
        return false;
    }
    if (banner.compare(0, 5, "READY") != 0 ||
        banner.find(" 32-bit") == std::string::npos) {
        const std::string bannerError = "broker did not announce a 32-bit implementation: " + banner;
        stop();
        _lastError = bannerError;
        return false;
    }
    return true;
}

void J2534BrokerClient::stop()
{
    if (_inWr) { send("QUIT"); CloseHandle(_inWr); _inWr = NULL; }
    if (_outRd) { CloseHandle(_outRd); _outRd = NULL; }
    if (_proc) {
        /* Give it a moment to leave on its own; the vendor library can be slow to
         * release the device, and killing it there leaves the interface claimed. */
        if (WaitForSingleObject(_proc, 3000) == WAIT_TIMEOUT)
            TerminateProcess(_proc, 0);
        CloseHandle(_proc);
        _proc = NULL;
    }
    if (_job) { CloseHandle(_job); _job = NULL; }
}

bool J2534BrokerClient::send(const std::string &line)
{
    if (!_inWr) {
        _lastError = "broker input pipe is not open";
        return false;
    }
    std::string out = line + "\n";
    DWORD written = 0;
    if (!WriteFile(_inWr, out.data(), (DWORD)out.size(), &written, NULL) ||
        written != out.size()) {
        _lastError = "cannot send a command to the J2534 broker";
        return false;
    }
    return true;
}

bool J2534BrokerClient::recv(std::string *line, DWORD timeoutMs)
{
    if (!_outRd) {
        _lastError = "broker output pipe is not open";
        return false;
    }
    const DWORD started = GetTickCount();
    for (;;) {
        size_t nl = _pending.find('\n');
        if (nl != std::string::npos) {
            *line = _pending.substr(0, nl);
            _pending.erase(0, nl + 1);
            while (!line->empty() && line->at(line->size() - 1) == '\r')
                line->erase(line->size() - 1);
            return true;
        }

        DWORD available = 0;
        if (!PeekNamedPipe(_outRd, NULL, 0, NULL, &available, NULL)) {
            _lastError = "cannot read from the J2534 broker pipe";
            return false;
        }
        if (available == 0) {
            if (_proc && WaitForSingleObject(_proc, 0) == WAIT_OBJECT_0) {
                _lastError = "the J2534 broker process exited unexpectedly";
                return false;
            }
            /* DWORD subtraction intentionally handles the GetTickCount wrap. */
            if (timeoutMs != INFINITE && (DWORD)(GetTickCount() - started) >= timeoutMs) {
                _lastError = "timed out waiting for the J2534 broker";
                return false;
            }
            Sleep(10);
            continue;
        }

        char buf[512];
        DWORD got = 0;
        const DWORD wanted = available < sizeof buf ? available : (DWORD)sizeof buf;
        if (!ReadFile(_outRd, buf, wanted, &got, NULL) || got == 0) {
            _lastError = "cannot read a reply from the J2534 broker";
            return false;
        }
        _pending.append(buf, got);
    }
}

long J2534BrokerClient::simple(const std::string &line, unsigned long *value)
{
    if (!send(line)) return J2534API_ERROR_BROKER_TRANSPORT;
    std::string reply;
    if (!recv(&reply)) return J2534API_ERROR_BROKER_TRANSPORT;
    if (reply.compare(0, 2, "OK") == 0) {
        if (value) {
            std::istringstream in(reply.substr(2));
            *value = 0;
            in >> *value;
        }
        return 0;   /* STATUS_NOERROR */
    }
    _lastError = reply;
    long code = J2534API_ERROR_BROKER_TRANSPORT;
    if (reply.compare(0, 3, "ERR") == 0) {
        std::istringstream in(reply.substr(3));
        in >> code;
    }
    return code == 0 ? J2534API_ERROR_BROKER_TRANSPORT : code;
}

std::vector<BrokerLib> J2534BrokerClient::list()
{
    std::vector<BrokerLib> libs;
    unsigned long n = 0;
    if (simple("LIST", &n) != 0) return libs;
    for (unsigned long i = 0; i < n; i++) {
        std::string row;
        if (!recv(&row)) break;
        BrokerLib l;
        size_t t1 = row.find('\t');
        size_t t2 = (t1 == std::string::npos) ? t1 : row.find('\t', t1 + 1);
        l.name = (t1 == std::string::npos) ? row : row.substr(0, t1);
        if (t1 != std::string::npos)
            l.path = row.substr(t1 + 1, (t2 == std::string::npos) ? t2 : t2 - t1 - 1);
        if (t2 != std::string::npos)
            l.protocols = (unsigned int)strtoul(row.substr(t2 + 1).c_str(), NULL, 10);
        libs.push_back(l);
    }
    return libs;
}

bool J2534BrokerClient::load(const std::string &libPath)
{
    if (libPath.find('\r') != std::string::npos || libPath.find('\n') != std::string::npos) {
        _lastError = "the J2534 library path contains an invalid line break";
        return false;
    }
    return simple("LOAD " + libPath, NULL) == 0;
}

long J2534BrokerClient::open(unsigned long *deviceID)
{
    return simple("OPEN", deviceID);
}

long J2534BrokerClient::close(unsigned long)
{
    return simple("CLOSE", NULL);
}

long J2534BrokerClient::connect(unsigned long protocolID, unsigned long flags,
                                unsigned long baud, unsigned long *channelID)
{
    std::ostringstream o;
    o << "CONNECT " << protocolID << " " << flags << " " << baud;
    return simple(o.str(), channelID);
}

long J2534BrokerClient::disconnect(unsigned long)
{
    return simple("DISCONNECT", NULL);
}

long J2534BrokerClient::readVBatt(unsigned long *millivolts)
{
    return simple("VBATT", millivolts);
}

long J2534BrokerClient::startMsgFilter(unsigned long type,
                                       const std::vector<unsigned char> &mask,
                                       const std::vector<unsigned char> &pattern,
                                       const std::vector<unsigned char> &flowControl,
                                       unsigned long *filterID)
{
    std::ostringstream o;
    o << "FILTER " << type << " " << toHex(mask) << " " << toHex(pattern);
    if (!flowControl.empty()) o << " " << toHex(flowControl);
    return simple(o.str(), filterID);
}

long J2534BrokerClient::writeMsg(const std::vector<unsigned char> &data,
                                 unsigned long txFlags)
{
    std::ostringstream o;
    o << "WRITE " << toHex(data) << " " << txFlags;
    return simple(o.str(), NULL);
}

long J2534BrokerClient::readMsg(std::vector<unsigned char> *data,
                                unsigned long timeoutMs,
                                unsigned long *rxStatus,
                                unsigned long *extraDataIndex,
                                unsigned long *timestamp)
{
    if (rxStatus) *rxStatus = 0;
    if (extraDataIndex) *extraDataIndex = 0;
    if (timestamp) *timestamp = 0;
    std::ostringstream o;
    o << "READ " << timeoutMs;
    if (!send(o.str())) return J2534API_ERROR_BROKER_TRANSPORT;
    std::string reply;
    /* The broker waits inside the vendor DLL for timeoutMs. Give IPC a small
     * fixed margin, but never let unsigned arithmetic turn it into no timeout. */
    const DWORD replyTimeout = timeoutMs >= (unsigned long)(INFINITE - 5000)
            ? INFINITE - 1 : (DWORD)timeoutMs + 5000;
    if (!recv(&reply, replyTimeout)) return J2534API_ERROR_BROKER_TRANSPORT;
    if (reply.compare(0, 4, "DATA") != 0) {
        _lastError = reply;
        long code = J2534API_ERROR_BROKER_TRANSPORT;
        if (reply.compare(0, 3, "ERR") == 0) {
            std::istringstream errorReply(reply.substr(3));
            errorReply >> code;
        }
        return code == 0 ? J2534API_ERROR_BROKER_TRANSPORT : code;
    }
    /* "DATA" alone means nothing arrived before the timeout, which is a normal
     * outcome and not an error - the caller decides what silence means.
     * Otherwise: hex, then RxStatus, ExtraDataIndex and Timestamp. A broker that
     * predates those fields simply sends the hex, and they stay zero. */
    std::string body = (reply.size() > 5) ? reply.substr(5) : std::string();
    std::istringstream in(body);
    std::string hex;
    in >> hex;
    *data = fromHex(hex);
    unsigned long v = 0;
    if (in >> v) { if (rxStatus) *rxStatus = v; }
    if (in >> v) { if (extraDataIndex) *extraDataIndex = v; }
    if (in >> v) { if (timestamp) *timestamp = v; }
    return 0;
}

long J2534BrokerClient::stopMsgFilter(unsigned long filterID)
{
    std::ostringstream o;
    o << "STOPFILTER " << filterID;
    return simple(o.str(), NULL);
}

long J2534BrokerClient::ioctl(unsigned long ioctlID,
                              const std::vector<std::pair<unsigned long, unsigned long> > &cfg,
                              unsigned long *out)
{
    std::ostringstream o;
    o << "IOCTL " << ioctlID;
    for (size_t i = 0; i < cfg.size(); i++)
        o << " " << cfg[i].first << ":" << cfg[i].second;
    return simple(o.str(), out);
}

long J2534BrokerClient::readVersion(std::string *firmware, std::string *dll,
                                    std::string *api)
{
    if (!send("VERSION")) return J2534API_ERROR_BROKER_TRANSPORT;
    std::string reply;
    if (!recv(&reply)) return J2534API_ERROR_BROKER_TRANSPORT;
    if (reply.compare(0, 2, "OK") != 0) {
        _lastError = reply;
        long code = J2534API_ERROR_BROKER_TRANSPORT;
        if (reply.compare(0, 3, "ERR") == 0) {
            std::istringstream errorReply(reply.substr(3));
            errorReply >> code;
        }
        return code == 0 ? J2534API_ERROR_BROKER_TRANSPORT : code;
    }
    std::string body = reply.size() > 3 ? reply.substr(3) : std::string();
    size_t a = body.find('	'), b = (a == std::string::npos) ? a : body.find('	', a + 1);
    if (firmware) *firmware = body.substr(0, a);
    if (dll && a != std::string::npos) *dll = body.substr(a + 1, b - a - 1);
    if (api && b != std::string::npos) *api = body.substr(b + 1);
    return 0;
}
