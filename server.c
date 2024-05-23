#include <stdio.h>
#include <winsock2.h>
#include <process.h>
#include <windows.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

typedef struct {
    char username[50];
    SOCKET socket;
    struct sockaddr_in address;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];
int num_clients = 0;
HANDLE mutex;

// Colori per i messaggi dei client
const char* colors[MAX_CLIENTS] = {
        "\x1b[31m", // Rosso
        "\x1b[32m", // Verde
        "\x1b[33m", // Giallo
        "\x1b[34m", // Blu
        "\x1b[35m", // Magenta
        "\x1b[36m", // Ciano
        "\x1b[91m", // Rosso chiaro
        "\x1b[92m", // Verde chiaro
        "\x1b[93m", // Giallo chiaro
        "\x1b[94m"  // Blu chiaro
};

void handleClient(void* client_info_ptr);
void handleAdminInput(void* arg);
void broadcastMessage(const char* message, SOCKET exclude_socket);

int main() {
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int addr_size = sizeof(struct sockaddr_in);

    //inizializza la libreria Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Inizializzazione Winsock fallita. Codice errore: %d\n", WSAGetLastError());
        return 1;
    }

    //crea il socket del server
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Creazione socket fallita. Codice errore: %d\n", WSAGetLastError());
        return 1;
    }

    //inizializza la struttura dell'indirizzo del server
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    //associa il socket all'indirizzo e alla porta specificati
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Associazione fallita. Codice errore: %d\n", WSAGetLastError());
        return 1;
    }

    //mette il server in ascolto di connessioni
    if (listen(server_socket, 10) == SOCKET_ERROR) {
        printf("In attesa di connessioni fallita. Codice errore: %d\n", WSAGetLastError());
        return 1;
    }

    printf("Server in ascolto sulla porta %d...\n", PORT);

    //crea il mutex
    mutex = CreateMutex(NULL, FALSE, NULL);
    if (mutex == NULL) {
        printf("Creazione mutex fallita. Codice errore: %d\n", GetLastError());
        return 1;
    }

    //crea un thread per gestire l'input dell'admin
    _beginthread(handleAdminInput, 0, NULL);

    //accetta connessioni dai client
    while (1) {
        //accetta la connessione da un client
        if ((client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size)) == INVALID_SOCKET) {
            printf("Accettazione connessione fallita. Codice errore: %d\n", WSAGetLastError());
            return 1;
        }

        //blocca l'accesso ai dati condivisi
        WaitForSingleObject(mutex, INFINITE);

        clients[num_clients].socket = client_socket;
        clients[num_clients].address = client_addr;

        //invia un messaggio di benvenuto al client appena connesso
        char welcome_message[] = "Benvenuto! Inserisci il tuo username:\n";
        send(client_socket, welcome_message, strlen(welcome_message), 0);

        //sblocca l'accesso ai dati condivisi
        ReleaseMutex(mutex);

        // crea un thread per gestire il client
        _beginthread(handleClient, 0, (void*)&clients[num_clients]);

        num_clients++;
    }

    //chiudi il socket del server
    closesocket(server_socket);
    WSACleanup();

    //chiudi il mutex
    CloseHandle(mutex);

    return 0;
}

void handleClient(void* client_info_ptr) {
    ClientInfo* client_info = (ClientInfo*)client_info_ptr;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + 50];

    // Ricevi l'username del client
    int bytes_received = recv(client_info->socket, client_info->username, sizeof(client_info->username), 0);
    if (bytes_received > 0) {
        client_info->username[bytes_received] = '\0';

        //ottieni l'indirizzo IP del client
        char* client_ip = inet_ntoa(client_info->address.sin_addr);

        // Ottieni l'ora e la data correnti
        time_t now = time(NULL);
        struct tm* local_time = localtime(&now);
        char time_str[100];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);

        // stampa le informazioni del client
        printf("Client connesso: %s (IP: %s) alle %s\n", client_info->username, client_ip, time_str);

        // notifica a tutti i client che un nuovo client si e' connesso
        snprintf(message, sizeof(message), "\x1b[93m%s si e' connesso\x1b[0m\n", client_info->username);
        broadcastMessage(message, INVALID_SOCKET);
    } else {
        closesocket(client_info->socket);
        return;
    }

    //ricevi messaggi dal client
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        bytes_received = recv(client_info->socket, buffer, sizeof(buffer), 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';

            // prepara il messaggio da inviare agli altri client
            snprintf(message, sizeof(message), "%s%s: %s\x1b[0m", colors[client_info - clients], client_info->username, buffer);

            // invia il messaggio a tutti gli altri client
            broadcastMessage(message, client_info->socket);
        } else if (bytes_received == 0) {
            printf("Client disconnesso: %s\n", client_info->username);
            break;
        } else {
            printf("Ricezione messaggio fallita. Codice errore: %d\n", WSAGetLastError());
            break;
        }
    }

    //chiudi il socket del client
    closesocket(client_info->socket);

    // notifica a tutti i client che un client si e' disconnesso
    snprintf(message, sizeof(message), "\x1b[93m%s si e' disconnesso\x1b[0m\n", client_info->username);
    broadcastMessage(message, INVALID_SOCKET);

    // rimuovi il client dalla lista dei client
    WaitForSingleObject(mutex, INFINITE);
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].socket == client_info->socket) {
            for (int j = i; j < num_clients - 1; j++) {
                clients[j] = clients[j + 1];
            }
            num_clients--;
            break;
        }
    }
    ReleaseMutex(mutex);
}

void handleAdminInput(void* arg) {
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + 50];

    while (1) {
        // Leggi l'input dell'admin
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // Rimuovi il carattere di nuova riga

        // prepara il messaggio da inviare ai client
        snprintf(message, sizeof(message), "\x1b[95madmin: %s\x1b[0m", buffer);

        //invia il messaggio a tutti i client
        broadcastMessage(message, INVALID_SOCKET);
    }
}

void broadcastMessage(const char* message, SOCKET exclude_socket) {
    WaitForSingleObject(mutex, INFINITE);
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].socket != INVALID_SOCKET && clients[i].socket != exclude_socket) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    ReleaseMutex(mutex);
}
