// Peripheral.h
// Este arquivo de cabeçalho define a classe Peripheral, que representa
// a lógica principal do lado "cliente" do protocolo SLOW.
// Ele gerencia o estado da conexão, os identificadores de sessão,
// números de sequência e a janela de controle de fluxo.
// Declara métodos para iniciar a conexão, enviar dados, desconectar,
// e processar os pacotes recebidos do central.

#ifndef PERIPHERAL_H
#define PERIPHERAL_H

#include "UdpSocket.h"
#include "SlowPacket.h"
#include <array>
#include <string>
#include <chrono>
#include <map>
#include <vector>

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

private:
    UdpSocket udp_socket;
    std::string central_ip;
    int central_port;
    SessionState current_state;

    std::array<uint8_t, 16> session_id;
    uint32_t session_sttl; // ADICIONADO: Para armazenar o TTL da sessão 

    uint32_t current_seqnum;
    uint32_t last_acknum_received;
    uint16_t remote_window_size;
    uint16_t local_window_size;

    std::vector<UnackedPacketInfo> unacked_packets;
    std::map<uint8_t, std::map<uint8_t, std::vector<uint8_t>>> fragmented_data_buffer;

    void processReceivedPacket(const std::vector<uint8_t>& raw_packet);
    void handleSetupResponse(const SlowPacket& packet);
    void handleAckResponse(const SlowPacket& packet);
    void handleFailedResponse(const SlowPacket& packet);
    void handleRetransmission();
};

#endif