/*
 * J2534_broker_client - the 64-bit side of the bridge.
 *
 * Drop-in replacement for the parts of J2534_API that touch the vendor DLL. The
 * class surface is deliberately the same, so FreeSSM's J2534DiagInterface, its
 * protocol code and its GUI are untouched: the only thing that changes is how a
 * PassThru call reaches the library.
 *
 * On a 64-bit build the DLL cannot be loaded in-process - it is 32-bit and the
 * operating system forbids it - so every call is relayed to a small 32-bit helper
 * over a pipe. On a 32-bit build none of this is needed and J2534_API keeps loading
 * the library directly, which is why the whole thing is behind _WIN64.
 *
 * The helper is spawned once and lives as long as the connection. It is a child
 * process on an anonymous pipe, so it needs no port and is not reachable from the
 * network. A Windows job object terminates it if FreeSSM crashes.
 */

#ifndef J2534_BROKER_CLIENT_H
#define J2534_BROKER_CLIENT_H

#include <string>
#include <vector>
#include <utility>
#include <windows.h>

struct BrokerLib
{
    std::string name;
    std::string path;
    /* Bit values match J2534_protocol_flags. Zero means the registry named no
     * protocol, which is worth passing through as-is rather than guessing. */
    unsigned int protocols;
    BrokerLib() : protocols(0) {}
};

class J2534BrokerClient
{
public:
    J2534BrokerClient();
    ~J2534BrokerClient();

    /* Start the helper. brokerPath is the 32-bit executable; empty means look for
     * j2534_broker.exe beside the application. */
    bool start(const std::string &brokerPath = std::string());
    void stop();
    bool running() const { return _proc != NULL; }

    /* Registered interfaces, as the helper sees them - which is the 32-bit registry
     * view. A 64-bit process reading HKLM\SOFTWARE without KEY_WOW64_32KEY sees no
     * PassThruSupport key at all and concludes, wrongly and silently, that no
     * interface is installed. */
    std::vector<BrokerLib> list();

    bool load(const std::string &libPath);
    long open(unsigned long *deviceID);
    long close(unsigned long deviceID);
    long connect(unsigned long protocolID, unsigned long flags,
                 unsigned long baud, unsigned long *channelID);
    long disconnect(unsigned long channelID);
    long readVBatt(unsigned long *millivolts);
    long startMsgFilter(unsigned long type, const std::vector<unsigned char> &mask,
                        const std::vector<unsigned char> &pattern,
                        const std::vector<unsigned char> &flowControl,
                        unsigned long *filterID);
    long writeMsg(const std::vector<unsigned char> &data, unsigned long txFlags);
    long readMsg(std::vector<unsigned char> *data, unsigned long timeoutMs,
                 unsigned long *rxStatus = 0, unsigned long *extraDataIndex = 0,
                 unsigned long *timestamp = 0);
    long stopMsgFilter(unsigned long filterID);
    /* SET_CONFIG relays as parameter/value pairs; pass an empty list for the
     * null-input ioctls such as CLEAR_RX_BUFFER. */
    long ioctl(unsigned long ioctlID,
               const std::vector<std::pair<unsigned long, unsigned long> > &cfg,
               unsigned long *out);
    long readVersion(std::string *firmware, std::string *dll, std::string *api);

    std::string lastError() const { return _lastError; }

private:
    bool send(const std::string &line);
    bool recv(std::string *line, DWORD timeoutMs = 15000);
    long simple(const std::string &line, unsigned long *value);

    HANDLE _proc, _job, _inWr, _outRd;
    std::string _lastError;
    std::string _pending;   /* bytes read past the end of a line */
};

#endif
