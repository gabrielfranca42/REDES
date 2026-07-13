#ifndef TELA_H
#define TELA_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Estrutura que armazena o resultado da captura de tela na memória.
// O chamador é responsável por liberar o ponteiro 'dados' com free().
// ============================================================================
typedef struct {
    unsigned char *dados;   // Buffer contendo o BMP completo (FileHeader + InfoHeader + Pixels)
    int tamanho;            // Tamanho total do buffer em bytes
} ScreenCapture;

// ============================================================================
// Função que captura a tela inteira e retorna os bytes do BMP na memória.
// Retorna uma struct ScreenCapture. Se falhar, dados será NULL e tamanho será 0.
// O chamador DEVE chamar free(resultado.dados) após o uso!
// ============================================================================
static ScreenCapture CapturarTela(void) {
    ScreenCapture resultado = {NULL, 0};

    HWND hDesktopWnd = GetDesktopWindow();
    HDC hDesktopDC = GetDC(hDesktopWnd);
    HDC hMemoryDC = CreateCompatibleDC(hDesktopDC);

    int largura = GetSystemMetrics(SM_CXSCREEN);
    int altura = GetSystemMetrics(SM_CYSCREEN);

    HBITMAP hBitmap = CreateCompatibleBitmap(hDesktopDC, largura, altura);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    // Copia os pixels da tela real para a memória
    BitBlt(hMemoryDC, 0, 0, largura, altura, hDesktopDC, 0, 0, SRCCOPY);

    // Prepara o cabeçalho de informações da imagem
    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = largura;
    bi.biHeight = altura;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    // Calcula o tamanho total dos pixels em bytes
    DWORD dwBmpSize = ((largura * bi.biBitCount + 31) / 32) * 4 * altura;

    // Aloca memória para os pixels
    HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
    char *lpbitmap = (char *)GlobalLock(hDIB);

    // Extrai os pixels do bitmap para o buffer
    GetDIBits(hMemoryDC, hBitmap, 0, altura, lpbitmap, (BITMAPINFO *)&bi, DIB_RGB_COLORS);

    // Monta o BMP completo na memória (FileHeader + InfoHeader + Pixels)
    BITMAPFILEHEADER bfh;
    bfh.bfType = 0x4D42;  // "BM"
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
    bfh.bfReserved1 = 0;
    bfh.bfReserved2 = 0;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    int tamanhoTotal = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
    unsigned char *buffer = (unsigned char *)malloc(tamanhoTotal);

    if (buffer) {
        // Copia os 3 blocos sequencialmente no buffer
        int offset = 0;

        memcpy(buffer + offset, &bfh, sizeof(BITMAPFILEHEADER));
        offset += sizeof(BITMAPFILEHEADER);

        memcpy(buffer + offset, &bi, sizeof(BITMAPINFOHEADER));
        offset += sizeof(BITMAPINFOHEADER);

        memcpy(buffer + offset, lpbitmap, dwBmpSize);

        resultado.dados = buffer;
        resultado.tamanho = tamanhoTotal;
    }

    // Limpeza
    GlobalUnlock(hDIB);
    GlobalFree(hDIB);
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteDC(hMemoryDC);
    DeleteObject(hBitmap);
    ReleaseDC(hDesktopWnd, hDesktopDC);

    return resultado;
}

#endif // TELA_H
