/**
 * @file Peripheral.cpp
 * @brief Implementação da classe Peripheral, que gerencia a lógica do cliente do protocolo SLOW.
 *
 * Esta classe é responsável por todo o ciclo de vida da comunicação do lado do periférico:
 * - Iniciar a conexão com o Central através de um handshake.
 * - Enviar pacotes de dados, com suporte a fragmentação para cargas maiores.
 * - Manter o estado da sessão (ID, STTL, números de sequência).
 * - Processar respostas do Central, como confirmações (ACKs).
 * - Gerenciar uma fila de pacotes não confirmados para retransmissão em caso de timeout.
 * - Iniciar o processo de desconexão.
 * A classe é projetada para operar de forma assíncrona. Uma thread de rede dedicada
 * executa o método `run()` para receber pacotes e gerenciar retransmissões, enquanto
 * outros métodos são chamados para iniciar o envio de dados. O acesso a recursos
 * compartilhados, como a fila de retransmissão, é protegido por mutex.
 */

#include "Peripheral.h"
#include "SlowPacket.h"
#include "Utils.h"
#include <iostream>
#include <thread>
#include <algorithm>
#include <vector>

// Constantes de configuração do protocolo
const uint16_t LOCAL_RECEIVE_BUFFER_SIZE = 1024; 
const auto RETRANSMISSION_TIMEOUT = std::chrono::milliseconds(10000); // Timeout para reenviar um pacote

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

//==============================================================================
// MÉTODOS PÚBLICOS - Ciclo de Vida da Conexão
//==============================================================================

/**
 * @brief Inicializa o socket e envia o primeiro pacote de conexão.
 * @return true se o socket foi inicializado e o pacote de conexão enviado, false caso contrário.
 */
bool Peripheral::start() {
    if (!udp_socket.bindSocket(0)) {
        std::cerr << "Falha ao iniciar o peripheral: não foi possível ligar o socket." << std::endl;
        return false;
    }
    
    // Define um timeout para a chamada de recebimento no socket.
    // Isso permite que o loop da thread de rede não bloqueie indefinidamente,
    // podendo verificar outras lógicas, como a de retransmissão.
    if (!udp_socket.setReceiveTimeout(0, 100000)) { // 100ms
        std::cerr << "Falha ao configurar timeout do socket." << std::endl;
        return false;
    }

    return sendConnect();
}

/**
 * @brief Envia um pacote de dados para o Central. Lida com fragmentação se necessário.
 * @param data_payload O vetor de bytes a ser enviado.
 * @return true se os dados foram enfileirados para envio, false se a conexão não estiver ativa.
 */
bool Peripheral::sendData(const std::vector<uint8_t>& data_payload) {
    if (current_state != CONNECTED) {
        std::cerr << "Não é possível enviar dados. Conexão não estabelecida." << std::endl;
        return false;
    }
    
    // Se os dados excederem o tamanho máximo, fragmente-os.
    if (data_payload.size() > MAX_SLOW_DATA_SIZE) {
        uint8_t fid = Utils::generateUUIDv8()[0]; // ID de fragmento único para esta mensagem
        size_t offset = 0;
        uint8_t fo = 0; // Fragment Offset começa em 0

        while(offset < data_payload.size()) {
            SlowPacket fragment_packet;
            fragment_packet.setSessionID(session_id);
            
            size_t chunk_size = std::min((size_t)MAX_SLOW_DATA_SIZE, data_payload.size() - offset);
            std::vector<uint8_t> chunk(data_payload.begin() + offset, data_payload.begin() + offset + chunk_size);
            fragment_packet.setData(chunk);

            // Define a flag 'More Bits' se este não for o último fragmento.
            if (offset + chunk_size < data_payload.size()) {
                fragment_packet.header.setFlag(FLAG_MORE_BITS, true);
            }

            fragment_packet.header.setFlag(FLAG_ACK, true);
            fragment_packet.header.setSttl(session_sttl);
            fragment_packet.setSequenceNumber(current_seqnum++);
            fragment_packet.setAcknowledgementNumber(last_seqnum_from_central);
            fragment_packet.setWindowSize(local_window_size);
            fragment_packet.setFragmentID(fid);
            fragment_packet.setFragmentOffset(fo);

            // Imprime detalhes de cada fragmento
            std::string label = "Pacote DATA Fragmento " + std::to_string(static_cast<int>(fo)) + " Saindo";
            Utils::printPacketDetails(fragment_packet, label);
            
            // Adiciona à fila de retransmissão de forma segura
            {
                std::lock_guard<std::mutex> lock(unacked_packets_mtx);
                unacked_packets.push_back({fragment_packet, std::chrono::steady_clock::now()});
            }
            auto raw_packet = fragment_packet.serialize();
            udp_socket.sendTo(raw_packet, central_ip, central_port);
            offset += chunk_size;
            fo++;
        }
        std::cout << "Dados fragmentados enviados em " << (int)fo << " pacotes." << std::endl;
    } else {
        // Envio de pacote de dados simples (não fragmentado).
        SlowPacket data_packet;
        data_packet.header.setFlag(FLAG_ACK, true);
        data_packet.header.setSttl(session_sttl);
        data_packet.setSessionID(session_id);
        data_packet.setSequenceNumber(current_seqnum++);
        data_packet.setAcknowledgementNumber(last_seqnum_from_central);
        data_packet.setWindowSize(local_window_size);
        data_packet.setData(data_payload);

        Utils::printPacketDetails(data_packet, "Pacote DATA Saindo");
        
        {
            std::lock_guard<std::mutex> lock(unacked_packets_mtx);
            unacked_packets.push_back({data_packet, std::chrono::steady_clock::now()});
        }
        auto raw_packet = data_packet.serialize();
        udp_socket.sendTo(raw_packet, central_ip, central_port);
        std::cout << "Pacote de dados (seq=" << data_packet.getSequenceNumber() -1 << ") enviado." << std::endl;
    }
    return true;
}

