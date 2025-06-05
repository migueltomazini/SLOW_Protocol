// SlowPacket.cpp
// Este arquivo contém a implementação dos métodos declarados em SlowPacket.h.
// Aqui são detalhadas as lógicas para empacotar (serializar) e desempacotar (deserializar)
// os dados do cabeçalho e da carga útil (payload) do pacote SLOW, garantindo a
// conformidade com o formato de bits e a ordem little endian do protocolo.
// Também inclui a manipulação das flags e dos campos combinados (sttl e flags).

#include "SlowPacket.h"
#include "Utils.h"
#include <cstring> 
#include <iostream>

    // Inicializa o cabeçalho com todos os valores 0
    SlowPacket::SlowPacket() {
        std::memset(&header, 0, sizeof(SlowHeader));
    }
    // Construtor para criar um pacote a partir de bytes recebidos
    SlowPacket::SlowPacket(const std::vector<uint8_t>& raw_packet) {
        deserialize(raw_packet);
    }

    // Serializa o pacote para envio via UDP
    std::vector<uint8_t> SlowPacket::serialize() const {
        size_t total_size = sizeof(SlowHeader) + data.size();
        if (total_size > MAX_SLOW_PACKET_SIZE) {
            std::cerr << "Erro de serialização: Pacote excede o tamanho máximo permitido (" << MAX_SLOW_PACKET_SIZE << " bytes)." << std::endl;
            return {}; // Retorna um vetor vazio em caso de erro
        }

        std::vector<uint8_t> serialized_data(total_size);
        uint8_t* ptr = serialized_data.data();

        // Session ID (16 bytes)
        std::memcpy(ptr, header.sid.data(), sizeof(header.sid));
        ptr += sizeof(header.sid);

        // sttl_and_flags (4 bytes)
        uint32_t temp_sttl_and_flags = Utils::hostToLittleEndian32(header.sttl_and_flags);
        std::memcpy(ptr, &temp_sttl_and_flags, sizeof(header.sttl_and_flags));
        ptr += sizeof(header.sttl_and_flags);

        // seqnum (4 bytes)
        std::memcpy(ptr, &header.seqnum, sizeof(header.seqnum));
        ptr += sizeof(header.seqnum);

        // acknum (4 bytes)
        std::memcpy(ptr, &header.acknum, sizeof(header.acknum));
        ptr += sizeof(header.acknum);

        // window (2 bytes)
        std::memcpy(ptr, &header.window, sizeof(header.window));
        ptr += sizeof(header.window);

        // fid (1 byte)
        std::memcpy(ptr, &header.fid, sizeof(header.fid));
        ptr += sizeof(header.fid);

        // fo (1 byte)
        std::memcpy(ptr, &header.fo, sizeof(header.fo));
        ptr += sizeof(header.fo);

        // Data (variável)
        if (!data.empty()) {
            std::memcpy(ptr, data.data(), data.size());
        }

        return serialized_data;
    }
    
    // Deserializa bytes recebidos para preencher o pacote
    bool SlowPacket::deserialize(const std::vector<uint8_t>& raw_packet) {
        if (raw_packet.size() < sizeof(SlowHeader)) {
            std::cerr << "Erro de deserialização: Pacote recebido é muito pequeno para conter o cabeçalho SLOW." << std::endl;
            return false;
        }
        if (raw_packet.size() > MAX_SLOW_PACKET_SIZE) {
            std::cerr << "Erro de deserialização: Pacote recebido excede o tamanho máximo permitido (" << MAX_SLOW_PACKET_SIZE << " bytes)." << std::endl;
            return false;
        }

        const uint8_t* ptr = raw_packet.data();

        // Session ID (16 bytes)
        std::memcpy(header.sid.data(), ptr, sizeof(header.sid));
        ptr += sizeof(header.sid);

        // sttl_and_flags (4 bytes)
        std::memcpy(&header.sttl_and_flags, ptr, sizeof(header.sttl_and_flags));
        header.sttl_and_flags = Utils::littleEndianToHost32(header.sttl_and_flags);
        ptr += sizeof(header.sttl_and_flags);

        // seqnum (4 bytes)
        std::memcpy(&header.seqnum, ptr, sizeof(header.seqnum));
        header.seqnum = Utils::littleEndianToHost32(header.seqnum);
        ptr += sizeof(header.seqnum);

        // acknum (4 bytes)
        std::memcpy(&header.acknum, ptr, sizeof(header.acknum));
        header.acknum = Utils::littleEndianToHost32(header.acknum);
        ptr += sizeof(header.acknum);

        // window (2 bytes)
        std::memcpy(&header.window, ptr, sizeof(header.window));
        header.window = Utils::littleEndianToHost16(header.window);
        ptr += sizeof(header.window);

        // fid (1 byte)
        std::memcpy(&header.fid, ptr, sizeof(header.fid));
        ptr += sizeof(header.fid);

        // fo (1 byte)
        std::memcpy(&header.fo, ptr, sizeof(header.fo));
        ptr += sizeof(header.fo);

        // Data (o que sobrar após o cabeçalho)
        size_t data_size = raw_packet.size() - sizeof(SlowHeader);
        if (data_size > 0) {
            this->data.assign(ptr, ptr + data_size);
        } else {
            this->data.clear();
        }

        return true;
    }

    // Métodos auxiliares para definir e obter campos
    // Sttl
    void SlowHeader::setSttl(uint32_t sttl) {
        sttl &= 0x07FFFFFF; // Limita o valor de sttl aos 27 bits
        sttl_and_flags = (sttl_and_flags & 0xF8000000) | sttl;
    }
    
    uint32_t SlowHeader::getSttl() const {
        return sttl_and_flags & 0x07FFFFFF;
    }
    
    // Flags
    void SlowHeader::setFlag(SlowFlags flag, bool value) {
uint32_t flag_mask = 0;
        // Define a máscara de bit correta para a flag específica.
        switch (flag) {
            case FLAG_CONNECT:       flag_mask = 1U << 27; break;
            case FLAG_REVIVE:        flag_mask = 1U << 28; break;
            case FLAG_ACK:           flag_mask = 1U << 29; break;
            case FLAG_ACCEPT_REJECT: flag_mask = 1U << 30; break;
            case FLAG_MORE_BITS:     flag_mask = 1U << 31; break;
            default: return;
        }

        if (value) {
            // Se 'value' for true, liga o bit da flag usando OR.
            sttl_and_flags |= flag_mask;
        } else {
            // Se 'value' for false, desliga o bit da flag usando AND com o complemento.
            sttl_and_flags &= ~flag_mask;
        }
    }
    
    bool SlowHeader::getFlag(SlowFlags flag) const {
        uint32_t flag_mask = 0;

        // Define a máscara de bit correta para a flag específica.
        switch (flag) {
            case FLAG_CONNECT:       flag_mask = 1U << 27; break;
            case FLAG_REVIVE:        flag_mask = 1U << 28; break;
            case FLAG_ACK:           flag_mask = 1U << 29; break;
            case FLAG_ACCEPT_REJECT: flag_mask = 1U << 30; break;
            case FLAG_MORE_BITS:     flag_mask = 1U << 31; break;
            default: return false;
        }

        // Retorna true se o bit correspondente à flag estiver ligado.
        return (sttl_and_flags & flag_mask) != 0;
    }

    // Section ID
    void SlowPacket::setSessionID(const std::array<uint8_t, 16>& id) {
        header.sid = id;
    }

    std::array<uint8_t, 16> SlowPacket::getSessionID() const {
        return header.sid;
    }

    // Sequence Number
    void SlowPacket::setSequenceNumber(uint32_t num) {
        header.seqnum = Utils::hostToLittleEndian32(num);
    }

    uint32_t SlowPacket::getSequenceNumber() const {
        return Utils::littleEndianToHost32(header.seqnum);
    }

    // AcknowledgmentNumber
    void SlowPacket::setAcknowledgementNumber(uint32_t num) {
        header.acknum = Utils::hostToLittleEndian32(num);
    }

    uint32_t SlowPacket::getAcknowledgementNumber() const {
        return Utils::littleEndianToHost32(header.acknum);
    }

    // Window Size
    void SlowPacket::setWindowSize(uint16_t size) {
        header.window = Utils::hostToLittleEndian32(size);
    }

    uint16_t SlowPacket::getWindowSize() const {
        return Utils::littleEndianToHost32(header.window);
    }

    // Fragment ID
    void SlowPacket::setFragmentID(uint8_t id) {
        header.fid = Utils::hostToLittleEndian32(id);
    }

    uint8_t SlowPacket::getFragmentID() const {
        return Utils::littleEndianToHost32(header.fid);
    }

    // Fragement Offset
    void SlowPacket::setFragmentOffset(uint8_t offset) {
        header.fo = Utils::hostToLittleEndian32(offset);
    }

    uint8_t SlowPacket::getFragmentOffset() const {
    return Utils::littleEndianToHost32(header.fo);
    }

    // Data
    void SlowPacket::setData(const std::vector<uint8_t>& data_bytes) {
        if (data_bytes.size() > MAX_SLOW_DATA_SIZE) {
            std::cerr << "Erro: Dados excedem o tamanho máximo permitido para o payload." << std::endl;
            this->data.clear();
        } else {
            this->data = data_bytes;
        }
    }

    const std::vector<uint8_t>& SlowPacket::getData() const {
        return data;
    }