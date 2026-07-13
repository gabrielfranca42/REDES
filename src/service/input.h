#ifndef INPUT_H
#define INPUT_H

#include <windows.h>

// ============================================================================
// Funções para injetar eventos de mouse e teclado na máquina alvo.
// Usa a API SendInput() do Win32 para simular ações do usuário.
// ============================================================================

// Move o cursor do mouse para as coordenadas absolutas (x, y) da tela
static void InjetarMouseMove(int x, int y) {
    // SendInput espera coordenadas normalizadas de 0 a 65535
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dx = (x * 65535) / screenW;
    input.mi.dy = (y * 65535) / screenH;
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

    SendInput(1, &input, sizeof(INPUT));
}

// Simula um clique esquerdo do mouse nas coordenadas (x, y)
static void InjetarMouseClick(int x, int y) {
    // Primeiro move o cursor para a posição
    InjetarMouseMove(x, y);
    Sleep(10); // Pequeno delay para o sistema registrar o movimento

    // Depois simula o clique (press + release em sequência)
    INPUT inputs[2] = {0};

    // Mouse button DOWN
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    // Mouse button UP
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    SendInput(2, inputs, sizeof(INPUT));
}

// Simula um clique direito do mouse nas coordenadas (x, y)
static void InjetarMouseRightClick(int x, int y) {
    InjetarMouseMove(x, y);
    Sleep(10);

    INPUT inputs[2] = {0};

    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;

    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;

    SendInput(2, inputs, sizeof(INPUT));
}

// Simula o pressionamento de uma tecla pelo Virtual Key Code
static void InjetarTecla(WORD vkCode) {
    INPUT inputs[2] = {0};

    // Key DOWN
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vkCode;

    // Key UP
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vkCode;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
}

// Simula a digitação de um caractere Unicode (qualquer caractere, incluindo acentos)
static void InjetarCaractere(WCHAR caractere) {
    INPUT inputs[2] = {0};

    // Key DOWN (via Unicode scan code)
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = caractere;
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;

    // Key UP
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wScan = caractere;
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
}

#endif // INPUT_H