/**
 * @brief Envia um pacote de desconexão para o Central.
 * @note O pacote é configurado com flags específicas (CONNECT, REVIVE, ACK) e os
 *       dados da sessão para sinalizar o encerramento da conexão. Esta função
 *       inicia a transição para o estado de desconexão no lado do periférico.
 * @return true se o pacote foi enviado, false se a conexão não estava ativa.
 */
bool Peripheral::sendDisconnect() {
    if (current_state != CONNECTED) return false;

    current_state = DISCONNECTING;
    SlowPacket disconnect_packet;
    
    disconnect_packet.setSessionID(session_id);
    disconnect_packet.header.setSttl(session_sttl);
    disconnect_packet.setSequenceNumber(current_seqnum++);
    disconnect_packet.setAcknowledgementNumber(last_seqnum_from_central);
    disconnect_packet.setWindowSize(local_window_size);
    disconnect_packet.header.setFlag(FLAG_CONNECT, true);
    disconnect_packet.header.setFlag(FLAG_REVIVE, true);
    disconnect_packet.header.setFlag(FLAG_ACK, true);

    Utils::printPacketDetails(disconnect_packet, "Pacote DISCONNECT Saindo");

    auto raw_packet = disconnect_packet.serialize();
    udp_socket.sendTo(raw_packet, central_ip, central_port);
    
    std::cout << "Pacote de desconexão enviado. A sessão será encerrada." << std::endl;
    return true;
}

//==============================================================================
// THREAD DE REDE
//==============================================================================

/**
 * @brief O loop principal da thread de rede.
 *        Escuta continuamente por pacotes UDP, os processa e lida com retransmissões.
 *        O loop termina quando o estado é DISCONNECTED e não há mais pacotes aguardando ACK.
 */
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
    std::cout << "Loop da thread de rede encerrado." << std::endl;
}

//==============================================================================
// PROCESSAMENTO DE PACOTES (HANDLERS)
//==============================================================================

/**
 * @brief Deserializa e roteia um pacote recebido para o handler apropriado com base no estado atual.
 * @param raw_packet O vetor de bytes brutos recebido do socket.
 */
void Peripheral::processReceivedPacket(const std::vector<uint8_t>& raw_packet) {
    SlowPacket packet;
    if (!packet.deserialize(raw_packet)) {
        std::cerr << "Aviso: Falha ao deserializar pacote recebido. Ignorando." << std::endl;
        return;
    }

    switch (current_state) {
        case CONNECTING:
            // No estado CONNECTING, esperamos uma resposta de Setup.
            if (!packet.header.getFlag(FLAG_CONNECT) && packet.header.getFlag(FLAG_ACCEPT_REJECT)) {
                handleSetupResponse(packet);
            } else {
                handleFailedResponse(packet);
            }
            break;

        case CONNECTED:
            // Em uma sessão ativa, o SID deve corresponder.
            if (packet.getSessionID() != session_id) {
                std::cout << "Aviso: Pacote recebido com SID incorreto. Ignorando." << std::endl;
                return;
            }
            if (packet.header.getFlag(FLAG_ACK)) {
                handleAckResponse(packet);
            }
            break;
        
        case DISCONNECTING:
            // Durante a desconexão, um pacote com SID diferente do esperado é um erro.
            if (packet.getSessionID() != session_id) {
                std::cerr << "\nERRO CRÍTICO: Pacote com SID inválido recebido durante a desconexão." << std::endl;
                Utils::printPacketDetails(packet, "Pacote Incorreto Recebido");
                current_state = DISCONNECTED;
                // Limpa pacotes não confirmados para garantir que o loop run() termine.
                {
                    std::lock_guard<std::mutex> lock(unacked_packets_mtx);
                    unacked_packets.clear();
                }
                return;
            }
            // Se o SID estiver correto, processa como um ACK normal.
            if (packet.header.getFlag(FLAG_ACK)) {
                handleAckResponse(packet);
            }
            break;
            
        case DISCONNECTED:
            break;
    }
}

