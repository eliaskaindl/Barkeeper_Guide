#include <lwip/sockets.h>
#include <lwip/inet.h>
#include "captive_portal.h"

#define DNS_PORT 53

void dns_server_task(void *pvParameters)
{
    char rx_buffer[512];
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DNS_PORT);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        vTaskDelete(NULL);
        return;
    }

    bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    while (1)
    {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len > 12)
        { // Ein gültiger DNS-Header hat mindestens 12 Bytes

            // Header-Flags für Standard-Antwort setzen
            rx_buffer[2] = 0x81; // QR=1 (Response), Opcode=0, AA=0, TC=0, RD=1
            rx_buffer[3] = 0x80; // RA=1, Z=0, RCODE=0 (No error)

            // QDCOUNT (Anzahl Fragen) bleibt unverändert (Bytes 4 und 5)
            // ANCOUNT (Anzahl Antworten) auf 1 setzen
            rx_buffer[6] = 0x00;
            rx_buffer[7] = 0x01;
            // NSCOUNT & ARCOUNT (Authority & Additional Records, z.B. iOS EDNS0) auf 0 setzen!
            rx_buffer[8] = 0x00;
            rx_buffer[9] = 0x00;
            rx_buffer[10] = 0x00;
            rx_buffer[11] = 0x00;

            // Ende der eigentlichen Frage (QNAME + QTYPE + QCLASS) suchen,
            // um eventuelle iOS-Zusatzdaten (EDNS0) abzuschneiden!
            int query_end = 12;
            while (query_end < len && rx_buffer[query_end] != 0)
            {
                query_end += rx_buffer[query_end] + 1;
            }
            query_end += 5; // +1 für das Null-Byte des Namens, +4 für QTYPE und QCLASS

            if (query_end <= len)
            {
                unsigned char response_tail[] = {
                    0xc0, 0x0c,             // Pointer auf den Domainnamen bei Offset 12
                    0x00, 0x01,             // Type A (IPv4)
                    0x00, 0x01,             // Class IN
                    0x00, 0x00, 0x00, 0x3c, // TTL (60 Sekunden)
                    0x00, 0x04,             // Datenlänge (4 Bytes)
                    192, 168, 4, 1          // ESP32 IP
                };

                if (query_end + sizeof(response_tail) < sizeof(rx_buffer))
                {
                    memcpy(rx_buffer + query_end, response_tail, sizeof(response_tail));
                    sendto(sock, rx_buffer, query_end + sizeof(response_tail), 0, (struct sockaddr *)&source_addr, socklen);
                }
            }
        }
    }
}