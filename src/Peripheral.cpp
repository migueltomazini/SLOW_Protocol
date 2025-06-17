// Peripheral.cpp - VERSÃO THREAD-SAFE
// Adiciona locks de mutex em todos os acessos à fila unacked_packets
// para prevenir condições de corrida entre a thread principal e a de rede.

#include "Peripheral.h"
#include "SlowPacket.h"
#include "Utils.h"
#include <iostream>
#include <thread>
#include <algorithm>
#include <vector>

const uint16_t LOCAL_RECEIVE_BUFFER_SIZE = 1024; 
const auto RETRANSMISSION_TIMEOUT = std::chrono::milliseconds(10000);
const auto LOOP_INTERVAL = std::chrono::milliseconds(10);

Peripheral::Peripheral(const std::string& central_ip, int central_port)
    : central_ip(central_ip),
      central_port(central_port),
      current_state(DISCONNECTED),
      current_seqnum(0),
      last_seqnum_from_central(0),
      remote_window_size(0),
      local_window_size(LOCAL_RECEIVE_BUFFER_SIZE),
      session_sttl(0)
{
    session_id = Utils::generateNilUUID();
}

Peripheral::~Peripheral() {}

bool Peripheral::start() {
    if (!udp_socket.bindSocket(0)) {
        std::cerr << "Falha ao iniciar o peripheral: não foi possível ligar o socket." << std::endl;
        return false;
    }
    
    if (!udp_socket.setReceiveTimeout(0, 100000)) { // 0 segundos, 100000 microssegundos
        std::cerr << "Falha ao configurar timeout do socket." << std::endl;
        return false;
    }

    std::cout << "Socket inicializado. Enviando pedido de conexão para " << central_ip << ":" << central_port << std::endl;
    return sendConnect();
}

bool Peripheral::sendConnect() {
    if (current_state != DISCONNECTED) {
        std::cerr << "Só é possível iniciar uma conexão a partir do estado DISCONNECTED." << std::endl;
        return false;
    }

    current_state = CONNECTING;

    SlowPacket connect_packet;
    connect_packet.setSessionID(Utils::generateNilUUID());
    connect_packet.header.setSttl(0);
    connect_packet.header.setFlag(FLAG_CONNECT, true);
    connect_packet.setSequenceNumber(0);
    connect_packet.setAcknowledgementNumber(0);
    connect_packet.setWindowSize(local_window_size);
    connect_packet.setFragmentID(0);
    connect_packet.setFragmentOffset(0);

    // --- PROTEGIDO POR MUTEX ---
    {
        std::lock_guard<std::mutex> lock(unacked_packets_mtx);
        unacked_packets.push_back({connect_packet, std::chrono::steady_clock::now()});
    }
    // ---------------------------

    auto raw_packet = connect_packet.serialize();
    Utils::printHex(raw_packet, "DEBUG: Pacote CONNECT saindo");
    udp_socket.sendTo(raw_packet, central_ip, central_port);
    
    std::cout << "Pacote de conexão enviado. Aguardando resposta..." << std::endl;
    return true;
}