/**
 * @brief Trata a resposta de Setup do Central, estabelecendo a sessão.
 * @param packet O pacote de Setup recebido.
 */
void Peripheral::handleSetupResponse(const SlowPacket& packet) {
    std::cout << "STTL recebido do central: " << packet.getSttl() << "ms" << std::endl;
    Utils::printPacketDetails(packet, "Pacote Setup Recebido");

    current_state = CONNECTED;
    session_id = packet.getSessionID();
    remote_window_size = packet.getWindowSize();
    session_sttl = packet.getSttl();
    last_seqnum_from_central = packet.getSequenceNumber();
    current_seqnum = packet.getSequenceNumber() + 1; 

    // O pacote de Connect (seq=0) foi confirmado, limpa a fila de retransmissão.
    {
        std::lock_guard<std::mutex> lock(unacked_packets_mtx);
        unacked_packets.clear();
    }
    
    std::cout << "Sessão estabelecida. SID recebido. Janela do Central: " << remote_window_size << std::endl;
}

/**
 * @brief Trata uma resposta de Setup com falha do Central.
 * @param packet O pacote de Reject recebido.
 */
void Peripheral::handleFailedResponse(const SlowPacket& packet) {
    std::cerr << "Falha na conexão: o Central rejeitou o pedido." << std::endl;
    current_state = DISCONNECTED;
    {
        std::lock_guard<std::mutex> lock(unacked_packets_mtx);
        unacked_packets.clear();
    }
}

/**
 * @brief Processa um pacote de Acknowledgement (ACK) do Central.
 *        Remove os pacotes confirmados da fila de retransmissão.
 * @param packet O pacote de ACK recebido.
 */
void Peripheral::handleAckResponse(const SlowPacket& packet) {
    uint32_t acknum = packet.getAcknowledgementNumber();
    remote_window_size = packet.getWindowSize();
    last_seqnum_from_central = packet.getSequenceNumber();

    {
        std::lock_guard<std::mutex> lock(unacked_packets_mtx);
        // O protocolo SLOW usa ACK cumulativo, então um ACK para N confirma todos os pacotes <= N.
        unacked_packets.erase(
            std::remove_if(unacked_packets.begin(), unacked_packets.end(),
                           [acknum](const auto& unacked) {
                               return unacked.packet.getSequenceNumber() <= acknum;
                           }),
            unacked_packets.end()
        );
    }
    
    // Se estávamos desconectando e agora não há mais pacotes pendentes, a desconexão está completa.
    if (current_state == DISCONNECTING && getUnackedPacketCount() == 0) {
        current_state = DISCONNECTED;
        std::cout << "Desconexão confirmada pelo central (todos os pacotes foram confirmados)." << std::endl;
    }
    
    std::cout << "ACK recebido (acknum=" << acknum << "). Pacotes em trânsito: " << getUnackedPacketCount() << std::endl;
}

/**
 * @brief Verifica a fila de pacotes não confirmados e retransmite aqueles cujo timeout expirou.
 */
void Peripheral::handleRetransmission() {
    // Cria uma cópia dos pacotes a serem retransmitidos fora do lock para evitar
    // chamar uma função de rede (sendTo) dentro de uma seção crítica.
    std::vector<SlowPacket> packets_to_retransmit;
    
    {
        std::lock_guard<std::mutex> lock(unacked_packets_mtx);
        auto now = std::chrono::steady_clock::now();
        for (auto& unacked : unacked_packets) {
            if (now - unacked.time_sent > RETRANSMISSION_TIMEOUT) {
                std::cout << "Timeout! Agendando retransmissão para pacote com seq=" << unacked.packet.getSequenceNumber() << std::endl;
                packets_to_retransmit.push_back(unacked.packet);
                unacked.time_sent = now; // Atualiza o timestamp para evitar retransmissões em rajada
            }
        }
    }

    // Envia os pacotes fora da seção crítica do mutex.
    for (const auto& packet : packets_to_retransmit) {
        auto raw_packet = packet.serialize();
        udp_socket.sendTo(raw_packet, central_ip, central_port);
    }
}

//==============================================================================
// MÉTODOS AUXILIARES (GETTERS)
//==============================================================================

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
    std::lock_guard<std::mutex> lock(unacked_packets_mtx);
    return unacked_packets.size();
}

/**
 * @brief Constrói e envia um pacote de conexão para o Central.
 * @return true se o envio foi iniciado, false se já estava em outro estado.
 */
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

    {
        std::lock_guard<std::mutex> lock(unacked_packets_mtx);
        unacked_packets.push_back({connect_packet, std::chrono::steady_clock::now()});
    }

    auto raw_packet = connect_packet.serialize();
    udp_socket.sendTo(raw_packet, central_ip, central_port);
    
    std::cout << "Pacote de conexão enviado. Aguardando resposta..." << std::endl;
    return true;
}