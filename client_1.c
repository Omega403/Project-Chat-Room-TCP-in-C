#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <process.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define PORT "8080"
#define BUFFER_SIZE 1024

void receiveMessages(void* socket_ptr);
void enableAnsiEscapeSequences();

int main() {
    WSADATA wsa;
    SOCKET client_socket = INVALID_SOCKET;
    struct addrinfo* result = NULL, * ptr = NULL, hints;
    char buffer[BUFFER_SIZE] = {0};
    char username[50];

    // Abilita le sequenze di escape ANSI su Windows
    enableAnsiEscapeSequences();

    // Inizializza la libreria Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Inizializzazione della libreria Winsock fallita. Codice errore : %d\n", WSAGetLastError());
        return 1;
    }

    // Inizializza la struttura hints
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;       // Usa IPv4
    hints.ai_socktype = SOCK_STREAM; // Usa TCP
    hints.ai_protocol = IPPROTO_TCP;

    // Ottieni informazioni sull'indirizzo del server
    if (getaddrinfo(SERVER_IP, PORT, &hints, &result) != 0) {
        printf("Errore nella getaddrinfo\n");
        WSACleanup();
        return 1;
    }

    // Crea il socket
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        client_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (client_socket == INVALID_SOCKET) {
            printf("Impossibile creare il socket. Codice errore : %d\n", WSAGetLastError());
            freeaddrinfo(result);
            WSACleanup();
            return 1;
        }

        // Connessione al server
        if (connect(client_socket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
            closesocket(client_socket);
            client_socket = INVALID_SOCKET;
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    if (client_socket == INVALID_SOCKET) {
        printf("Impossibile connettersi al server\n");
        WSACleanup();
        return 1;
    }

    // Ottieni l'username da parte dell'utente
    printf("Benvenuto! Inserisci il tuo username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = '\0'; // Rimuovi il carattere di nuova riga

    // Invia l'username al server
    if (send(client_socket, username, strlen(username), 0) == SOCKET_ERROR) {
        printf("Invio dell'username fallito. Codice errore : %d\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    // Crea un thread per ricevere i messaggi dal server
    _beginthread(receiveMessages, 0, (void*)&client_socket);

    // Invia messaggi al server
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);
        if (send(client_socket, buffer, strlen(buffer), 0) == SOCKET_ERROR) {
            printf("Invio del messaggio fallito. Codice errore : %d\n", WSAGetLastError());
            break;
        }
    }

    // Chiudi il socket del client
    closesocket(client_socket);
    WSACleanup();

    return 0;
}

void receiveMessages(void* socket_ptr) {
    SOCKET client_socket = *(SOCKET*)socket_ptr;
    char buffer[BUFFER_SIZE];

    // Ricevi messaggi dal server
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received > 0) {
            printf("%s\n", buffer);
        } else if (bytes_received == 0) {
            printf("Connessione chiusa dal server\n");
            break;
        } else {
            printf("Ricezione del messaggio fallita. Codice errore : %d\n", WSAGetLastError());
            break;
        }
    }
}

// Funzione per abilitare le sequenze di escape ANSI su Windows
void enableAnsiEscapeSequences() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
