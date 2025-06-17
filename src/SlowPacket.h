/**
 * @file SlowPacket.h
 * @brief Definição da estrutura do pacote e da classe de manipulação para o protocolo SLOW.
 *
 * Especifica o layout do cabeçalho do protocolo, incluindo campos como Session ID,
 * Sequence Number e flags. A classe SlowPacket oferece métodos para serializar
 * um pacote em bytes para transmissão e deserializar bytes recebidos de volta
 * para uma estrutura de pacote, cuidando da conversão de endianness e da
 * manipulação de campos de bits.
 */

#ifndef SLOW_PACKET_H
#define SLOW_PACKET_H

#include <cstdint>
#include <vector>
#include <array>

// Constantes do protocolo SLOW
const uint16_t MAX_SLOW_PACKET_SIZE = 1472; // Tamanho máximo do pacote UDP
const uint16_t MAX_SLOW_DATA_SIZE = 1440;  // Tamanho máximo do payload de dados

/**
 * @enum SlowFlags
 * @brief Enumeração para as flags de 5 bits no cabeçalho SLOW.
 */
enum SlowFlags : uint32_t {
    FLAG_CONNECT       = 1 << 4, // Sinaliza um pacote de início de conexão ou desconexão.
    FLAG_REVIVE        = 1 << 3, // Sinaliza uma tentativa de reativação de sessão (0-way connect).
    FLAG_ACK           = 1 << 2, // Sinaliza que o pacote é (ou contém) um Acknowledgement.
    FLAG_ACCEPT_REJECT = 1 << 1, // Sinaliza uma resposta de aceitação/rejeição a uma conexão.
    FLAG_MORE_BITS     = 1 << 0  // Sinaliza que há mais fragmentos de dados a serem recebidos.
};

/**
 * @struct SlowHeader
 * @brief Estrutura que representa o cabeçalho de 32 bytes do protocolo SLOW.
 *        O pragma pack(1) garante que o compilador não adicione preenchimento (padding)
 *        entre os membros, mantendo o layout de bytes exato para a transmissão.
 */
#pragma pack(push, 1)
struct SlowHeader {
    std::array<uint8_t, 16> sid;    // 16 bytes: Session ID (UUIDv8)
    uint32_t sttl_and_flags;        //  4 bytes: sttl (27 bits) e flags (5 bits) combinados
    uint32_t seqnum;                //  4 bytes: Sequence Number
    uint32_t acknum;                //  4 bytes: Acknowledgement Number
    uint16_t window;                //  2 bytes: Window Size
    uint8_t fid;                    //  1 byte:  Fragment ID
    uint8_t fo;                     //  1 byte:  Fragment Offset

    // Funções para manipular sttl e flags de forma segura
    void setSttl(uint32_t sttl);
    uint32_t getSttl() const;
    void setFlag(SlowFlags flag, bool value);
    bool getFlag(SlowFlags flag) const;
};
#pragma pack(pop)

/**
 * @class SlowPacket
 * @brief Encapsula um cabeçalho SLOW e um payload de dados, fornecendo métodos
 *        para serialização e deserialização.
 */
class SlowPacket {
public:
    SlowHeader header;
    std::vector<uint8_t> data;

    SlowPacket();
    explicit SlowPacket(const std::vector<uint8_t>& raw_packet);

    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& raw_packet);

    // Métodos de acesso (getters/setters) para os campos do cabeçalho
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
    uint32_t getSttl() const;
};

#endif // SLOW_PACKET_H