bool Peripheral::sendData(const std::vector<uint8_t>& data_payload) {
    if (current_state != CONNECTED) {
        std::cerr << "Não é possível enviar dados. Conexão não estabelecida." << std::endl;
        return false;
    }
    
    if (data_payload.size() > MAX_SLOW_DATA_SIZE) {
        // --- Lógica de Fragmentação ---
        uint8_t fid = Utils::generateUUIDv8()[0];
        size_t offset = 0;
        uint8_t fo = 0;

        while(offset < data_payload.size()) {
            SlowPacket fragment_packet;
            fragment_packet.setSessionID(session_id);
            // A flag ACK não deve ser setada em pacotes de dados
            // fragment_packet.header.setFlag(FLAG_ACK, true); 
            
            size_t chunk_size = std::min((size_t)MAX_SLOW_DATA_SIZE, data_payload.size() - offset);
            std::vector<uint8_t> chunk(data_payload.begin() + offset, data_payload.begin() + offset + chunk_size);
            fragment_packet.setData(chunk);

            if (offset + chunk_size < data_payload.size()) {
                fragment_packet.header.setFlag(FLAG_MORE_BITS, true);
            }

            fragment_packet.header.setSttl(session_sttl);
            fragment_packet.setSequenceNumber(current_seqnum++);
            fragment_packet.setAcknowledgementNumber(last_seqnum_from_central);
            fragment_packet.setWindowSize(local_window_size);
            fragment_packet.setFragmentID(fid);
            fragment_packet.setFragmentOffset(fo++);
            
            // --- PROTEGIDO POR MUTEX ---
            {
                std::lock_guard<std::mutex> lock(unacked_packets_mtx);
                unacked_packets.push_back({fragment_packet, std::chrono::steady_clock::now()});
            }
            // ---------------------------

            auto raw_packet = fragment_packet.serialize();
            udp_socket.sendTo(raw_packet, central_ip, central_port);
            
            offset += chunk_size;
        }
        std::cout << "Dados fragmentados enviados em " << (int)fo << " pacotes." << std::endl;

    } else {
        // --- Pacote de Dados Simples ---
        SlowPacket data_packet;
        data_packet.setSessionID(session_id);
        // data_packet.header.setFlag(FLAG_ACK, false); // Já é falso por padrão
        data_packet.setSequenceNumber(current_seqnum++);
        data_packet.setAcknowledgementNumber(last_seqnum_from_central);
        data_packet.setWindowSize(local_window_size);
        data_packet.setData(data_payload);
        data_packet.header.setSttl(session_sttl);

        Utils::printPacketDetails(data_packet, "Pacote DATA (simples) Saindo");
        
        // --- PROTEGIDO POR MUTEX ---
        {
            std::lock_guard<std::mutex> lock(unacked_packets_mtx);
            unacked_packets.push_back({data_packet, std::chrono::steady_clock::now()});
        }
        // ---------------------------

        auto raw_packet = data_packet.serialize();
        Utils::printHex(raw_packet, "DEBUG: Pacote DATA (simples) saindo");
        udp_socket.sendTo(raw_packet, central_ip, central_port);
        std::cout << "Pacote de dados (seq=" << data_packet.getSequenceNumber() -1 << ") enviado." << std::endl;
    }

    return true;
}

bool Peripheral::sendDisconnect() {
    if (current_state != CONNECTED) return false;

    current_state = DISCONNECTING;
    SlowPacket disconnect_packet;
    disconnect_packet.setSessionID(Utils::generateNilUUID());
    disconnect_packet.header.setFlag(FLAG_CONNECT, true);
    disconnect_packet.header.setFlag(FLAG_REVIVE, true);
    disconnect_packet.header.setFlag(FLAG_ACK, true);
    disconnect_packet.header.setSttl(0);
    disconnect_packet.setSequenceNumber(0);
    disconnect_packet.setAcknowledgementNumber(0);
    disconnect_packet.setWindowSize(local_window_size);

    // --- PROTEGIDO POR MUTEX ---
    {
        std::lock_guard<std::mutex> lock(unacked_packets_mtx);
        unacked_packets.push_back({disconnect_packet, std::chrono::steady_clock::now()});
    }
    // ---------------------------

    auto raw_packet = disconnect_packet.serialize();
    udp_socket.sendTo(raw_packet, central_ip, central_port);

    Utils::printHex(raw_packet, "DEBUG: Pacote DATA (simples) saindo");

    std::cout << "Pacote de desconexão enviado." << std::endl;
    return true;
}

void Peripheral::run() {
    while (current_state != DISCONNECTED || !unacked_packets.empty()) {
        std::vector<uint8_t> buffer(MAX_SLOW_PACKET_SIZE);
        std::string sender_ip;
        int sender_port;

        ssize_t bytes_received = udp_socket.receiveFrom(buffer, sender_ip, sender_port);

        if (bytes_received > 0) {
            buffer.resize(bytes_received);
            if (sender_ip == central_ip && sender_port == central_port) {
                processReceivedPacket(buffer);
            }
        }
        
        handleRetransmission();
    }
    std::cout << "Loop do peripheral encerrado." << std::endl;
}

void Peripheral::processReceivedPacket(const std::vector<uint8_t>& raw_packet) {
    Utils::printHex(raw_packet, "DEBUG: Pacote BRUTO recebido do Central");
    
    SlowPacket packet;
    if (!packet.deserialize(raw_packet)) {
        std::cerr << "Falha ao deserializar pacote recebido. Ignorando." << std::endl;
        return;
    }

    switch (current_state) {
        case CONNECTING:
            if (!packet.header.getFlag(FLAG_CONNECT) && packet.header.getFlag(FLAG_ACCEPT_REJECT)) {
                handleSetupResponse(packet);
            } else {
                handleFailedResponse(packet);
            }
            break;

        case CONNECTED:
        case DISCONNECTING:
            if (packet.getSessionID() != session_id) {
                std::cout << "Pacote recebido com SID incorreto. Ignorando." << std::endl;
                return;
            }
            if (packet.header.getFlag(FLAG_ACK)) {
                handleAckResponse(packet);
            }
            break;
            
        case DISCONNECTED:
            std::cout << "Pacote recebido no estado DISCONNECTED. Ignorando." << std::endl;
            break;
    }
}

