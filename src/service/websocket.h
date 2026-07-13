#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// SHA-1 (RFC 3174) - Implementacao minima para o handshake WebSocket
// ============================================================================

static void sha1(const unsigned char *msg, size_t len, unsigned char digest[20]) {
    unsigned int h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE;
    unsigned int h3 = 0x10325476, h4 = 0xC3D2E1F0;
    
    // Pre-processamento: padding
    size_t new_len = len + 1;
    while (new_len % 64 != 56) new_len++;
    
    unsigned char *padded = (unsigned char *)calloc(new_len + 8, 1);
    if (!padded) return;
    memcpy(padded, msg, len);
    padded[len] = 0x80;
    
    // Tamanho original em bits (big-endian) nos ultimos 8 bytes
    unsigned long long bits_len = (unsigned long long)len * 8;
    for (int i = 0; i < 8; i++)
        padded[new_len + 7 - i] = (unsigned char)(bits_len >> (i * 8));
    
    size_t total = new_len + 8;
    
    // Processar blocos de 64 bytes
    for (size_t offset = 0; offset < total; offset += 64) {
        unsigned int w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = ((unsigned int)padded[offset + i*4] << 24) |
                    ((unsigned int)padded[offset + i*4+1] << 16) |
                    ((unsigned int)padded[offset + i*4+2] << 8) |
                    ((unsigned int)padded[offset + i*4+3]);
        }
        for (int i = 16; i < 80; i++) {
            unsigned int val = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
            w[i] = (val << 1) | (val >> 31);
        }
        
        unsigned int a = h0, b = h1, c = h2, d = h3, e = h4;
        
        for (int i = 0; i < 80; i++) {
            unsigned int f, k;
            if (i < 20)      { f = (b & c) | ((~b) & d);           k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                      k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d);    k = 0x8F1BBCDC; }
            else              { f = b ^ c ^ d;                      k = 0xCA62C1D6; }
            
            unsigned int temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = temp;
        }
        
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    
    free(padded);
    
    // Resultado em big-endian
    unsigned int hashes[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; i++) {
        digest[i*4]   = (unsigned char)(hashes[i] >> 24);
        digest[i*4+1] = (unsigned char)(hashes[i] >> 16);
        digest[i*4+2] = (unsigned char)(hashes[i] >> 8);
        digest[i*4+3] = (unsigned char)(hashes[i]);
    }
}

// ============================================================================
// Base64 Encode - Para gerar o Sec-WebSocket-Accept
// ============================================================================

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const unsigned char *input, size_t len, char *output) {
    size_t i, j = 0;
    for (i = 0; i + 2 < len; i += 3) {
        output[j++] = b64_table[(input[i] >> 2) & 0x3F];
        output[j++] = b64_table[((input[i] & 0x3) << 4) | (input[i+1] >> 4)];
        output[j++] = b64_table[((input[i+1] & 0xF) << 2) | (input[i+2] >> 6)];
        output[j++] = b64_table[input[i+2] & 0x3F];
    }
    if (i < len) {
        output[j++] = b64_table[(input[i] >> 2) & 0x3F];
        if (i + 1 < len) {
            output[j++] = b64_table[((input[i] & 0x3) << 4) | (input[i+1] >> 4)];
            output[j++] = b64_table[(input[i+1] & 0xF) << 2];
        } else {
            output[j++] = b64_table[(input[i] & 0x3) << 4];
            output[j++] = '=';
        }
        output[j++] = '=';
    }
    output[j] = '\0';
}

// ============================================================================
// WebSocket Handshake (RFC 6455)
// ============================================================================

