 #include <stdio.h>
 #include <windows.h>
 #include <tchar.h>

 #define SERVICO_NOME       _T("MeuServicoSystem");
 #define SERVICO_EXIBICAO    _T("Meu Servico de Otimizacao Core")
 #define SERVICE_EXE_NOME    _T("service.exe")
 
 
BOOL ObterCaminhoService(TCHAR* buffer, DWORD tamanhoBuffer)
{
    TCHAR caminhoLauncher[MAX_PATH];
    
    if (GetModuleFileName(NULL, caminhoLauncher, MAX_PATH) == 0)
    {
        return FALSE;
    }

    TCHAR* ultimaBarra = _tcsrchr(caminhoLauncher, TEXT('\\'));
    if (ultimaBarra == NULL)
    {
        return FALSE;
    }

    *(ultimaBarra + 1) = TEXT('\0');
    
    
    _sntprinf(bufferCaminho, tamanhoBuffer, _T  ("%s%s"), caminhoLauncher, SERVICE_EXE_NOME);
    
    return TRUE;

}

int main () 
SC_HANDLE hSCManager = NUll;
SC_HANDLE hServico = NULL;
TCHAR szBinPath[MAX_PATH];

_tprinf(_T("[+]Iniciando o laucer/instalador ...\n"));

if (!ObterCaminhoService(szBinPath,MAX_PATH))
{
    printf("[-] Erro ao determinar o caminho do service.exe. Codigo: %lu\n", GetLastError());
    return 1;
}

hService = CreateService(
    hSCManager,
    SERVICO_NOME,
    SERVICO_EXIBICAO,
    SERVICE_ALL_ACCESS,
    SERVICE_WIN32_OWN_PROCESS,
    SERVICE_DEMAND_START,
    SERVICE_ERROR_NORMAL,
    szBinPath,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
);

if (hService == NULL){
    DWORD erro = GetLastError(0);

    if (erro == ERROR_SERVICE_EXISTS){
        _tprintf(_T("[*] O servico '&s' ja esta registraddo. Abrindo configuração...\n"), SERVICO_NOME)
        hService = OpenService(hSCManager, SERVICO_NOME,SERVICE_ALL_ACCESS);
    } else {
        printf("[-]Erro critico ao criar servico. Codigo: %lu\n", erro );
        CloseServiceHandle(hSCManager);
        return 1;
    }
} else{
   _tprintf(_T("[+] Servico '%s' registrado com sucesso como SYSTEM!\n"), SERVICO_NOME);
}

_tprintf(_T("[*] Solicitando ao Windows para iniciar o service.exe como SYSTEM...\n"));
if (!StartService(hService, 0, NULL)){
    DWORD erro = GetLastError();
    if (erro == ERROR_SERVICE_ALREADY_RUNNING){
        _tprintf(_T("[*] O servico ja estava em execucao.\n"));
    } else{
        printf("[-] Erro ao iniciar servico. Codigo: %lu\n", erro);
    }

} else{
    _tprintf(_T("[+] Servico iniciado com sucesso.\n"));
}

if (hServico){
    CloseServiceHandle(hServico);
}
if (hSCManager){
    CloseServiceHandle(hSCManager);
}

 _tprintf(_T("[+] Launcher finalizado.\n"));
return 0;
}


 