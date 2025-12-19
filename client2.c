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

//HAMMING ENCODE (Client1 ile aynı)
void hammingEncode(unsigned char data, unsigned char *hamming) {
    unsigned char d[8];
    for (int i = 0; i < 8; i++)
        d[i] = (data >> (7 - i)) & 1;  // D1..D8

    unsigned char h[12];
    // Veri bitlerini yerleştir
    h[2]  = d[0];  // D1
    h[4]  = d[1];  // D2
    h[5]  = d[2];  // D3
    h[6]  = d[3];  // D4
    h[8]  = d[4];  // D5
    h[9]  = d[5];  // D6
    h[10] = d[6];  // D7
    h[11] = d[7];  // D8

    // Parite bitlerini hesapla (even parity)
    h[0] = h[2] ^ h[4] ^ h[6] ^ h[8] ^ h[10];        // P1
    h[1] = h[2] ^ h[5] ^ h[6] ^ h[9] ^ h[10];        // P2
    h[3] = h[4] ^ h[5] ^ h[6] ^ h[11];               // P4
    h[7] = h[8] ^ h[9] ^ h[10] ^ h[11];              // P8

    // hamming dizisini 0/1 stringine çevir
    for (int i = 0; i < 12; i++)
        hamming[i] = h[i] + '0';
    hamming[12] = '\0';
}

int main() {
    printf("Client2 çalışıyor! (alıcı)\n");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { 
        printf("Socket açılamadı!\n"); 
        return 1; 
    }

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

        printf("Received Data: %s\n", data);
        printf("Method: %s\n", method);
        printf("Sent Check Bits: %s\n", control);

        //DOĞRULAMA KISMI
        if (strcmp(method, "PARITY") == 0) {
            int computed = parityHesapla(data);
            int received = atoi(control);
            
            printf("Computed Check Bits: %d\n", computed);
            
            if (computed == received) {
                printf("Status: DATA CORRECT\n");
            } else {
                printf("Status: DATA CORRUPTED\n");
            }
        }

        else if (strcmp(method, "2DPARITY") == 0) {
            char row[256], col[9];
            hesapla2DParity(data, row, col);

            char *recvRow = control;
            char *recvCol = strtok(NULL, "|");

            if (!recvCol) {
                printf("PACKET ERROR: 2DPARITY formatı yanlış!\n");
                continue;
            }

            printf("Computed Check Bits (Row): %s\n", row);
            printf("Computed Check Bits (Col): %s\n", col);
            
            int ok = (!strcmp(row, recvRow) && !strcmp(col, recvCol));
            
            if (ok) {
                printf("Status: DATA CORRECT\n");
            } else {
                printf("Status: DATA CORRUPTED\n");
            }
        }

        else if (strcmp(method, "CRC8") == 0) {
            unsigned char computed = crc8((unsigned char*)data, strlen(data));
            unsigned char received = (unsigned char)strtol(control, NULL, 16);
            
            printf("Computed Check Bits: %02X\n", computed);
            
            if (computed == received) {
                printf("Status: DATA CORRECT\n");
            } else {
                printf("Status: DATA CORRUPTED\n");
            }
        }

        else if (strcmp(method, "IPCHECKSUM") == 0) {
            uint16_t computed = internetChecksum((unsigned char*)data, strlen(data));
            uint16_t received = (uint16_t)strtol(control, NULL, 16);
            
            printf("Computed Check Bits: %04X\n", computed);
            
            if (computed == received) {
                printf("Status: DATA CORRECT\n");
            } else {
                printf("Status: DATA CORRUPTED\n");
            }
        }

        else if (strcmp(method, "HAMMING") == 0) {
            printf("Hamming dogrulama islemi baslatiliyor...\n");
            
            int data_len = strlen(data);
            int control_len = strlen(control);
            
            // Her karakter 12 bit Hamming kodu üretir
            int expected_len = data_len * 12;
            
            if (control_len != expected_len) {
                printf("HATA: Hamming kodu uzunluk uyumsuzlugu!\n");
                printf("Beklenen: %d bit, Gelen: %d bit\n", expected_len, control_len);
                continue;
            }
            
            // Her karakteri ayrı ayrı encode et ve karşılaştır
            char computed[8192] = "";
            
            for (int i = 0; i < data_len; i++) {
                unsigned char hamming[13];
                hammingEncode(data[i], (unsigned char*)hamming);
                strcat(computed, hamming);
            }
            
            printf("Computed Check Bits: %s\n", computed);
            
            // Karşılaştırma
            if (strcmp(computed, control) == 0) {
                printf("Status: DATA CORRECT\n");
            } else {
                printf("Status: DATA CORRUPTED\n");
                
                // Hangi karakterlerde hata var göster
                int errors = 0;
                for (int i = 0; i < data_len; i++) {
                    char comp_block[13], recv_block[13];
                    strncpy(comp_block, computed + (i*12), 12);
                    strncpy(recv_block, control + (i*12), 12);
                    comp_block[12] = '\0';
                    recv_block[12] = '\0';
                    
                    if (strcmp(comp_block, recv_block) != 0) {
                        printf("  -> Karakter %d ('%c') hatali\n", i+1, data[i]);
                        errors++;
                    }
                }
                printf("  -> Toplam %d karakter hatali bulundu.\n", errors);
            }
        }

        else {
            printf("Bilinmeyen yöntem!\n");
        }
    }

    close(sock);
    return 0;
}