// GUID magico definido pela RFC 6455 para o handshake
#define WS_MAGIC_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// Retorna 1 se o handshake foi bem-sucedido, 0 se falhou
static int ws_do_handshake(SOCKET client, const char *request) {
    // Procura o header "Sec-WebSocket-Key: " na requisicao HTTP
    const char *key_header = strstr(request, "Sec-WebSocket-Key: ");
    if (!key_header) return 0;
    
    key_header += 19; // Pula "Sec-WebSocket-Key: "
    
    // Extrai o valor da chave (ate o \r\n)
    char client_key[128] = {0};
    int i = 0;
    while (key_header[i] && key_header[i] != '\r' && key_header[i] != '\n' && i < 126) {
        client_key[i] = key_header[i];
        i++;
    }
    client_key[i] = '\0';
    
    // Concatena a chave do cliente com o GUID magico
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", client_key, WS_MAGIC_GUID);
    
    // SHA-1 da concatenacao
    unsigned char hash[20];
    sha1((const unsigned char *)combined, strlen(combined), hash);
    
    // Base64 do hash
    char accept_key[64];
    base64_encode(hash, 20, accept_key);
    
    // Monta e envia a resposta HTTP 101 Switching Protocols
    char response[512];
    snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept_key);
    
    send(client, response, (int)strlen(response), 0);
    return 1;
}

// ============================================================================
// WebSocket Frame - Leitura e Escrita
// ============================================================================

// Opcodes WebSocket
#define WS_OPCODE_TEXT   0x1
#define WS_OPCODE_BINARY 0x2
#define WS_OPCODE_CLOSE  0x8
#define WS_OPCODE_PING   0x9
#define WS_OPCODE_PONG   0xA

// Le um frame WebSocket do cliente. Retorna tamanho do payload ou -1 em erro/close
static int ws_recv_frame(SOCKET client, unsigned char *payload, int max_len, int *opcode) {
    unsigned char header[2];
    if (recv(client, (char *)header, 2, 0) != 2) return -1;
    
    *opcode = header[0] & 0x0F;
    int masked = (header[1] >> 7) & 1;
    unsigned long long payload_len = header[1] & 0x7F;
    
    // Payload estendido
    if (payload_len == 126) {
        unsigned char ext[2];
        if (recv(client, (char *)ext, 2, 0) != 2) return -1;
        payload_len = ((unsigned long long)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        unsigned char ext[8];
        if (recv(client, (char *)ext, 8, 0) != 8) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++)
            payload_len = (payload_len << 8) | ext[i];
    }
    
    if (payload_len > (unsigned long long)max_len) return -1;
    
    // Mascara (clientes DEVEM enviar mascarado)
    unsigned char mask[4] = {0};
    if (masked) {
        if (recv(client, (char *)mask, 4, 0) != 4) return -1;
    }
    
    // Recebe o payload
    int total = 0;
    while (total < (int)payload_len) {
        int r = recv(client, (char *)(payload + total), (int)(payload_len - total), 0);
        if (r <= 0) return -1;
        total += r;
    }
    
    // Decodifica a mascara XOR
    if (masked) {
        for (int i = 0; i < total; i++)
            payload[i] ^= mask[i % 4];
    }
    
    return total;
}

// Envia um frame WebSocket para o cliente (servidor NAO mascara)
static int ws_send_frame(SOCKET client, int opcode, const unsigned char *payload, int len) {
    unsigned char header[10];
    int header_len = 0;
    
    header[0] = 0x80 | (opcode & 0x0F); // FIN + opcode
    
    if (len < 126) {
        header[1] = (unsigned char)len;
        header_len = 2;
    } else if (len < 65536) {
        header[1] = 126;
        header[2] = (unsigned char)(len >> 8);
        header[3] = (unsigned char)(len & 0xFF);
        header_len = 4;
    } else {
        header[1] = 127;
        memset(header + 2, 0, 4);
        header[6] = (unsigned char)(len >> 24);
        header[7] = (unsigned char)(len >> 16);
        header[8] = (unsigned char)(len >> 8);
        header[9] = (unsigned char)(len);
        header_len = 10;
    }
    
    if (send(client, (char *)header, header_len, 0) != header_len) return -1;
    if (len > 0) {
        if (send(client, (const char *)payload, len, 0) != len) return -1;
    }
    return 0;
}

// Envia uma mensagem de texto WebSocket (wrapper conveniente)
static int ws_send_text(SOCKET client, const char *text) {
    return ws_send_frame(client, WS_OPCODE_TEXT, (const unsigned char *)text, (int)strlen(text));
}

#endif // WEBSOCKET_H
