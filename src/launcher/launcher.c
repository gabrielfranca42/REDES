#include <stdio.h>
#include <windows.h>
#include <tchar.h>

// Definicoes de constantes para o nosso servico
#define SERVICO_NOME       _T("MeuServicoSystem")                  // Nome interno que o Windows usa para identificar o servico
#define SERVICO_EXIBICAO    _T("Meu Servico de Otimizacao Core")   // Nome bonitinho que aparece no "services.msc" (Gerenciador de Servicos)
#define SERVICE_EXE_NOME    _T("service.exe")                      // Nome do arquivo executavel que queremos que rode como servico


// Funcao responsavel por descobrir em qual pasta o launcher esta rodando
// e criar o caminho completo para o service.exe
BOOL ObterCaminhoService(TCHAR* buffer, DWORD tamanhoBuffer)
{
    TCHAR caminhoLauncher[MAX_PATH];
    
    // Pega o caminho completo de onde este launcher.exe esta rodando (Ex: C:\Pasta\launcher.exe)
    if (GetModuleFileName(NULL, caminhoLauncher, MAX_PATH) == 0)
    {
        return FALSE;
    }

    // Procura a ultima barra invertida '\' no caminho para separar a pasta do nome do arquivo
    TCHAR* ultimaBarra = _tcsrchr(caminhoLauncher, TEXT('\\'));
    if (ultimaBarra == NULL)
    {
        return FALSE;
    }

    // Corta a string logo apos a ultima barra (substituindo o nome do arquivo por um terminador NULO)
    // Entao "C:\Pasta\launcher.exe" vira "C:\Pasta\"
    *(ultimaBarra + 1) = TEXT('\0');
    
    // Junta a pasta do launcher ("C:\Pasta\") com o nome do executavel do servico ("service.exe")
    // O resultado no 'buffer' sera algo como "C:\Pasta\service.exe"
    _sntprintf(buffer, tamanhoBuffer, _T("%s%s"), caminhoLauncher, SERVICE_EXE_NOME);
    
    return TRUE;
}

int main () {
    SC_HANDLE hSCManager = NULL; // Guarda a conexao com o Gerenciador de Servicos do Windows
    SC_HANDLE hService = NULL;   // Guarda a referencia para o nosso servico especifico
    TCHAR szBinPath[MAX_PATH];   // Vai armazenar o caminho completo do nosso service.exe

    _tprintf(_T("[+] Iniciando o launcher/instalador...\n"));

    // 1. Tenta descobrir onde o service.exe esta
    if (!ObterCaminhoService(szBinPath,MAX_PATH))
    {
        printf("[-] Erro ao determinar o caminho do service.exe. Codigo: %lu\n", GetLastError());
        return 1;
    }

    // 2. Abre comunicacao com o "Service Control Manager" (SCM), que gerencia os servicos do Windows
    // Usamos SC_MANAGER_ALL_ACCESS para ter permissao total de criar novos servicos
    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL) {
        printf("[-] Erro ao abrir o Service Control Manager. Codigo: %lu\n", GetLastError());
        return 1;
    }

    // 3. Tenta criar o servico novo no Windows
    hService = CreateService(
        hSCManager,                 // Referencia ao Gerenciador de Servicos
        SERVICO_NOME,               // Nome interno do servico
        SERVICO_EXIBICAO,           // Nome de exibicao (visivel ao usuario)
        SERVICE_ALL_ACCESS,         // Permissoes completas sob o servico
        SERVICE_WIN32_OWN_PROCESS,  // O servico vai rodar no seu proprio processo (independente)
        SERVICE_DEMAND_START,       // O servico so inicia quando alguem mandar (nao liga sozinho no boot)
        SERVICE_ERROR_NORMAL,       // Tratamento padrao de erros se falhar ao iniciar
        szBinPath,                  // Caminho completo pro executavel ("C:\...\service.exe")
        NULL, NULL, NULL, NULL, NULL
    );

    // 4. Verifica se a criacao falhou
    if (hService == NULL){
        DWORD erro = GetLastError();

        // Se o erro foi "O servico ja existe", significa que ja instalamos ele antes
        if (erro == ERROR_SERVICE_EXISTS){
            _tprintf(_T("[*] O servico '%s' ja esta registrado. Abrindo configuracao...\n"), SERVICO_NOME);
            
            // Entao apenas abrimos o servico existente para podermos inicia-lo depois
            hService = OpenService(hSCManager, SERVICO_NOME,SERVICE_ALL_ACCESS);
        } else {
            // Se foi qualquer outro erro, deu ruim. Exibe o erro e encerra.
            printf("[-]Erro critico ao criar servico. Codigo: %lu\n", erro );
            CloseServiceHandle(hSCManager);
            return 1;
        }
    } else{
       // Se entrou aqui, o servico foi recem-criado com sucesso!
       // Como nao passamos credenciais, ele foi instalado para rodar como SYSTEM
       _tprintf(_T("[+] Servico '%s' registrado com sucesso como SYSTEM!\n"), SERVICO_NOME);
    }

    _tprintf(_T("[*] Solicitando ao Windows para iniciar o service.exe como SYSTEM...\n"));
    
    // 5. Manda o Windows efetivamente iniciar o servico (executar o service.exe)
    if (!StartService(hService, 0, NULL)){
        DWORD erro = GetLastError();
        // Se der erro porque ele ja estava rodando, ignoramos o problema
        if (erro == ERROR_SERVICE_ALREADY_RUNNING){
            _tprintf(_T("[*] O servico ja estava em execucao.\n"));
        } else{
            // Caso contrario, mostramos o erro
            printf("[-] Erro ao iniciar servico. Codigo: %lu\n", erro);
        }

    } else{
        _tprintf(_T("[+] Servico iniciado com sucesso.\n"));
    }

    // 6. Limpeza e fechamento de conexoes com o Windows (Boas praticas de C)
    if (hService){
        CloseServiceHandle(hService);
    }
    if (hSCManager){
        CloseServiceHandle(hSCManager);
    }

    _tprintf(_T("[+] Launcher finalizado.\n"));
    return 0;
}