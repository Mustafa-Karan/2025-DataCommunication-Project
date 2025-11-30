#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

char colParity[9]; // 8 bit + null karakter

void liste(){
    printf("Metodlar\n");
    printf("1.Parite Biti\n");
    printf("2.2D Parite Hesapla\n");
    printf("3. CRC 8 bit\n");
    printf("4. Hamming Kodlama\n");
    printf("5. IP Checksum Hesapla\n");
    printf("-1. Çıkış\n");
}
int parityHesapla(char *metin){
    // fgets \n karakteri bırakıyor → temizlemek için kullandım
    //metin[strcspn(metin, "\n")] = '\0';bunu mainde yaptım zaten

    int len = strlen(metin);

    int ones = 0;  // 1 sayısını saymak için
    for (int i = 0; i < len; i++) {
        unsigned char c = metin[i];// char -128 ile 128 arasında assci için unsigned yaptım 0 ile 255
        
        int karakterBit=0;
        printf("%c -> ", (c == ' ' ? '_' : c));  // boşluk görünür olsun diye _ yazdırdım
                                                // ternary operatörü
        // 8 bitini yazdır + 1 say
        for (int bit = 7; bit >= 0; bit--) {
            int b = (c >> bit) & 1;
            printf("%d", b);
            ones += b;//tüm metindeki birleri sayıyorum
            karakterBit+=b; //buda hangi karakterdeyse o karakterdeki birleri sayıyor her döngüde sıfırlanıyor
        }
        
        int parity = ones % 2;  // EVEN parity
        printf(" | Parite = %d\n", parity);
    }

    return ones % 2;
}
void hesapla2DParity(char *metin, char *rowParity, char *colParity)
{
    metin[strcspn(metin, "\n")] = '\0';

    int len = strlen(metin);

    // Sütunlar için sayaç
    int colOnes[8] = {0};

    for (int i = 0; i < len; i++)
    {
        unsigned char c = metin[i];
        int rowOnes = 0;

        printf("%c → ", c);

        for (int bit = 7; bit >= 0; bit--)
        {
            int b = (c >> bit) & 1;
            printf("%d", b);

            rowOnes += b;
            colOnes[7 - bit] += b;  // sütun için
        }

        rowParity[i] = (rowOnes % 2) + '0';  // satır paritesi
        printf(" | Row Parity = %c\n", rowParity[i]);
    }

    rowParity[len] = '\0';

    // Sütun pariteleri
    for (int j = 0; j < 8; j++)
        colParity[j] = (colOnes[j] % 2) + '0';

    colParity[8] = '\0';

    printf("\nSütun Pariteleri: %s\n", colParity);
}
// CRC-8 polinom: x^8 + x^2 + x + 1 = 0x07
unsigned char crc8(unsigned char *data, int len) {
    unsigned char crc = 0; // başlangıç değeri

    for (int i = 0; i < len; i++) {
        crc ^= data[i];  // XOR ile başlat

        for (int j = 0; j < 8; j++) {  // her bit için
            if (crc & 0x80)  // en soldaki bit 1 ise
                crc = (crc << 1) ^ 0x07;  // sola kaydır ve polinom ile XOR
            else
                crc <<= 1;  // sadece sola kaydır
        }
    }

    return crc;  // 8 bit CRC
}
// 8-bitlik char → 12-bit Hamming kodu (parite bitleri dahil)
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
uint16_t internetChecksum(unsigned char *data, int len) {
    uint32_t sum = 0;

    // 2 byte = 16 bit ile topla
    for (int i = 0; i < len; i += 2) {
        uint16_t word = data[i] << 8; // yüksek byte
        if (i + 1 < len)
            word |= data[i + 1];      // düşük byte
        sum += word;

        // Taşma olursa tekrar eklensin
        if (sum > 0xFFFF)
            sum = (sum & 0xFFFF) + 1;
    }

    // 1’in komplemanı
    sum = ~sum;

    return (uint16_t)sum;
}

int main(){
    printf("Client 1 su an calisiyor !\n");
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
    printf("Socket olusturulamadi!\n");
    return 1;
}

struct sockaddr_in server;
server.sin_family = AF_INET;
server.sin_port = htons(8080);  // Server portu
server.sin_addr.s_addr = inet_addr("127.0.0.1");  // Server IP

if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
    printf("Sunucuya baglanilamadi!\n");
    return 1;
}

printf("Sunucuya baglanti basarili!\n");

    for(;;){
        printf("Bir secim yapınız:\n");
        liste();
        int secim=0;
        scanf("%d", &secim);
        getchar(); //enter veya new line karakterini buffera alıyor fgets te sorun çıkmasın diye
        if(secim==-1){
            printf("Cikis yapiliyor...\n");
            close(sock);
            break;
        }
        char metin [100];
        printf("Metin giriniz:\n");
        fgets(metin,sizeof(metin),stdin);
        metin[strcspn(metin, "\n")] = '\0';
        switch(secim){
            case 1:{
                char packet[256];
            sprintf(packet, "%s|PARITY|%d", metin, parityHesapla(metin));

            write(sock, packet, strlen(packet));


            }
            
            break;
            case 2:
            {
                char rowParity[strlen(metin) + 1]; // Satır paritesi için dizi
                hesapla2DParity(metin, rowParity, colParity);
                char packet[1024];
                sprintf(packet, "%s|2DPARITY|%s|%s", metin, rowParity, colParity);

                printf("Sunucuya gönderilecek paket: %s\n", packet);
                write(sock, packet, strlen(packet));
            }
            break;
            case 3:
            {
                unsigned char crc = crc8((unsigned char*)metin, strlen(metin));
                printf("Metin: %s\n", metin);
                printf("CRC-8: 0x%02X\n", crc);
                char packet[300];
                sprintf(packet, "%s|CRC8|%02X", metin, crc);

                printf("Sunucuya gönderilecek paket: %s\n", packet);
                write(sock, packet, strlen(packet));

            }
            break;
            case 4:{
                char packet[4096] = "";
                char hamming[13];

                for (int i = 0; i < strlen(metin); i++) {
                    hammingEncode(metin[i], hamming);
                    strcat(packet, hamming);
                }

                // Paket hazır
                char sendPacket[8192];
                sprintf(sendPacket, "%s|HAMMING|%s", metin, packet);

                printf("Sunucuya gönderilecek paket: %s\n", sendPacket);
                write(sock, sendPacket, strlen(sendPacket));

            }
            break;
            case 5:{
                uint16_t checksum = internetChecksum((unsigned char*)metin, strlen(metin));

                printf("Metin: %s\n", metin);
                printf("IP Checksum: 0x%04X\n", checksum);

                // Sunucuya gönderilecek paket
                char packet[512];
                sprintf(packet, "%s|IPCHECKSUM|%04X", metin, checksum);

                printf("Sunucuya gönderilecek paket: %s\n", packet);
                write(sock, packet, strlen(packet));
            }
        break;
        default:
            printf("Geçersiz seçim tekrar deneyiniz!\n");
        }
        if(secim==0){
            break;
        }
    }
    
    return 0;
}