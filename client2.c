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
    for (int i = 0; i < strlen(metin); i++) {
        unsigned char c = metin[i];
        for (int bit = 7; bit >= 0; bit--) {
            ones += ((c >> bit) & 1);
        }
    }
    return ones % 2; // even parity
}

//2D PARITY 
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

//CRC-8
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

//IP CHECKSUM 
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

//HAMMING DECODE (Sadece kontrol için basit XOR testi) 
void hammingEncode(unsigned char data, unsigned char *hamming);

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

        // Paket ayrıştır
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

        
        //DOĞRULAMA KISMI
        if (strcmp(method, "PARITY") == 0) {
            int p = parityHesapla(data);
            printf("Hesaplanan Parite = %d\n", p);
            printf((p == atoi(control)) ? "HATA YOK!\n" : "HATA VAR!\n");
        }

        else if (strcmp(method, "2DPARITY") == 0) {
            char row[256], col[9];
            hesapla2DParity(data, row, col);
            printf("Row Hesaplanan: %s\n", row);
            printf("Col Hesaplanan: %s\n", col);

            char *recvRow = control;
            char *recvCol = strtok(NULL, "|");

            if (!recvCol) {
                printf("PACKET ERROR: 2DPARITY formatı yanlış!\n");
                continue;
            }

            int ok = (!strcmp(row, recvRow) && !strcmp(col, recvCol));
            printf(ok ? "HATA YOK!\n" : "HATA VAR!\n");
        }

        else if (strcmp(method, "CRC8") == 0) {
            unsigned char crc = crc8((unsigned char*)data, strlen(data));
            printf("Hesaplanan CRC8 = %02X\n", crc);
            printf((crc == strtol(control, NULL, 16)) ? "HATA YOK!\n" : "HATA VAR!\n");
        }

        else if (strcmp(method, "IPCHECKSUM") == 0) {
            uint16_t chk = internetChecksum((unsigned char*)data, strlen(data));
            printf("Hesaplanan IP Checksum = %04X\n", chk);
            printf((chk == strtol(control, NULL, 16)) ? "HATA YOK!\n" : "HATA VAR!\n");
        }

        else if (strcmp(method, "HAMMING") == 0) {
    printf("Hamming decode islemi baslatiliyor...\n");

    if (strlen(data) != 7) {
        printf("HATA: Hamming(7,4) için 7 bitlik veri gereklidir!\n");
    } else {
        int b1 = data[0] - '0';
        int b2 = data[1] - '0';
        int b3 = data[2] - '0';
        int b4 = data[3] - '0';
        int b5 = data[4] - '0';
        int b6 = data[5] - '0';
        int b7 = data[6] - '0';

        // Hamming parity check hesapları
        int p1 = b1 ^ b3 ^ b5 ^ b7;
        int p2 = b2 ^ b3 ^ b6 ^ b7;
        int p3 = b4 ^ b5 ^ b6 ^ b7;

        int hataBit = (p3 << 2) | (p2 << 1) | p1;

        if (hataBit == 0) {
            printf("HATA YOK! Veri dogru.\n");
        } else {
            printf("HATA TESPIT EDILDI! Hatalı bit: %d\n", hataBit);

            int idx = hataBit - 1;

            // Bit'i düzelt
            char duzeltilmis[8];
            strcpy(duzeltilmis, data);
            duzeltilmis[idx] = (duzeltilmis[idx] == '0') ? '1' : '0';

            printf("Düzeltilmiş veri: %s\n", duzeltilmis);
        }
    }

        }

        else {
            printf("Bilinmeyen yöntem!\n");
        }
    }

    close(sock);
    return 0;
}