// Peripheral.h
#ifndef PERIPHERAL_H
#define PERIPHERAL_H

#include "UdpSocket.h"
#include "SlowPacket.h"
#include <array>
#include <string>
#include <chrono>
#include <map>
#include <vector>
#include <mutex> // ADICIONADO: Para std::mutex

// Estrutura para guardar pacotes pendentes de ACK
struct UnackedPacketInfo {
    SlowPacket packet;
    std::chrono::steady_clock::time_point time_sent;
};

enum SessionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
};

class Peripheral {
public:
    Peripheral(const std::string& central_ip, int central_port);
    ~Peripheral();

    bool start();
    void run();

    bool sendConnect();
    bool sendData(const std::vector<uint8_t>& data_payload);
    bool sendDisconnect();

    // Getters para debug e status
    std::string getStateAsString() const;
    bool isConnected() const;
    uint32_t getSessionSTTL() const;
    uint32_t getCurrentSeqNum() const;
    size_t getUnackedPacketCount() const;

private:
    UdpSocket udp_socket;
    std::string central_ip;
    int central_port;
    SessionState current_state;

    std::array<uint8_t, 16> session_id;
    uint32_t session_sttl;

    uint32_t current_seqnum;
    uint32_t last_seqnum_from_central;
    uint16_t remote_window_size;
    uint16_t local_window_size;

    std::vector<UnackedPacketInfo> unacked_packets;
    // ADICIONADO: Mutex para proteger o acesso a `unacked_packets`.
    // Declarado como 'mutable' para que possa ser travado/destravado dentro de métodos const, como getUnackedPacketCount.
    mutable std::mutex unacked_packets_mtx; 
    
    std::map<uint8_t, std::map<uint8_t, std::vector<uint8_t>>> fragmented_data_buffer;

    void processReceivedPacket(const std::vector<uint8_t>& raw_packet);
    void handleSetupResponse(const SlowPacket& packet);
    void handleAckResponse(const SlowPacket& packet);
    void handleFailedResponse(const SlowPacket& packet);
    void handleRetransmission();
};

#endif