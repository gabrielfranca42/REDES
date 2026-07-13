#ifndef TELA_H
#define TELA_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// stb_image_write: Biblioteca single-header para compressão JPEG
// O #define IMPLEMENTATION deve aparecer em EXATAMENTE UM arquivo .c ou .h
// ============================================================================
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ============================================================================
// Estrutura que armazena o resultado da captura de tela na memória.
// O chamador é responsável por liberar o ponteiro 'dados' com free().
// ============================================================================
typedef struct {
    unsigned char *dados;   // Buffer contendo a imagem comprimida (JPEG)
    int tamanho;            // Tamanho total do buffer em bytes
} ScreenCapture;

// ============================================================================
// Callback usado pelo stb_image_write para escrever JPEG na memória
// Em vez de gravar num arquivo, acumula os bytes num buffer dinâmico
// ============================================================================
typedef struct {
    unsigned char *buffer;
    int tamanho;
    int capacidade;
} JpegBuffer;

static void jpeg_write_callback(void *context, void *data, int size) {
    JpegBuffer *buf = (JpegBuffer *)context;

    // Se o buffer não tem espaço suficiente, dobra a capacidade
    while (buf->tamanho + size > buf->capacidade) {
        buf->capacidade *= 2;
        buf->buffer = (unsigned char *)realloc(buf->buffer, buf->capacidade);
    }

    memcpy(buf->buffer + buf->tamanho, data, size);
    buf->tamanho += size;
}

// ============================================================================
// Função que captura a tela inteira e retorna os bytes de um JPEG na memória.
// Qualidade JPEG: 70 (bom equilíbrio entre tamanho e nitidez)
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

    // Prepara o cabeçalho (altura NEGATIVA = top-down, pixels na ordem correta)
    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = largura;
    bi.biHeight = -altura;  // NEGATIVO: top-down (linha 0 = topo da tela)
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    // CreateDIBSection nos dá acesso direto aos pixels (pPixels)
    unsigned char *pPixels = NULL;
    HBITMAP hBitmap = CreateDIBSection(hDesktopDC, (BITMAPINFO *)&bi, DIB_RGB_COLORS, (void **)&pPixels, NULL, 0);

    if (!hBitmap || !pPixels) {
        DeleteDC(hMemoryDC);
        ReleaseDC(hDesktopWnd, hDesktopDC);
        return resultado;
    }

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    // Copia os pixels da tela real para o nosso bitmap
    BitBlt(hMemoryDC, 0, 0, largura, altura, hDesktopDC, 0, 0, SRCCOPY);

    // Os pixels estão em formato BGRA (32 bits). O stb_image_write espera RGB (24 bits).
    // Precisamos converter BGRA → RGB e trocar os canais B e R.
    int totalPixels = largura * altura;
    unsigned char *rgb = (unsigned char *)malloc(totalPixels * 3);

    if (rgb) {
        for (int i = 0; i < totalPixels; i++) {
            // pPixels está em BGRA (Blue, Green, Red, Alpha)
            rgb[i * 3 + 0] = pPixels[i * 4 + 2]; // R (era o 3º byte: index 2)
            rgb[i * 3 + 1] = pPixels[i * 4 + 1]; // G (fica no mesmo lugar)
            rgb[i * 3 + 2] = pPixels[i * 4 + 0]; // B (era o 1º byte: index 0)
        }

        // Comprime para JPEG na memória usando o callback
        JpegBuffer jpegBuf;
        jpegBuf.capacidade = 256 * 1024; // Começa com 256 KB
        jpegBuf.tamanho = 0;
        jpegBuf.buffer = (unsigned char *)malloc(jpegBuf.capacidade);

        if (jpegBuf.buffer) {
            // Qualidade 70: bom equilíbrio (tamanho ~100-300 KB vs ~8 MB do BMP)
            stbi_write_jpg_to_func(jpeg_write_callback, &jpegBuf, largura, altura, 3, rgb, 70);

            resultado.dados = jpegBuf.buffer;
            resultado.tamanho = jpegBuf.tamanho;
        }

        free(rgb);
    }

    // Limpeza dos handles GDI
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(hDesktopWnd, hDesktopDC);

    return resultado;
}

#endif // TELA_H
