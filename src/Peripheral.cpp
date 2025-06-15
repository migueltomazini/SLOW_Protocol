// ... (comentários iniciais) ...

#include "Peripheral.h"
#include "SlowPacket.h"
#include "Utils.h"
#include <iostream>
#include <thread>
#include <algorithm> // ADICIONADO: Para std::remove_if

// ... (constantes de configuração) ...
const uint16_t LOCAL_RECEIVE_BUFFER_SIZE = 1024; 
const auto RETRANSMISSION_TIMEOUT = std::chrono::milliseconds(1000);
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
    
    // ADICIONADO: Configura o timeout para 100ms
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
    // ... (configuração do pacote de conexão) ...
    connect_packet.setSessionID(Utils::generateNilUUID());
    connect_packet.header.setSttl(0);
    connect_packet.header.setFlag(FLAG_CONNECT, true);

    connect_packet.setSequenceNumber(0);
    connect_packet.setAcknowledgementNumber(0);
    connect_packet.setWindowSize(local_window_size);
    connect_packet.setFragmentID(0);
    connect_packet.setFragmentOffset(0);

    // A chamada push_back agora está correta, pois estamos inserindo um UnackedPacketInfo
    unacked_packets.push_back({connect_packet, std::chrono::steady_clock::now()});

    auto raw_packet = connect_packet.serialize();

    // ADICIONADO: DEPURAÇÃO PARA VER OS BYTES
    Utils::printHex(raw_packet, "DEBUG: Pacote CONNECT saindo");

    // Envia
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
            // ... (configuração do pacote de fragmento) ...
            fragment_packet.setSessionID(session_id);
            fragment_packet.header.setFlag(FLAG_ACK, true);
            
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
            
            // push_back correto
            unacked_packets.push_back({fragment_packet, std::chrono::steady_clock::now()});
            auto raw_packet = fragment_packet.serialize();
            udp_socket.sendTo(raw_packet, central_ip, central_port);
            
            offset += chunk_size;
        }
        std::cout << "Dados fragmentados enviados em " << (int)fo << " pacotes." << std::endl;

    } else {
        // --- Pacote de Dados Simples ---
        SlowPacket data_packet;
        // ... (configuração do pacote de dados) ...
        data_packet.setSessionID(session_id);
        data_packet.header.setFlag(FLAG_ACK, true);


        data_packet.setSequenceNumber(current_seqnum++);
        data_packet.setAcknowledgementNumber(last_seqnum_from_central);
        data_packet.setWindowSize(local_window_size);
        data_packet.setData(data_payload);
        data_packet.header.setSttl(session_sttl);

        
        unacked_packets.push_back({data_packet, std::chrono::steady_clock::now()});
        auto raw_packet = data_packet.serialize();
    

        // --- LINHA ADICIONADA PARA DEBUG ---
        Utils::printHex(raw_packet, "DEBUG: Pacote DATA (simples) saindo");
        // ------------------------------------

        udp_socket.sendTo(raw_packet, central_ip, central_port);
        std::cout << "Pacote de dados (seq=" << data_packet.getSequenceNumber() << ") enviado." << std::endl;
    }

    return true;
}

bool Peripheral::sendDisconnect() {
    if (current_state != CONNECTED) return false;

    current_state = DISCONNECTING;
    SlowPacket disconnect_packet;
    // ... (configuração do pacote de desconexão) ...
    disconnect_packet.setSessionID(session_id);
    disconnect_packet.header.setFlag(FLAG_CONNECT, true);
    disconnect_packet.header.setFlag(FLAG_REVIVE, true);
    disconnect_packet.header.setFlag(FLAG_ACK, true);
    disconnect_packet.header.setSttl(session_sttl);
    disconnect_packet.setSequenceNumber(current_seqnum++);
    disconnect_packet.setAcknowledgementNumber(last_seqnum_from_central);
    disconnect_packet.setWindowSize(0);

    // push_back correto
    unacked_packets.push_back({disconnect_packet, std::chrono::steady_clock::now()});
    auto raw_packet = disconnect_packet.serialize();
    udp_socket.sendTo(raw_packet, central_ip, central_port);

    std::cout << "Pacote de desconexão enviado." << std::endl;
    return true;
}

