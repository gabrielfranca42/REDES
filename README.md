# C_REDES - Mini AnyDesk em C (Windows Service + WebSocket)

Este projeto é um sistema de **acesso remoto** construído do zero em linguagem C, que implementa um Serviço do Windows (Windows Service) com um servidor WebSocket nativo embutido. Ele roda em background com privilégios elevados (SYSTEM), permitindo **captura de tela em tempo real**, **controle remoto de mouse e teclado**, e comunicação bidirecional com clientes através do protocolo WebSocket.

## 🏗️ System Design e Arquitetura

A arquitetura do projeto é dividida nos seguintes componentes:

### 1. Launcher / Instalador (`src/launcher/launcher.c`)
*   **Função:** Responsável por instalar e iniciar o serviço no sistema operacional.
*   **Funcionamento:** Descobre o caminho absoluto do executável `service.exe`, comunica-se com o Service Control Manager (SCM) do Windows para registrar o serviço (nome interno `MeuServicoSystem`, nome de exibição `Meu Servico de Otimizacao Core`) configurando-o para rodar no seu próprio processo como `SYSTEM`. Caso o serviço já exista, ele apenas o inicia.

### 2. Serviço Core do Windows (`src/service/service.c`)
*   **Função:** O núcleo do sistema, rodando em background sem interface gráfica.
*   **Funcionamento:** Implementa o ciclo de vida padrão de um serviço do Windows (`ServiceMain`, `ServiceCtrlHandler`). Inicia uma thread principal (`ServiceWorkerThread`) que atua como um servidor TCP ouvindo na porta `4444`. Para cada cliente que se conecta, gera uma nova thread de processamento (`HandleClientThread`), permitindo múltiplas conexões simultâneas.
*   **Comandos WebSocket suportados:**
    *   `CAPTURAR_TELA` — Captura a tela inteira, comprime para JPEG e envia os bytes via frame binário.
    *   `MOUSE_CLICK x y` — Simula um clique esquerdo na posição (x, y) da tela.
    *   `MOUSE_RCLICK x y` — Simula um clique direito na posição (x, y) da tela.
    *   `MOUSE_MOVE x y` — Move o cursor do mouse para (x, y).
    *   `KEY_PRESS vkCode` — Simula o pressionamento de uma tecla pelo Virtual Key Code do Windows.
    *   Qualquer outra mensagem — Responde com echo: `[SYSTEM Echo] <mensagem>`.

### 3. Módulo WebSocket (`src/service/websocket.h`)
*   **Função:** Lida com a abstração do protocolo WebSocket (RFC 6455) diretamente em C, sem depender de bibliotecas externas de rede.
*   **Funcionamento:**
    *   **Handshake:** Processa a requisição HTTP de `Upgrade`, extrai o `Sec-WebSocket-Key`, concatena com o GUID Mágico do WebSocket e gera um hash SHA-1 e um encoding Base64 para responder com o `Sec-WebSocket-Accept`.
    *   **Framing:** Suporta frames de texto (opcode 0x1), binários (opcode 0x2), Ping/Pong e Close. Decodifica a máscara XOR dos frames do cliente e envia frames sem máscara (como exigido para servidores).

### 4. Módulo de Captura de Tela (`src/service/tela.h`)
*   **Função:** Captura a tela inteira do Windows e retorna os bytes comprimidos em JPEG na memória.
*   **Funcionamento:**
    *   Usa as APIs GDI do Windows (`GetDC`, `CreateCompatibleDC`, `BitBlt`, `CreateDIBSection`) para copiar os pixels da tela para um buffer.
    *   Converte os pixels de BGRA (formato nativo do Windows) para RGB.
    *   Comprime para JPEG (qualidade 70) usando a biblioteca `stb_image_write`, reduzindo o tamanho de **~8 MB (BMP cru) para ~100-300 KB**.
    *   Retorna uma struct `ScreenCapture` com o ponteiro para os dados e o tamanho total.

