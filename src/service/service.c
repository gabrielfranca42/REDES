#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

// Inclui nossas funcoes de WebSocket (SHA-1, Base64, Handshake, Framing)
#include "websocket.h"

// Inclui a funcao de captura de tela (CapturarTela)
#include "tela.h"

// Inclui as funcoes de injecao de mouse e teclado (SendInput)
#include "input.h"

// Vincula a biblioteca Winsock (necessária para utilizar sockets no Windows)
#pragma comment(lib, "ws2_32.lib")

// Nome de registro do serviço
#define SERVICE_NAME TEXT("MeuServicoSystem")

// Porta do servidor WebSocket
#define WS_PORT 4444

// Tamanho maximo do buffer de leitura
#define BUFFER_SIZE 4096

// Variáveis globais para gerenciar o estado e comunicação do serviço com o SCM (Service Control Manager)
SERVICE_STATUS g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;

// Protótipos das funções do serviço
VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);
DWORD WINAPI HandleClientThread(LPVOID lpParam);

int main() {
    // Tabela que associa o nome do serviço à sua respectiva função de entrada (ServiceMain)
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };

    // Conecta o thread principal do processo ao Service Control Manager (SCM) do Windows
    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE) {
        return GetLastError();
    }

    return 0;
}

// Ponto de entrada do serviço do Windows
VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv) {
    // Registra a função controladora (handler) que lida com requisições como STOP ou PAUSE
    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

    if (g_StatusHandle == NULL) {
        return;
    }

    // Configura o estado inicial do serviço (carregando/iniciando)
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Cria um evento sinalizável para notificar as threads de trabalho quando o serviço deve parar
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (g_ServiceStopEvent == NULL) {
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    // Atualiza o estado do serviço para em execução (SERVICE_RUNNING)
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Cria a thread que executa o servidor WebSocket
    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

    // Aguarda até que o evento g_ServiceStopEvent seja sinalizado
    WaitForSingleObject(g_ServiceStopEvent, INFINITE);

    // Atualiza o estado para parado (SERVICE_STOPPED) após receber o sinal de parada
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

// Manipulador de comandos enviados pelo SCM (ex: botão parar do Services.msc)
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    switch (CtrlCode) {
        case SERVICE_CONTROL_STOP:
            // Define o estado como "parada pendente" e sinaliza o evento para parar a thread de trabalho
            g_ServiceStatus.dwWin32ExitCode = 0;
            g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
            
            // Sinaliza o evento global que diz a todas as threads para finalizarem
            SetEvent(g_ServiceStopEvent);
            break;
        default:
            break;
    }
}

// ============================================================================
// Thread que lida com um cliente WebSocket individual
// ============================================================================
DWORD WINAPI HandleClientThread(LPVOID lpParam) {
    SOCKET ClientSocket = (SOCKET)(uintptr_t)lpParam;
    char request[BUFFER_SIZE];
    
    // 1. Recebe a requisicao HTTP de upgrade do cliente
    int bytes = recv(ClientSocket, request, BUFFER_SIZE - 1, 0);
    if (bytes <= 0) {
        closesocket(ClientSocket);
        return 1;
    }
    request[bytes] = '\0';
    
    // 2. Verifica se e uma requisicao WebSocket e faz o handshake (RFC 6455)
    if (strstr(request, "Upgrade: websocket") == NULL &&
        strstr(request, "Upgrade: Websocket") == NULL) {
        // Nao e WebSocket - responde com HTTP simples informando que e um servidor WS
        const char *http_resp = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Este servidor aceita apenas conexoes WebSocket na porta 4444.\r\n";
        send(ClientSocket, http_resp, (int)strlen(http_resp), 0);
        closesocket(ClientSocket);
        return 0;
    }
    
    // 3. Realiza o handshake WebSocket (gera Sec-WebSocket-Accept via SHA-1 + Base64)
    if (!ws_do_handshake(ClientSocket, request)) {
        closesocket(ClientSocket);
        return 1;
    }
    
    // 4. Envia mensagem de boas-vindas ao cliente via WebSocket
    ws_send_text(ClientSocket, "Conexao WebSocket estabelecida! Bem-vindo ao servico SYSTEM.");
    
    // 5. Loop de comunicacao WebSocket (echo server)
    //    Recebe mensagens do cliente e responde com eco
    unsigned char payload[BUFFER_SIZE];
    int opcode;
    
    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0) {
        // Usa select para nao bloquear indefinidamente
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(ClientSocket, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(0, &readfds, NULL, NULL, &timeout);
        if (activity == SOCKET_ERROR) break;
        if (activity == 0) continue; // Timeout, volta pro loop
        
        // Le um frame WebSocket
        int payload_len = ws_recv_frame(ClientSocket, payload, BUFFER_SIZE - 1, &opcode);
        
        if (payload_len < 0 || opcode == WS_OPCODE_CLOSE) {
            // Cliente desconectou ou enviou frame de close
            break;
        }
        
        if (opcode == WS_OPCODE_PING) {
            // Responde PING com PONG (mesmo payload)
            ws_send_frame(ClientSocket, WS_OPCODE_PONG, payload, payload_len);
            continue;
        }
        
        if (opcode == WS_OPCODE_TEXT) {
            // Mensagem de texto recebida
            payload[payload_len] = '\0';
            
            // Verifica se o cliente enviou o comando de captura de tela
            if (strcmp((char *)payload, "CAPTURAR_TELA") == 0) {
                // Executa a captura de tela e recebe os bytes na memória
                ScreenCapture captura = CapturarTela();
                if (captura.dados != NULL) {
                    // Envia os bytes do BMP como frame binário (opcode 0x02)
                    ws_send_frame(ClientSocket, WS_OPCODE_BINARY, captura.dados, captura.tamanho);
                    // Libera a memória alocada pela CapturarTela
                    free(captura.dados);
                } else {
                    ws_send_text(ClientSocket, "[SYSTEM] Erro ao capturar a tela.");
                }
            // Comandos de controle remoto: MOUSE_CLICK x y
            } else if (strncmp((char *)payload, "MOUSE_CLICK ", 12) == 0) {
                int x, y;
                if (sscanf((char *)payload + 12, "%d %d", &x, &y) == 2) {
                    InjetarMouseClick(x, y);
                    ws_send_text(ClientSocket, "[SYSTEM] Click executado.");
                }

            // MOUSE_RCLICK x y
            } else if (strncmp((char *)payload, "MOUSE_RCLICK ", 13) == 0) {
                int x, y;
                if (sscanf((char *)payload + 13, "%d %d", &x, &y) == 2) {
                    InjetarMouseRightClick(x, y);
                    ws_send_text(ClientSocket, "[SYSTEM] Right-click executado.");
                }

            // MOUSE_MOVE x y
            } else if (strncmp((char *)payload, "MOUSE_MOVE ", 11) == 0) {
                int x, y;
                if (sscanf((char *)payload + 11, "%d %d", &x, &y) == 2) {
                    InjetarMouseMove(x, y);
                }

            // KEY_PRESS vkCode
            } else if (strncmp((char *)payload, "KEY_PRESS ", 10) == 0) {
                int vk;
                if (sscanf((char *)payload + 10, "%d", &vk) == 1) {
                    InjetarTecla((WORD)vk);
                    ws_send_text(ClientSocket, "[SYSTEM] Tecla executada.");
                }

            } else {
                // Mensagem normal - responde com eco (logica original preservada)
                char response[BUFFER_SIZE + 64];
                snprintf(response, sizeof(response), "[SYSTEM Echo] %s", (char *)payload);
                ws_send_text(ClientSocket, response);
            }
        }
    }
    
    closesocket(ClientSocket);
    return 0;
}

// ============================================================================
// Thread principal do servidor WebSocket (aceita conexoes)
// ============================================================================
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam) {
    WSADATA wsaData;
    SOCKET ListenSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddr, clientAddr;
    int clientAddrSize = sizeof(clientAddr);

    // Inicializa a API Winsock do Windows
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return 1;
    }

    // Cria o socket TCP (IPv4, Stream de Dados, protocolo TCP)
    ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    // Define os parâmetros de conexão local (Porta WS_PORT e aceitar de qualquer IP local)
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(WS_PORT);

    // Associa o socket ao endereço de IP e porta configurados
    if (bind(ListenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // Coloca o socket em modo de escuta para conexões pendentes
    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // Loop que roda enquanto o evento de parada do serviço NÃO for sinalizado
    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(ListenSocket, &readfds);
        
        // Define tempo limite de 1 segundo para a chamada do select
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // Monitora se há novas solicitações de conexão de entrada
        int activity = select(0, &readfds, NULL, NULL, &timeout);

        if (activity == SOCKET_ERROR) {
            break;
        }

        // Se houver conexões pendentes para serem aceitas
        if (activity > 0 && FD_ISSET(ListenSocket, &readfds)) {
            SOCKET ClientSocket = accept(ListenSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
            if (ClientSocket != INVALID_SOCKET) {
                // Cria uma thread separada para cada cliente WebSocket
                // Isso permite multiplas conexoes simultaneas
                CreateThread(NULL, 0, HandleClientThread, 
                           (LPVOID)(uintptr_t)ClientSocket, 0, NULL);
            }
        }
    }

    // Encerra os sockets e limpa a biblioteca Winsock ao parar o serviço
    closesocket(ListenSocket);
    WSACleanup();

    return 0;
}
