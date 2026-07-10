# C_REDES - Windows Service WebSocket System

Este projeto é um sistema em linguagem C que implementa um Serviço do Windows (Windows Service) com um servidor WebSocket nativo embutido. Ele é projetado para rodar em background no sistema operacional com privilégios elevados (SYSTEM), permitindo uma comunicação bidirecional em tempo real com clientes locais ou remotos através do protocolo WebSocket.

## 🏗️ System Design e Arquitetura

A arquitetura do projeto é dividida em três componentes principais:

1.  **Launcher / Instalador (`launcher.c`)**
    *   **Função:** É o responsável por instalar e iniciar o serviço no sistema operacional.
    *   **Funcionamento:** Ele descobre o caminho absoluto do executável `service.exe`, comunica-se com o Service Control Manager (SCM) do Windows para registrar o serviço (nome interno `MeuServicoSystem`, nome de exibição `Meu Servico de Otimizacao Core`) configurando-o para rodar no seu próprio processo como `SYSTEM`. Caso o serviço já exista, ele apenas o inicia.

2.  **Serviço Core do Windows (`service.c`)**
    *   **Função:** O núcleo do sistema, rodando em background sem interface gráfica.
    *   **Funcionamento:** Implementa o ciclo de vida padrão de um serviço do Windows (`ServiceMain`, `ServiceCtrlHandler`). Ele inicia uma thread principal (`ServiceWorkerThread`) que atua como um servidor TCP ouvindo na porta `4444`. Para cada cliente que se conecta, ele gera uma nova thread de processamento (`HandleClientThread`), permitindo múltiplas conexões simultâneas.

3.  **Módulo WebSocket (`websocket.h`)**
    *   **Função:** Lida com a abstração do protocolo WebSocket (RFC 6455) diretamente em C, sem depender de bibliotecas externas complexas de rede.
    *   **Funcionamento:** 
        *   **Handshake:** Processa a requisição HTTP de `Upgrade`, extrai o `Sec-WebSocket-Key`, concatena com o GUID Mágico do WebSocket e gera um hash SHA-1 e um encoding Base64 para responder com o `Sec-WebSocket-Accept`.
        *   **Framing:** Possui funções para desembrulhar (unmask) frames binários que chegam do cliente e embrulhar frames de resposta (Text, Ping, Pong, Close).
        *   **Echo Server:** Atualmente configurado como um Echo Server. Ele recebe a mensagem em texto, e responde ao cliente prefixando `[SYSTEM Echo]`.

4.  **Cliente Web (`test_websocket.html`)**
    *   **Função:** Interface gráfica de testes para conectar e interagir com o serviço.
    *   **Funcionamento:** Um front-end simples com HTML, CSS e JavaScript puros. Fornece uma interface de "terminal", onde exibe logs de conexão, estado do websocket (Conectado/Desconectado), e permite enviar mensagens via socket e ver o retorno do serviço do Windows.

---

## 📚 Listagem de Bibliotecas (Libs)

Este projeto foi construído focando em **baixo nível e zero dependências de terceiros**, utilizando apenas bibliotecas nativas do Windows e do padrão C:

*   **`<winsock2.h>`**: Biblioteca nativa do Windows (WinSock 2) para comunicação de rede (TCP/IP e Sockets). Ela requer ser linkada na compilação com `ws2_32.lib`.
*   **`<windows.h>`**: Biblioteca principal da API do Windows. Usada para criação de Serviços do SCM, manipulação de Threads, Eventos (`CreateEvent`, `WaitForSingleObject`), e tipos de dados do Windows.
*   **`<tchar.h>`**: Utilizada no `launcher.c` para mapeamento de caracteres genéricos (Unicode/ANSI) visando compatibilidade com as APIs de strings do Windows.
*   **`<stdio.h>`**, **`<stdlib.h>`**, **`<string.h>`**: Bibliotecas padrão (Standard C Library) para operações de entrada/saída, manipulação de strings e alocação dinâmica de memória, principalmente nas funções SHA-1, Base64 e montagem de cabeçalhos HTTP.

---

## 🚀 Como Startar o Projeto

### Pré-requisitos
*   Compilador C (GCC, MSVC ou MinGW).
*   CMake (versão 3.10 ou superior).
*   Sistema Operacional Windows (pois usa as APIs nativas de Serviço).

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
    *   Digite uma mensagem e clique em **"Enviar"**. Você verá a resposta do serviço SYSTEM (ex: `[SYSTEM Echo] Sua mensagem`).

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
3.  **Entenda o Listener TCP (`src/service/service.c` - `ServiceWorkerThread`)**: Esta é uma thread padrão de escuta de Socket TCP. Entenda como o bind e o listen são feitos na porta 4444, e como a função `select` é usada de forma não bloqueante para checar novas conexões sem travar o serviço. Veja que cada `accept` gera um `CreateThread`.
4.  **Entenda o Protocolo WebSocket (`src/service/websocket.h`)**: Aqui é o coração da rede de baixo nível.
    *   Estude as funções `sha1` e `base64_encode`.
    *   Veja a `ws_do_handshake`, onde a string "Sec-WebSocket-Key" é extraída, e a resposta 101 Switching Protocols é gerada.
    *   Leia as funções `ws_recv_frame` e `ws_send_frame`. O WebSocket transmite dados em "frames" com máscaras bit a bit (XOR mask) para segurança. Entender o parse de bytes destas duas funções é fundamental.
5.  **Entenda a Sessão do Cliente (`src/service/service.c` - `HandleClientThread`)**: Finalmente, veja como as peças se juntam. O cliente conecta, o HTTP é lido, o handshake é efetuado, uma mensagem de boas-vindas é enviada e um loop com `select` aguarda os pacotes do client, processando PING/PONG e textos (Onde ocorre o Echo).
