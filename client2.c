#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

// PARITY 
int parityHesapla(char *metin) {
    int ones = 0;
    int len = strlen(metin);
    for (int i = 0; i < len; i++) {
        unsigned char c = metin[i];
        for (int bit = 7; bit >= 0; bit--) {
            ones += ((c >> bit) & 1);
        }
    }
    return ones % 2; // even parity
}

// 2D PARITY 
void hesapla2DParity(char *metin, char *rowParity, char *colParity) {
    int len = strlen(metin);
    int colOnes[8] = {0};

    for (int i = 0; i < len; i++) {
        unsigned char c = metin[i];
        int rowOnes = 0;
        for (int bit = 7; bit >= 0; bit--) {
            int b = (c >> bit) & 1;
            rowOnes += b;
            colOnes[7 - bit] += b;
        }
        rowParity[i] = (rowOnes % 2) + '0';
    }
    rowParity[len] = '\0';

    for (int j = 0; j < 8; j++)
        colParity[j] = (colOnes[j] % 2) + '0';
    colParity[8] = '\0';
}

// CRC-8
unsigned char crc8(unsigned char *data, int len) {
    unsigned char crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
    }
    return crc;
}

// IP CHECKSUM 
uint16_t internetChecksum(unsigned char *data, int len) {
    uint32_t sum = 0;
    for (int i = 0; i < len; i += 2) {
        uint16_t word = data[i] << 8;
        if (i + 1 < len) word |= data[i + 1];
        sum += word;
        if (sum > 0xFFFF) sum = (sum & 0xFFFF) + 1;
    }
    return ~sum;
}

/* =========================
   HAMMING (12,8) DECODE
   ========================= */
void hammingDecode12_8(const char *hamming) {
    int h[12];
    for (int i = 0; i < 12; i++)
        h[i] = hamming[i] - '0';

    // Even parity check
    int p1 = h[0] ^ h[2] ^ h[4] ^ h[6] ^ h[8] ^ h[10];
    int p2 = h[1] ^ h[2] ^ h[5] ^ h[6] ^ h[9] ^ h[10];
    int p4 = h[3] ^ h[4] ^ h[5] ^ h[6] ^ h[11];
    int p8 = h[7] ^ h[8] ^ h[9] ^ h[10] ^ h[11];

    int errorPos = (p8 << 3) | (p4 << 2) | (p2 << 1) | p1;

    if (errorPos != 0) {
        printf("HATA TESPIT EDILDI! Bit pozisyonu: %d\n", errorPos);
        h[errorPos - 1] ^= 1; // düzelt
        printf("HATA DUZELTILDI.\n");
    } else {
        printf("HATA YOK!\n");
    }

    // Veri bitlerini çıkar
    unsigned char data =
        (h[2]  << 7) |
        (h[4]  << 6) |
        (h[5]  << 5) |
        (h[6]  << 4) |
        (h[8]  << 3) |
        (h[9]  << 2) |
        (h[10] << 1) |
        (h[11]);

    printf("Cozulen karakter: '%c' (0x%02X)\n", data, data);
}

int main() {
    printf("Client2 çalışıyor! (alıcı)\n");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { printf("Socket açılamadı!\n"); return 1; }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(8081);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        printf("Sunucuya bağlanılamadı!\n");
        return 1;
    }

    printf("Sunucuya bağlanıldı. Paketler bekleniyor...\n");

    char buf[8192];

    while (1) {
        int r = recv(sock, buf, sizeof(buf) - 1, 0);
        if (r <= 0) {
            printf("Sunucu bağlantıyı kapattı!\n");
            break;
        }

        buf[r] = '\0';
        printf("\n--- Paket Alındı ---\n%s\n", buf);

        char *data = strtok(buf, "|");
        char *method = strtok(NULL, "|");
        char *control = strtok(NULL, "|");

        if (!data || !method || !control) {
            printf("HATALI FORMAT!\n");
            continue;
        }

        printf("DATA: %s\n", data);
        printf("METHOD: %s\n", method);
        printf("CONTROL: %s\n", control);

        if (strcmp(method, "PARITY") == 0) {
            int p = parityHesapla(data);
            printf((p == atoi(control)) ? "HATA YOK!\n" : "HATA VAR!\n");
        }

        else if (strcmp(method, "2DPARITY") == 0) {
            char row[256], col[9];
            hesapla2DParity(data, row, col);
            char *recvRow = control;
            char *recvCol = strtok(NULL, "|");
            int ok = recvCol && !strcmp(row, recvRow) && !strcmp(col, recvCol);
            printf(ok ? "HATA YOK!\n" : "HATA VAR!\n");
        }

        else if (strcmp(method, "CRC8") == 0) {
            unsigned char crc = crc8((unsigned char*)data, strlen(data));
            printf((crc == strtol(control, NULL, 16)) ? "HATA YOK!\n" : "HATA VAR!\n");
        }

        else if (strcmp(method, "IPCHECKSUM") == 0) {
            uint16_t chk = internetChecksum((unsigned char*)data, strlen(data));
            printf((chk == strtol(control, NULL, 16)) ? "HATA YOK!\n" : "HATA VAR!\n");
        }

        else if (strcmp(method, "HAMMING") == 0) {
            printf("Hamming(12,8) decode basladi...\n");
            int len = strlen(control);

            if (len % 12 != 0) {
                printf("HATALI HAMMING VERISI!\n");
                continue;
            }

            for (int i = 0; i < len; i += 12) {
                char block[13];
                strncpy(block, &control[i], 12);
                block[12] = '\0';
                hammingDecode12_8(block);
            }
        }

        else {
            printf("Bilinmeyen yöntem!\n");
        }
    }

    close(sock);
    return 0;
}
