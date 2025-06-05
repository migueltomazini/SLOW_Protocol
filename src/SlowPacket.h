// SlowPacket.h
// Este arquivo de cabeçalho define a estrutura fundamental do pacote SLOW.
// Ele especifica o layout do cabeçalho do protocolo, incluindo campos como
// Session ID, Sequence Number, Flags e informações de fragmentação/janela.
// Além disso, declara a classe SlowPacket que encapsula esses dados
// e oferece métodos para serializar e deserializar os pacotes em bytes.

#ifndef SLOW_PACKET_H
#define SLOW_PACKET_H

#include <cstdint>
#include <vector>
#include <array>

// Tamanho máximo do pacote SLOW
const uint16_t MAX_SLOW_PACKET_SIZE = 1472;
// Tamanho máximo do campo de dados
const uint16_t MAX_SLOW_DATA_SIZE = 1440;

// Flags SLOW (5 bits)
enum SlowFlags : uint8_t {
    FLAG_CONNECT = 0x08,        // C (bit 0)
    FLAG_REVIVE = 0x04,         // R (bit 1)
    FLAG_ACK = 0x02,            // ACK (bit 2)
    FLAG_ACCEPT_REJECT = 0x01,  // A/R (bit 3)
    FLAG_MORE_BITS = 0x10       // MB (bit 4)
};


// Estrutura do cabeçalho do pacote SLOW
#pragma pack(push, 1) // Garante que não haverá padding entre os campos
struct SlowHeader {
    std::array<uint8_t, 16> sid;    // Session ID (UUIDv8) - 128 bits
    uint32_t sttl_and_flags;        // sttl (27 bits) e flags (5 bits) combinados
    uint32_t seqnum;                // Sequence Number - 32 bits
    uint32_t acknum;                // Acknowledgement Number - 32 bits
    uint16_t window;                // Window Size - 16 bits
    uint8_t fid;                    // Fragment ID - 8 bits
    uint8_t fo;                     // Fragment Offset - 8 bits

    // Funções para manipular sttl e flags
    void setSttl(uint32_t sttl);
    uint32_t getSttl() const;
    void setFlag(SlowFlags flag, bool value);
    bool getFlag(SlowFlags flag) const;
};
#pragma pack(pop)

class SlowPacket {
public:
    SlowHeader header;
    std::vector<uint8_t> data; // Campo de dados, até 1440 bytes

    SlowPacket();
    // Construtor para criar um pacote a partir de bytes recebidos
    SlowPacket(const std::vector<uint8_t>& raw_packet);

    // Serializa o pacote para envio via UDP
    std::vector<uint8_t> serialize() const;
    
    // Deserializa bytes recebidos para preencher o pacote
    bool deserialize(const std::vector<uint8_t>& raw_packet);

    // Métodos auxiliares para definir e obter campos
    void setSessionID(const std::array<uint8_t, 16>& id);
    std::array<uint8_t, 16> getSessionID() const;

    void setSequenceNumber(uint32_t num);
    uint32_t getSequenceNumber() const;

    void setAcknowledgementNumber(uint32_t num);
    uint32_t getAcknowledgementNumber() const;

    void setWindowSize(uint16_t size);
    uint16_t getWindowSize() const;

    void setFragmentID(uint8_t id);
    uint8_t getFragmentID() const;

    void setFragmentOffset(uint8_t offset);
    uint8_t getFragmentOffset() const;

    void setData(const std::vector<uint8_t>& data_bytes);
    const std::vector<uint8_t>& getData() const;
};

#endif