/**
 * @file Peripheral.h
 * @brief Definição da classe Peripheral e estruturas relacionadas para o protocolo SLOW.
 */
#ifndef PERIPHERAL_H
#define PERIPHERAL_H

#include "UdpSocket.h"
#include "SlowPacket.h"
#include <array>
#include <string>
#include <chrono>
#include <map>
#include <vector>
#include <mutex>

/**
 * @struct UnackedPacketInfo
 * @brief Armazena informações de um pacote enviado que ainda não foi confirmado (ACKed).
 */
struct UnackedPacketInfo {
    SlowPacket packet;
    std::chrono::steady_clock::time_point time_sent;
};

/**
 * @enum SessionState
 * @brief Enumeração dos possíveis estados de uma sessão SLOW.
 */
enum SessionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
};

/**
 * @class Peripheral
 * @brief Gerencia a lógica do cliente (periférico) para o protocolo SLOW.
 */
class Peripheral {
public:
    Peripheral(const std::string& central_ip, int central_port);
    ~Peripheral();

    // Métodos do ciclo de vida
    bool start();
    void run();
    bool sendData(const std::vector<uint8_t>& data_payload);
    bool sendDisconnect();

    // Getters para consulta de estado
    std::string getStateAsString() const;
    bool isConnected() const;
    uint32_t getSessionSTTL() const;
    uint32_t getCurrentSeqNum() const;
    size_t getUnackedPacketCount() const;

private:
    // Métodos internos de envio e processamento
    bool sendConnect();
    void processReceivedPacket(const std::vector<uint8_t>& raw_packet);
    void handleSetupResponse(const SlowPacket& packet);
    void handleAckResponse(const SlowPacket& packet);
    void handleFailedResponse(const SlowPacket& packet);
    void handleRetransmission();

    // Membros de estado da sessão
    UdpSocket udp_socket;
    std::string central_ip;
    int central_port;
    SessionState current_state;
    std::array<uint8_t, 16> session_id;
    uint32_t session_sttl;

    // Membros de controle de fluxo e sequência
    uint32_t current_seqnum;
    uint32_t last_seqnum_from_central;
    uint16_t remote_window_size;
    uint16_t local_window_size;

    // Fila de retransmissão
    std::vector<UnackedPacketInfo> unacked_packets;
    mutable std::mutex unacked_packets_mtx; 
};

#endif