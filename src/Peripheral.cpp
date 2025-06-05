// Peripheral.cpp
// Este arquivo contém a implementação detalhada da classe Peripheral.
// Ele implementa as lógicas de alto nível do protocolo, como o 3-way connect,
// o tratamento de ACKs, retransmissões, fragmentação e a atualização da janela deslizante.
// Aqui são definidos os algoritmos e estados que governam o comportamento do peripheral
// para garantir a comunicação confiável e ordenada com o central.

#include "Peripheral.h"
#include "Utils.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <thread>

/**
 * @brief Construtor da classe Peripheral
 * Inicializa os membros da classe com valores padrão e configura o endereço do central.
 * @param central_ip: Endereço IP ou hostname do servidor central.
 * @param central_port: Porta UDP do servidor central.
 */
Peripheral::Peripheral(const std::string& central_ip, int central_port)
    : central_ip(central_ip), central_port(central_port),
      current_state(DISCONNECTED), current_seqnum(0),
      last_acknum_received(0), remote_window_size(0),
      local_window_size(10000) { 
    // Inicia com um Nil UUID, pois ainda não há sessão estabelecida.
    session_id = Utils::generateNilUUID(); 
}

// Destrutor da classe Peripheral
Peripheral::~Peripheral() {
}

/**
 * @brief Inicia o socket UDP do peripheral e o liga à porta SLOW_PORT.
 * Esta é a fase inicial para o peripheral poder enviar e receber dados na rede.
 * @return true se o socket foi ligado com sucesso, false caso contrário.
 */
bool Peripheral::start() {
    // Tenta ligar o socket UDP à porta SLOW_PORT (7033).
    if (!udp_socket.bindSocket(SLOW_PORT)) {
        std::cerr << "Falha ao iniciar o socket do peripheral." << std::endl;
        return false;
    }
    std::cout << "Peripheral iniciado na porta " << SLOW_PORT << std::endl;
    return true;
}

/**
 * @brief Loop principal de execução do peripheral.
 * Este método simula um loop de eventos: continuamente recebe e processa pacotes,
 * e gerencia a lógica de retransmissão e atualização da janela deslizante.
 */
