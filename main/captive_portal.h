#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

/**
 * @brief Startet den DNS-Server-Task für das Captive Portal.
 * Beantwortet alle DNS-Anfragen auf UDP-Port 53 mit der IP 192.168.4.1.
 */
void dns_server_task(void *pvParameters);

#endif // CAPTIVE_PORTAL_H