### 5. Módulo de Injeção de Input (`src/service/input.h`)
*   **Função:** Injeta eventos de mouse e teclado na máquina alvo para controle remoto.
*   **Funcionamento:**
    *   Usa a API `SendInput()` do Win32 para simular ações do usuário.
    *   Suporta: movimento do mouse (coordenadas absolutas normalizadas 0-65535), clique esquerdo, clique direito, pressionamento de tecla por Virtual Key Code, e digitação de caracteres Unicode.

### 6. Agente de Eventos (`src/service/agent.c`)
*   **Função:** Captura eventos de teclado do usuário local usando Hooks do Windows.
*   **Funcionamento:** Usa `SetWindowsHookEx(WH_KEYBOARD_LL)` para interceptar teclas pressionadas em todo o sistema e envia os eventos para o serviço via socket local.

### 7. Cliente Web (`test_websocket.html`)
*   **Função:** Interface gráfica completa para acesso remoto via navegador.
*   **Funcionalidades:**
    *   Conexão/Desconexão WebSocket na porta 4444.
    *   **📸 Captura única** — Solicita um screenshot e exibe na tela.
    *   **▶️ Stream em tempo real** — Modo vídeo contínuo (captura frames em loop).
    *   **🖱️ Controle remoto** — Clique esquerdo/direito na imagem envia comandos de mouse com coordenadas convertidas proporcionalmente para a resolução real da tela remota.
    *   **⌨️ Teclado remoto** — Teclas pressionadas são capturadas e enviadas como `KEY_PRESS`.
    *   **Info bar** — Exibe coordenadas do mouse, FPS do stream e estado do controle remoto.

---

## 📚 Listagem de Bibliotecas (Libs)

### Bibliotecas Nativas (Zero dependências externas de rede)
*   **`<winsock2.h>`**: WinSock 2 para comunicação TCP/IP e Sockets. Requer linkagem com `ws2_32.lib`.
*   **`<windows.h>`**: API principal do Windows. Serviços do SCM, Threads, Eventos, GDI (captura de tela), `SendInput` (injeção de input).
*   **`<tchar.h>`**: Mapeamento de caracteres genéricos (Unicode/ANSI) no `launcher.c`.
*   **`<stdio.h>`**, **`<stdlib.h>`**, **`<string.h>`**: Standard C Library para I/O, alocação de memória e manipulação de strings.