void Peripheral::run() {
    // O loop continua enquanto não estivermos totalmente desconectados E limpos
    while (current_state != DISCONNECTED || !unacked_packets.empty()) {
        std::vector<uint8_t> buffer(MAX_SLOW_PACKET_SIZE);
        std::string sender_ip;
        int sender_port;

        // receiveFrom agora vai bloquear por no máximo 100ms
        ssize_t bytes_received = udp_socket.receiveFrom(buffer, sender_ip, sender_port);

        if (bytes_received > 0) {
            buffer.resize(bytes_received);
            if (sender_ip == central_ip && sender_port == central_port) {
                processReceivedPacket(buffer);
            }
        }
        
        // Esta lógica agora será executada a cada ~100ms, mesmo se nenhum pacote for recebido.
        // Isso permite que o pacote CONNECT seja retransmitido após o RETRANSMISSION_TIMEOUT.
        handleRetransmission();

        // REMOVIDO: A chamada sleep não é mais necessária, o timeout do socket controla o ritmo do loop.
        // std::this_thread::sleep_for(LOOP_INTERVAL);
    }
    std::cout << "Loop do peripheral encerrado." << std::endl;
}

void Peripheral::processReceivedPacket(const std::vector<uint8_t>& raw_packet) {
    // --- LINHA ADICIONADA PARA DEBUG ---
    Utils::printHex(raw_packet, "DEBUG: Pacote BRUTO recebido do Central");
    // ------------------------------------
    
    SlowPacket packet;
    if (!packet.deserialize(raw_packet)) {
        std::cerr << "Falha ao deserializar pacote recebido. Ignorando." << std::endl;
        return;
    }

    // Lógica principal de roteamento baseada no estado atual
    switch (current_state) {
        case CONNECTING:
            // No estado de conexão, esperamos um pacote SETUP (Accept/Reject)
            // A flag CONNECT do pacote recebido deve estar desligada.
            if (!packet.header.getFlag(FLAG_CONNECT)) {
                if (packet.header.getFlag(FLAG_ACCEPT_REJECT)) {
                    handleSetupResponse(packet);
                } else {
                    handleFailedResponse(packet);
                }
            }
            break;

        case CONNECTED:
        case DISCONNECTING:
            // Em um estado de sessão ativa, o SID deve corresponder.
            if (packet.getSessionID() != session_id) {
                std::cout << "Pacote recebido com SID incorreto. Ignorando." << std::endl;
                return;
            }

            // A maioria dos pacotes do central será um ACK.
            if (packet.header.getFlag(FLAG_ACK)) {
                handleAckResponse(packet);
            }
            
            // TODO: Se o central também pudesse enviar dados, aqui seria o lugar
            // para processar pacotes com payload (dados) e fragmentação.
            // Para este trabalho, só precisamos processar ACKs do central.
            break;
            
        case DISCONNECTED:
            // Por padrão, ignoramos pacotes quando estamos desconectados.
            // Uma implementação futura poderia lidar com pacotes de 'revive'.
            std::cout << "Pacote recebido no estado DISCONNECTED. Ignorando." << std::endl;
            break;
    }
}

void Peripheral::handleSetupResponse(const SlowPacket& packet) {
    // O pacote de Setup (Accept) confirma nossa conexão.
    std::cout << "STTL recebido do central: " << packet.getSttl() << std::endl;
    
    uint32_t received_seq = packet.getSequenceNumber();
    uint32_t received_ack = packet.getAcknowledgementNumber();

    // --- NOVO DEBUG CRÍTICO ---
    std::cout << "[DEBUG] handleSetupResponse: "
              << "Recebido SeqNum=" << received_seq
              << ", Recebido AckNum=" << received_ack << std::endl;

    current_state = CONNECTED;
    session_id = packet.getSessionID();
    remote_window_size = packet.getWindowSize();
    session_sttl = packet.getSttl();
    
    // O seqnum do pacote de setup é o primeiro da sessão (geralmente 0 ou 1).
    // O próximo pacote que recebermos do central deve ter um seqnum maior.
    // O acknum do setup confirma o nosso pacote de connect (seq=0).
    last_seqnum_from_central = packet.getSequenceNumber();

    // Nosso próximo número de sequência será 1.
    current_seqnum = packet.getSequenceNumber() + 1; 

    // O pacote de connect (seq=0) foi confirmado. Limpamos a fila de retransmissão.
    unacked_packets.clear();

    sendConnectAck(packet);

    std::cout << "Sessão estabelecida. SID recebido. Janela do Central: " << remote_window_size << std::endl;
}