void Peripheral::handleSetupResponse(const SlowPacket& packet) {
    std::cout << "STTL recebido do central: " << packet.getSttl() << std::endl;
    Utils::printPacketDetails(packet, "Pacote Recebido (simples) Saindo");
    
    uint32_t received_seq = packet.getSequenceNumber();
    uint32_t received_ack = packet.getAcknowledgementNumber();

    std::cout << "[DEBUG] handleSetupResponse: "
              << "Recebido SeqNum=" << received_seq
              << ", Recebido AckNum=" << received_ack << std::endl;

    current_state = CONNECTED;
    session_id = packet.getSessionID();
    remote_window_size = packet.getWindowSize();
    session_sttl = packet.getSttl();
    last_seqnum_from_central = packet.getSequenceNumber();
    current_seqnum = packet.getSequenceNumber() + 1; 

    // --- PROTEGIDO POR MUTEX ---
    {
        std::lock_guard<std::mutex> lock(unacked_packets_mtx);
        unacked_packets.clear();
    }
    // ---------------------------

    std::cout << "Sessão estabelecida. SID recebido. Janela do Central: " << remote_window_size << std::endl;
}

void Peripheral::handleFailedResponse(const SlowPacket& packet) {
    std::cerr << "Falha na conexão: o central rejeitou o pedido." << std::endl;
    current_state = DISCONNECTED;
    
    // --- PROTEGIDO POR MUTEX ---
    {
        std::lock_guard<std::mutex> lock(unacked_packets_mtx);
        unacked_packets.clear();
    }
    // ---------------------------
}

void Peripheral::handleAckResponse(const SlowPacket& packet) {
    uint32_t acknum = packet.getAcknowledgementNumber();
    
    remote_window_size = packet.getWindowSize();
    last_seqnum_from_central = packet.getSequenceNumber();

    size_t count_before = 0;
    // --- PROTEGIDO POR MUTEX ---
    {
        std::lock_guard<std::mutex> lock(unacked_packets_mtx);
        count_before = unacked_packets.size();
        unacked_packets.erase(
            std::remove_if(unacked_packets.begin(), unacked_packets.end(),
                           [acknum](const auto& unacked) {
                               return unacked.packet.getSequenceNumber() <= acknum;
                           }),
            unacked_packets.end()
        );
    }
    // ---------------------------
    
    // A verificação de desconexão e o log devem usar a contagem atual (thread-safe)
    if (current_state == DISCONNECTING && getUnackedPacketCount() == 0) {
        current_state = DISCONNECTED;
        std::cout << "Desconexão confirmada pelo central." << std::endl;
    }
    
    std::cout << "ACK recebido (acknum=" << acknum << "). Pacotes em trânsito: " << getUnackedPacketCount() << std::endl;
}

void Peripheral::handleRetransmission() {
    auto now = std::chrono::steady_clock::now();
    
    // --- PROTEGIDO POR MUTEX ---
    std::lock_guard<std::mutex> lock(unacked_packets_mtx);
    for (auto& unacked : unacked_packets) {
        if (now - unacked.time_sent > RETRANSMISSION_TIMEOUT) {
            std::cout << "Timeout! Retransmitindo pacote com seq=" << unacked.packet.getSequenceNumber() << std::endl;
            auto raw_packet = unacked.packet.serialize();
            udp_socket.sendTo(raw_packet, central_ip, central_port);
            unacked.time_sent = now;
        }
    }
}

std::string Peripheral::getStateAsString() const {
    switch (current_state) {
        case DISCONNECTED:  return "DISCONNECTED";
        case CONNECTING:    return "CONNECTING";
        case CONNECTED:     return "CONNECTED";
        case DISCONNECTING: return "DISCONNECTING";
        default:            return "UNKNOWN";
    }
}

bool Peripheral::isConnected() const {
    return current_state == CONNECTED;
}

uint32_t Peripheral::getSessionSTTL() const {
    return session_sttl;
}

uint32_t Peripheral::getCurrentSeqNum() const {
    return current_seqnum;
}

size_t Peripheral::getUnackedPacketCount() const {
    // --- PROTEGIDO POR MUTEX ---
    std::lock_guard<std::mutex> lock(unacked_packets_mtx);
    return unacked_packets.size();
}
