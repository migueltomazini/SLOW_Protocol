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
    void run(); // Loop principal do peripheral

    // Métodos para enviar os tipos de mensagens
    bool sendConnect();
    bool sendData(const std::vector<uint8_t>& data_payload);
    bool sendDisconnect();
    bool sendReviveData(const std::vector<uint8_t>& data_payload);


private:
    UdpSocket udp_socket;
    std::string central_ip;
    int central_port;
    SessionState current_state;
    std::array<uint8_t, 16> session_id;
    uint32_t current_seqnum;
    uint32_t last_acknum_received;
    uint16_t remote_window_size; // Janela reportada pelo central
    uint16_t local_window_size; // Sua própria janela de buffer

    // Buffer para dados pendentes de ACK ou para remontagem de fragmentos
    std::vector<SlowPacket> unacked_packets;
    // Buffer para remontagem de pacotes fragmentados
    std::map<uint8_t, std::map<uint8_t, std::vector<uint8_t>>> fragmented_data_buffer;

    // Helper para processar pacotes recebidos
    void processReceivedPacket(const std::vector<uint8_t>& raw_packet);
    void handleSetupResponse(const SlowPacket& packet);
    void handleAckResponse(const SlowPacket& packet);
    void handleFailedResponse(const SlowPacket& packet);

    // Métodos para gerenciar a janela deslizante
    void updateEffectiveWindow();

    // Se não receber ACK, retransmitir
    void handleRetransmission();

    // Método para gerenciar fragmentação
    bool assembleFragmentedPacket(const SlowPacket& fragment_packet);

};

#endif