### Bibliotecas de Terceiros (Single-Header)
*   **`stb_image_write.h`** ([nothings/stb](https://github.com/nothings/stb)): Biblioteca de domínio público (single-header) para escrita de imagens. Usada para compressão JPEG em memória via `stbi_write_jpg_to_func`.

---

## 🚀 Como Startar o Projeto

### Pré-requisitos
*   Compilador C (GCC, MSVC ou MinGW).
*   CMake (versão 3.10 ou superior).
*   Sistema Operacional Windows (usa APIs nativas de Serviço, GDI e SendInput).

### Passos de Compilação
1.  Abra o terminal na raiz do projeto.
2.  Crie uma pasta para o build: `mkdir build`
3.  Acesse a pasta: `cd build`
4.  Gere os arquivos de build: `cmake ..`
5.  Compile o projeto: `cmake --build .`
    *   *Nota: O output gerado resultará em dois executáveis: `launcher.exe` e `service.exe` na pasta de build ou dentro da pasta `src/launcher/Debug` e `src/service/Debug` dependendo do seu gerador.*

### Passos de Execução
1.  **Copie os executáveis:** Garanta que `launcher.exe` e `service.exe` estejam **na mesma pasta**.
2.  **Inicie o Launcher (Como Administrador):** O Windows requer privilégios de administrador para instalar um serviço. Abra o terminal como Administrador, navegue até a pasta dos executáveis e rode:
    ```bash
    ./launcher.exe
    ```
    Você verá logs informando que o serviço foi registrado e iniciado.
3.  **Teste a conexão:**
    *   Abra o arquivo `test_websocket.html` que está na raiz do projeto em qualquer navegador moderno.
    *   Clique no botão **"Conectar"**. O status mudará para verde.
    *   Clique em **"📸 Capturar Tela"** para receber um screenshot da máquina remota.
    *   Clique em **"▶️ Iniciar Stream"** para ver a tela em tempo real.
    *   Clique em **"🖱️ Ativar Controle"** para poder clicar e digitar na máquina remota.

### Desinstalando o Serviço
Caso queira parar e remover o serviço durante os testes, abra o terminal como Administrador e digite:
```cmd
sc stop MeuServicoSystem
sc delete MeuServicoSystem
```

---

## 🧠 Como Entender o Código

Para dominar este código, sugere-se a seguinte ordem de leitura:

1.  **Entenda o Deploy (`src/launcher/launcher.c`)**: Comece por aqui. Veja como a função `CreateService` e `StartService` é chamada. Isso te fará entender como o Windows trata o executável `service.exe`.
2.  **Entenda o Ciclo de Vida do Serviço (`src/service/service.c` - linhas iniciais)**: Leia a `main()`, `ServiceMain()` e `ServiceCtrlHandler()`. Observe como o serviço reporta seus estados (`SERVICE_START_PENDING`, `SERVICE_RUNNING`, `SERVICE_STOPPED`) para o Windows não acusar falha de tempo limite. Observe a criação do evento `g_ServiceStopEvent`.
3.  **Entenda o Listener TCP (`src/service/service.c` - `ServiceWorkerThread`)**: Thread padrão de escuta de Socket TCP. Entenda como o bind e o listen são feitos na porta 4444, e como a função `select` é usada de forma não bloqueante. Veja que cada `accept` gera um `CreateThread`.
4.  **Entenda o Protocolo WebSocket (`src/service/websocket.h`)**: O coração da rede de baixo nível.
    *   Estude as funções `sha1` e `base64_encode`.
    *   Veja a `ws_do_handshake`, onde o `Sec-WebSocket-Key` é extraído e a resposta 101 Switching Protocols é gerada.
    *   Leia `ws_recv_frame` e `ws_send_frame`. O WebSocket transmite dados em "frames" com máscaras XOR para segurança.
5.  **Entenda a Captura de Tela (`src/service/tela.h`)**: Veja como GDI captura os pixels, como BGRA é convertido para RGB, e como `stb_image_write` comprime para JPEG na memória via callback.
6.  **Entenda o Controle Remoto (`src/service/input.h`)**: Estude como `SendInput` injeta eventos de mouse e teclado, incluindo a normalização de coordenadas (0 a 65535).
7.  **Entenda a Sessão do Cliente (`src/service/service.c` - `HandleClientThread`)**: Veja como as peças se juntam: handshake, parsing de comandos (`CAPTURAR_TELA`, `MOUSE_CLICK`, `KEY_PRESS`), envio de frames binários (imagem) e frames de texto (echo).

---

## 📊 Progresso do Projeto

| Feature | Status |
|---|---|
| Comunicação WebSocket (RFC 6455) | ✅ Completo |
| Serviço do Windows (SCM) | ✅ Completo |
| Captura de Tela (GDI) | ✅ Completo |
| Compressão JPEG (stb_image_write) | ✅ Completo |
| Transmissão de Imagem (Frame Binário) | ✅ Completo |
| Stream em Tempo Real (Modo Vídeo) | ✅ Completo |
| Controle Remoto (Mouse + Teclado) | ✅ Completo |
| Cliente Web (HTML/JS) | ✅ Completo |
| Criptografia (TLS/SSL) | ⬜ Pendente |
| NAT Traversal (Acesso fora da LAN) | ⬜ Pendente |
