#ifndef TELA_H
#define TELA_H

#include <windows.h>
#include <stdio.h>

// ============================================================================
// Função que captura a tela inteira e salva como screenshot.bmp
// Retorna 1 em caso de sucesso, 0 em caso de falha.
// Baseada na lógica do tela.c, encapsulada para uso pelo service.
// ============================================================================
static int CapturarTela(void) {
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

    // Aloca memória global para armazenar os pixels
    HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
    char* lpbitmap = (char*)GlobalLock(hDIB);

    // Extrai os pixels do bitmap para o buffer de memória
    GetDIBits(hMemoryDC, hBitmap, 0, altura, lpbitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // Salva o arquivo BMP no disco
    HANDLE hFile = CreateFile(TEXT("screenshot.bmp"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    int sucesso = 0;

    if (hFile != INVALID_HANDLE_VALUE) {
        BITMAPFILEHEADER bfh;
        bfh.bfType = 0x4D42;
        bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
        bfh.bfReserved1 = 0;
        bfh.bfReserved2 = 0;
        bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

        DWORD dwBytesWritten = 0;

        WriteFile(hFile, &bfh, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
        WriteFile(hFile, &bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
        WriteFile(hFile, lpbitmap, dwBmpSize, &dwBytesWritten, NULL);

        CloseHandle(hFile);
        sucesso = 1;
    }

    // Limpeza (sempre executa, mesmo se o arquivo falhar)
    GlobalUnlock(hDIB);
    GlobalFree(hDIB);
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteDC(hMemoryDC);
    DeleteObject(hBitmap);
    ReleaseDC(hDesktopWnd, hDesktopDC);

    return sucesso;
}

#endif // TELA_H