void Peripheral::run() {
    std::cout << "Peripheral iniciando loop de execucao..." << std::endl;

    // Loop principal para processar eventos de rede.
    while (true) {
        std::vector<uint8_t> buffer; 
        std::string sender_ip;     // Endereço IP do remetente.
        int sender_port;           // Porta do remetente.

        // Tenta receber um pacote UDP.
        ssize_t bytes_received = udp_socket.receiveFrom(buffer, sender_ip, sender_port);

        if (bytes_received > 0) {
            buffer.resize(bytes_received);
            processReceivedPacket(buffer);
        } else if (bytes_received < 0) {
            std::cerr << "Erro ao receber pacote UDP." << std::endl;
        }

        // Executa a lógica de retransmissão para pacotes não confirmados.
        handleRetransmission();
        // Atualiza a janela efetiva do central, considerando pacotes em trânsito.
        updateEffectiveWindow();

        // Pequena pausa para evitar alto consumo de CPU em um loop muito rápido.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/**
 * @brief Envia um pacote CONNECT para iniciar uma conexão 3-way com o central.
 * Configura o pacote com os valores iniciais exigidos para o handshake de conexão.
 * @return true se o pacote foi enviado com sucesso, false caso contrário.
 */
bool Peripheral::sendConnect() {
    SlowPacket connect_packet;
    // sid: Nil UUID para o Connect.
    connect_packet.setSessionID(Utils::generateNilUUID());
    // sttl: 0 para o Connect.
    connect_packet.header.setSttl(0);
    // flags: A flag CONNECT é ligada para indicar a intenção de iniciar uma conexão.
    connect_packet.header.setFlag(FLAG_CONNECT, true);
    // seqnum: 0 para o primeiro pacote da sessão.
    connect_packet.setSequenceNumber(0);
    // acknum: 0, pois ainda não há pacotes para confirmar.
    connect_packet.setAcknowledgementNumber(0);
    // window: Reflete o tamanho do buffer de recebimento do peripheral.
    connect_packet.setWindowSize(local_window_size);
    // fid: 0.
    connect_packet.setFragmentID(0);
    // fo: 0.
    connect_packet.setFragmentOffset(0);
    // data: Campo inexistente.

    std::vector<uint8_t> serialized_packet = connect_packet.serialize();
    if (serialized_packet.empty()) {
        std::cerr << "Falha na serialização do pacote CONNECT." << std::endl;
        return false;
    }

    // Envia o pacote serializado para o central.
    ssize_t sent_bytes = udp_socket.sendTo(serialized_packet, central_ip, central_port);

    if (sent_bytes > 0) {
        std::cout << "Pacote CONNECT (seqnum: " << connect_packet.getSequenceNumber() << ") enviado." << std::endl;
        // Altera o estado para CONNECTING, aguardando resposta.
        current_state = CONNECTING; 
        return true;
    } else {
        std::cerr << "Erro ao enviar pacote CONNECT." << std::endl;
        return false;
    }
}

/**
 * @brief Envia pacotes de DATA para o central, com suporte a fragmentação.
 * Divide a carga útil em fragmentos menores se excederem o tamanho máximo do pacote.
 * @param data_payload: O vetor de bytes contendo os dados a serem enviados.
 * @return true se todos os pacotes/fragmentos foram enviados com sucesso, false caso contrário.
 */
bool Peripheral::sendData(const std::vector<uint8_t>& data_payload) {
    if (current_state != CONNECTED && current_state != CONNECTING) {
        std::cerr << "Não é possível enviar dados: peripheral não está em estado conectado ou conectando." << std::endl;
        return false;
    }
    if (data_payload.empty()) {
        std::cout << "Nenhum dado para enviar." << std::endl;
        return true;
    }

    bool all_sent_successfully = true;
    // next_fid é estático para gerar um ID único para cada nova mensagem lógica (fragmentada ou não).
    static uint8_t next_fid = 0;
    uint8_t current_fid = next_fid++;

    size_t bytes_sent_for_message = 0;
    size_t total_payload_size = data_payload.size();
    uint8_t current_fo = 0;

    // Loop para fragmentar e enviar os dados.
    while (bytes_sent_for_message < total_payload_size) {
        SlowPacket data_packet;
        data_packet.setSessionID(session_id); // sid: UUID da sessão atual.
        data_packet.header.setSttl(0); // sttl: 0, pois é ignorado pelo central vindo do peripheral.
        data_packet.header.setFlag(FLAG_ACK, true); // flags: ACK ligado para confirmar o próximo recebimento.

        // Determina o tamanho do fragmento atual, limitado por MAX_SLOW_DATA_SIZE.
        size_t fragment_size = std::min((size_t)MAX_SLOW_DATA_SIZE, total_payload_size - bytes_sent_for_message);
        std::vector<uint8_t> current_data_segment(
            data_payload.begin() + bytes_sent_for_message,
            data_payload.begin() + bytes_sent_for_message + fragment_size
        );
        data_packet.setData(current_data_segment);

        // Define a flag More Bits (MB).
        // Se ainda houver dados a serem enviados após este fragmento, a flag MB é ligada.
        if (bytes_sent_for_message + fragment_size < total_payload_size) {
            data_packet.header.setFlag(FLAG_MORE_BITS, true);
        } else { // Último fragmento: MB desligado.
            data_packet.header.setFlag(FLAG_MORE_BITS, false);
        }

        // Atualização dos parâmetros do cabeçalho
        data_packet.setSequenceNumber(++current_seqnum);
        data_packet.setAcknowledgementNumber(last_acknum_received);
        data_packet.setWindowSize(local_window_size); 
        data_packet.setFragmentID(current_fid); 
        data_packet.setFragmentOffset(current_fo);

        // Serialização dos dados
        std::vector<uint8_t> serialized_packet = data_packet.serialize();
        if (serialized_packet.empty()) {
            std::cerr << "Falha na serialização de um fragmento de DATA (seqnum: " << data_packet.getSequenceNumber() << ")." << std::endl;
            all_sent_successfully = false;
            break;
        }

        // Envia o fragmento serializado.
        ssize_t sent_bytes = udp_socket.sendTo(serialized_packet, central_ip, central_port);

        if (sent_bytes > 0) {
            std::cout << "Pacote DATA (seqnum: " << data_packet.getSequenceNumber()
                      << ", fid: " << (int)data_packet.getFragmentID()
                      << ", fo: " << (int)data_packet.getFragmentOffset() << ") enviado." << std::endl;
            // Adiciona ao buffer de pacotes não confirmados (para retransmissão).
            unacked_packets.push_back(data_packet); 
        } else {
            std::cerr << "Erro ao enviar um fragmento de DATA (seqnum: " << data_packet.getSequenceNumber() << ")." << std::endl;
            all_sent_successfully = false;
            break;
        }

        bytes_sent_for_message += fragment_size;
        current_fo++;
    }

    return all_sent_successfully;
}

/**
 * @brief Envia um pacote DISCONNECT para o central, indicando o fim da sessão.
 * A sessão entra em modo inativo e pode ser reativada via 0-way connect.
 * @return true se o pacote foi enviado com sucesso, false caso contrário.
 */
bool Peripheral::sendDisconnect() {
    if (current_state != CONNECTED) {
        std::cerr << "Não é possível enviar DISCONNECT: peripheral não está conectado." << std::endl;
        return false;
    }

    SlowPacket disconnect_packet;
    // sid: UUID da sessão atual.
    disconnect_packet.setSessionID(session_id); 
    // sttl: 0.
    disconnect_packet.header.setSttl(0); 
    // flags: ACK, CONNECT e REVIVE são ligadas simultaneamente para simbolizar DISCONNECT.
    disconnect_packet.header.setFlag(FLAG_ACK, true);
    disconnect_packet.header.setFlag(FLAG_CONNECT, true);
    disconnect_packet.header.setFlag(FLAG_REVIVE, true);
    // seqnum: Próximo número de sequência.
    disconnect_packet.setSequenceNumber(++current_seqnum); 
    // acknum: Último pacote recebido.
    disconnect_packet.setAcknowledgementNumber(last_acknum_received); 
    // window: 0, pois não espera mais dados.
    disconnect_packet.setWindowSize(0); 
    // fid: 0.
    disconnect_packet.setFragmentID(0); 
    // fo: 0.
    disconnect_packet.setFragmentOffset(0); 
    // data: Campo inexistente para esta mensagem.

    std::vector<uint8_t> serialized_packet = disconnect_packet.serialize();
    if (serialized_packet.empty()) {
        std::cerr << "Falha na serialização do pacote DISCONNECT." << std::endl;
        return false;
    }

    // Envia o pacote serializado.
    ssize_t sent_bytes = udp_socket.sendTo(serialized_packet, central_ip, central_port);

    if (sent_bytes > 0) {
        std::cout << "Pacote DISCONNECT (seqnum: " << disconnect_packet.getSequenceNumber() << ") enviado." << std::endl;
        current_state = DISCONNECTING; 
        return true;
    } else {
        std::cerr << "Erro ao enviar pacote DISCONNECT." << std::endl;
        return false;
    }
}

/**
 * @brief Envia um pacote de DATA com a flag REVIVE ligada para tentar reiniciar uma sessão (0-way connect).
 * Usado quando uma sessão anterior está inativa mas ainda pode ser reativada.
 * @param data_payload: Os dados a serem enviados junto com a requisição de revive.
 * @return true se o pacote foi enviado com sucesso, false caso contrário.
 */
bool Peripheral::sendReviveData(const std::vector<uint8_t>& data_payload) {
    // Apenas sessões DISCONNECTED podem ser revividas.
    if (current_state != DISCONNECTED) { 
        std::cerr << "Não é possível enviar REVIVE DATA: sessão não está DISCONNECTED." << std::endl;
        return false;
    }
    if (session_id == Utils::generateNilUUID()) {
        std::cerr << "Não é possível enviar REVIVE DATA: Session ID não é válido (Nil UUID para revive)." << std::endl;
        return false;
    }
    if (data_payload.empty()) {
        std::cerr << "Payload de dados vazio para REVIVE DATA." << std::endl;
        return false;
    }

    static uint8_t next_revive_fid = 0;
    uint8_t current_revive_fid = next_revive_fid++;

    // Validação de tamanho
    if (data_payload.size() > MAX_SLOW_DATA_SIZE) {
        std::cerr << "Erro: Payload de REVIVE DATA muito grande. Fragmentação não implementada para REVIVE." << std::endl;
        return false;
    }

    SlowPacket revive_data_packet;
    // sid: UUID da sessão anterior.
    revive_data_packet.setSessionID(session_id); 
    // sttl: 0, ignorado pelo central.
    revive_data_packet.header.setSttl(0); 
    // flags: ACK e REVIVE são ligadas.
    revive_data_packet.header.setFlag(FLAG_ACK, true); 
    revive_data_packet.header.setFlag(FLAG_REVIVE, true);
    // MB: Falso.
    revive_data_packet.header.setFlag(FLAG_MORE_BITS, false); 
    // seqnum: Próximo número de sequência.
    revive_data_packet.setSequenceNumber(++current_seqnum); 
    // acknum: Último pacote recebido.
    revive_data_packet.setAcknowledgementNumber(last_acknum_received); 
    // window: Janela local.
    revive_data_packet.setWindowSize(local_window_size); 
    // fid: ID do fragmento.
    revive_data_packet.setFragmentID(current_revive_fid); 
    // fo: 0.
    revive_data_packet.setFragmentOffset(0); 
    // data: Carga útil.
    revive_data_packet.setData(data_payload); 

    std::vector<uint8_t> serialized_packet = revive_data_packet.serialize();
    if (serialized_packet.empty()) {
        std::cerr << "Falha na serialização do pacote REVIVE DATA." << std::endl;
        return false;
    }

    // Envia o pacote serializado.
    ssize_t sent_bytes = udp_socket.sendTo(serialized_packet, central_ip, central_port);

    if (sent_bytes > 0) {
        std::cout << "Pacote REVIVE DATA (seqnum: " << revive_data_packet.getSequenceNumber() << ") enviado." << std::endl;
        unacked_packets.push_back(revive_data_packet); 
        current_state = CONNECTING;
        return true;
    } else {
        std::cerr << "Erro ao enviar pacote REVIVE DATA." << std::endl;
        return false;
    }
}

/**
 * @brief Processa um pacote SLOW recebido do central.
 * Deserializa o pacote e o encaminha para a função de tratamento apropriada
 * com base nas flags e no estado da conexão.
 * @param raw_packet: O vetor de bytes do pacote recebido.
 */
void Peripheral::processReceivedPacket(const std::vector<uint8_t>& raw_packet) {
    SlowPacket received_packet;
    if (!received_packet.deserialize(raw_packet)) {
        std::cerr << "Falha ao deserializar pacote recebido." << std::endl;
        return;
    }

    last_acknum_received = received_packet.getSequenceNumber();
    remote_window_size = received_packet.getWindowSize();

    // Lógica de encaminhamento baseada nas flags do pacote.
    if (received_packet.header.getFlag(FLAG_ACCEPT_REJECT)) {
        // Se a flag ACCEPT_REJECT estiver ligada, é uma resposta de Setup ou Failed.
        if (received_packet.header.getFlag(FLAG_ACK)) {
            handleSetupResponse(received_packet);
        } else {
            handleFailedResponse(received_packet);
        }
    } else if (received_packet.header.getFlag(FLAG_ACK)) {
        // Se a flag ACK estiver ligada, é uma confirmação de recebimento de dados.
        handleAckResponse(received_packet);
    } else if (received_packet.header.getFlag(FLAG_MORE_BITS)) {
        std::cout << "Recebido fragmento de dados do central." << std::endl;
    }
}

/**
 * @brief Lida com as respostas de SETUP (aceitação de conexão) ou ACK de um REVIVE aceito.
 * @param packet: O pacote recebido com a resposta de SETUP/ACK.
 */
void Peripheral::handleSetupResponse(const SlowPacket& packet) {
    // Verifica se o pacote é uma aceitação de conexão (FLAG_ACCEPT_REJECT e FLAG_ACK ligadas).
    if (packet.header.getFlag(FLAG_ACCEPT_REJECT) && packet.header.getFlag(FLAG_ACK)) {
        // Se o peripheral estava tentando conectar (3-way)
        if (current_state == CONNECTING) {
            session_id = packet.getSessionID();
            current_seqnum = packet.getSequenceNumber();
            current_state = CONNECTED;
            std::cout << "Conexão estabelecida com o Central" << std::endl;
        // Se foi um ACK de um revive bem-sucedido (FLAG_REVIVE também ligada).
        } else if (current_state == DISCONNECTED && packet.header.getFlag(FLAG_REVIVE)) {
            session_id = packet.getSessionID();
            current_seqnum = packet.getSequenceNumber();
            current_state = CONNECTED;
            std::cout << "Sessão revivida com o Central" << std::endl;
        }
    }
}

/**
 * @brief Lida com pacotes ACK recebidos do central.
 * Remove os pacotes que foram confirmados do buffer de pacotes não confirmados (`unacked_packets`).
 * @param packet: O pacote ACK recebido.
 */
void Peripheral::handleAckResponse(const SlowPacket& packet) {
    // acknum: Número de sequência do pacote que está sendo confirmado.
    uint32_t acknum = packet.getAcknowledgementNumber();

    // Remove do buffer todos os pacotes cujo número de sequência é menor ou igual ao acknum.
    unacked_packets.erase(
        std::remove_if(unacked_packets.begin(), unacked_packets.end(),
                       [acknum](const SlowPacket& p) {
                           return p.getSequenceNumber() <= acknum;
                       }),
        unacked_packets.end()
    );
    std::cout << "ACK recebido para seqnum: " << acknum << ". Pacotes não confirmados restantes: " << unacked_packets.size() << std::endl;

    // Se o ACK recebido for para um pacote DISCONNECT (C, R, ACK ligadas), finaliza o estado.
    if (current_state == DISCONNECTING &&
        packet.header.getFlag(FLAG_CONNECT) && packet.header.getFlag(FLAG_REVIVE) && packet.header.getFlag(FLAG_ACK)) {
        current_state = DISCONNECTED;
        session_id = Utils::generateNilUUID();
        current_seqnum = 0;
        last_acknum_received = 0;              
        std::cout << "Desconectado do Central com sucesso." << std::endl;
    }
}

/**
 * @brief Lida com pacotes FAILED recebidos do central, indicando uma falha.
 * @param packet: O pacote FAILED recebido.
 */
void Peripheral::handleFailedResponse(const SlowPacket& packet) {
    // Verifica se a flag ACCEPT_REJECT está desligada (0), indicando uma rejeição/falha.
    if (!packet.header.getFlag(FLAG_ACCEPT_REJECT)) {
        std::cerr << "Recebido pacote FAILED do Central (revive rejeitado)." << std::endl;
        current_state = DISCONNECTED;
        session_id = Utils::generateNilUUID();
        current_seqnum = 0;
        last_acknum_received = 0;
    }
}

/**
 * @brief Atualiza a janela efetiva do central para controle de fluxo.
 * A janela efetiva é calculada como a janela reportada pelo central
 * menos o total de bytes dos pacotes que estão em trânsito (não confirmados).
 */
void Peripheral::updateEffectiveWindow() {
    size_t bytes_in_flight = 0;
    // Soma o tamanho dos dados de todos os pacotes no buffer de não confirmados.
    for (const auto& packet : unacked_packets) {
        bytes_in_flight += packet.getData().size();
    }
    uint16_t effective_window = (remote_window_size > bytes_in_flight) ? (remote_window_size - bytes_in_flight) : 0;
}

/**
 * @brief Lida com a retransmissão de pacotes.
 * É essencial para garantir a confiabilidade do protocolo sobre UDP.
 */
void Peripheral::handleRetransmission() {
    // Implementação pendente:
    // 1. Iterar sobre `unacked_packets`.
    // 2. Para cada pacote, verificar se seu tempo de envio excedeu um `RETRANSMISSION_TIMEOUT`.
    // 3. Se excedeu, retransmitir o pacote e atualizar seu tempo de envio.
    // 4. Implementar um limite de tentativas de retransmissão para cada pacote.
}

/**
 * @brief Tenta remontar um pacote completo a partir de fragmentos recebidos.
 * Armazena os fragmentos em um buffer e, quando o último fragmento é detectado (MB=0),
 * reconstrói a mensagem original.
 * @param fragment_packet: O fragmento de pacote SLOW recebido.
 * @return true se um pacote completo foi remontado com sucesso, false caso contrário.
 */
bool Peripheral::assembleFragmentedPacket(const SlowPacket& fragment_packet) {
    uint8_t fid = fragment_packet.getFragmentID(); // Fragment ID: ID da mensagem à qual o fragmento pertence.
    uint8_t fo = fragment_packet.getFragmentOffset(); // Fragment Offset: Ordem do fragmento na mensagem.

    fragmented_data_buffer[fid][fo] = fragment_packet.getData();

    // Verifica se este é o último fragmento da mensagem (flag MORE_BITS desligada).
    if (!fragment_packet.header.getFlag(FLAG_MORE_BITS)) {
        std::vector<uint8_t> full_data;
        for (const auto& pair : fragmented_data_buffer[fid]) {
            full_data.insert(full_data.end(), pair.second.begin(), pair.second.end());
        }
        std::cout << "Pacote completo remontado para FID: " << (int)fid << ". Tamanho: " << full_data.size() << " bytes." << std::endl;

        fragmented_data_buffer.erase(fid);
        return true;
    }
    return false;
}