void Peripheral::handleFailedResponse(const SlowPacket& packet) {
    // O pacote de Reject encerra a tentativa de conexão.
    std::cerr << "Falha na conexão: o central rejeitou o pedido." << std::endl;
    current_state = DISCONNECTED;
    // Limpa a fila, pois não vamos mais retransmitir o pacote de connect.
    unacked_packets.clear();
}

void Peripheral::handleAckResponse(const SlowPacket& packet) {
    uint32_t acknum = packet.getAcknowledgementNumber();
    
    remote_window_size = packet.getWindowSize();
    last_seqnum_from_central = packet.getSequenceNumber();

    // MODIFICADO: Agora acessamos unacked.packet para pegar o pacote
    unacked_packets.erase(
        std::remove_if(unacked_packets.begin(), unacked_packets.end(),
                       [acknum](const auto& unacked) {
                           return unacked.packet.getSequenceNumber() < acknum;
                       }),
        unacked_packets.end()
    );

    if (current_state == DISCONNECTING && unacked_packets.empty()) {
        current_state = DISCONNECTED;
        std::cout << "Desconexão confirmada pelo central." << std::endl;
    }
    
    std::cout << "ACK recebido (acknum=" << acknum << "). Pacotes em trânsito: " << unacked_packets.size() << std::endl;
}

void Peripheral::handleRetransmission() {
    auto now = std::chrono::steady_clock::now();
    for (auto& unacked : unacked_packets) {
        // MODIFICADO: acessamos unacked.time_sent e unacked.packet
        if (now - unacked.time_sent > RETRANSMISSION_TIMEOUT) {
            std::cout << "Timeout! Retransmitindo pacote com seq=" << unacked.packet.getSequenceNumber() << std::endl;
            auto raw_packet = unacked.packet.serialize();
            udp_socket.sendTo(raw_packet, central_ip, central_port);
            // Atualiza o timestamp do pacote reenviado
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
    return unacked_packets.size();
}

void Peripheral::sendConnectAck(const SlowPacket& packet) {
    std::cout << "[DEBUG] Enviando 3ª via do handshake (puro ACK)..." << std::endl;
    
    SlowPacket ack_packet;
    ack_packet.setSessionID(session_id);    // Usa o SID que acabamos de receber
    ack_packet.header.setSttl(session_sttl); // Usa o STTL da sessão

    // Flags: Apenas a flag ACK. Este pacote apenas confirma o recebimento do Setup.
    ack_packet.header.setFlag(FLAG_ACK, true);
    
    // SeqNum: Conforme o TCP, nosso seqnum avança. Se o Connect foi 0, este é 1.
    ack_packet.setSequenceNumber(packet.getSequenceNumber() + 1);

    // AckNum: Conforme o TCP, confirmamos o seqnum do servidor (ISN_S) com ISN_S + 1.
    ack_packet.setAcknowledgementNumber(last_seqnum_from_central);
    
    ack_packet.setWindowSize(local_window_size);
    // IMPORTANTE: Este é um pacote de puro controle, sem dados (payload).

    // Este pacote consome o seqnum=1. O próximo pacote (o primeiro de DADOS) usará seq=2.
    current_seqnum = packet.getSequenceNumber() + 2;

    // Colocamos na fila de retransmissão para garantir que o servidor o receba
    // e estabeleça a conexão. O servidor pode responder com um ACK para este pacote.
    // unacked_packets.push_back({ack_packet, std::chrono::steady_clock::now()});

    auto raw_packet = ack_packet.serialize();
    Utils::printHex(raw_packet, "DEBUG: Pacote Connect-ACK saindo");
    udp_socket.sendTo(raw_packet, central_ip, central_port);
}