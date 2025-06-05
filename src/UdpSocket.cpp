// UdpSocket.cpp
// Este arquivo implementa as funcionalidades da classe UdpSocket.
// Ele contém o código para as chamadas de sistema POSIX (sockets) que permitem
// a comunicação UDP, como `socket()`, `bind()`, `sendto()` e `recvfrom()`.
// Garante que o peripheral possa enviar e receber pacotes SLOW
// através da rede, usando a porta UDP/7033 conforme especificado.