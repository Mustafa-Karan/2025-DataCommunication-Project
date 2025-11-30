#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#define PORT_C1 8080
#define PORT_C2 8081
#define BACKLOG 5 // Maksimum bekleyen bağlantı sayısı
#define BUF_SIZE 8192

// Rastgele sayı fonksiyonu
int rand_range(int a, int b) { return a + rand() % (b - a + 1); }

// Bozma (corruption) metodları 
void bit_flip_once(unsigned char *s, int len) {
    if (len <= 0) return;
    int byte_idx = rand_range(0, len - 1);
    int bit = rand_range(0, 7);
    s[byte_idx] ^= (1 << bit);
}

void multiple_bit_flips(unsigned char *s, int len) {
    if (len <= 0) return;
    int flips = rand_range(2, (len*8 > 16 ? 8 : len*8)); // birkaç bit flip
    for (int i = 0; i < flips; ++i) {
        int byte_idx = rand_range(0, len - 1);
        int bit = rand_range(0, 7);
        s[byte_idx] ^= (1 << bit);
    }
}

void char_substitution(unsigned char *s, int len) {
    if (len <= 0) return;
    int idx = rand_range(0, len - 1);
    unsigned char orig = s[idx];
    unsigned char newc;
    do {
        newc = (unsigned char)rand_range(32, 126); // yazdırılabilir karakter
    } while (newc == orig);
    s[idx] = newc;
}

void char_deletion(unsigned char *s, int *plen) {
    if (*plen <= 1) return;
    int idx = rand_range(0, (*plen) - 1);
    memmove(&s[idx], &s[idx+1], (*plen) - idx - 1);
    (*plen)--;
}

void char_insertion(unsigned char *s, int *plen) {
    if (*plen + 1 >= BUF_SIZE - 10) return;
    int idx = rand_range(0, *plen);
    unsigned char c = (unsigned char)rand_range(32, 126);
    memmove(&s[idx+1], &s[idx], (*plen) - idx);
    s[idx] = c;
    (*plen)++;
}

void char_swapping(unsigned char *s, int len) {
    if (len <= 1) return;
    int idx = rand_range(0, len - 2);
    unsigned char tmp = s[idx];
    s[idx] = s[idx+1];
    s[idx+1] = tmp;
}

void burst_error(unsigned char *s, int *plen) {
    if (*plen <= 0) return;
    int burst_len = rand_range(3, (*plen < 8 ? *plen : 8));
    int start = rand_range(0, (*plen) - burst_len);
    for (int i = 0; i < burst_len; ++i) {
        int bit = rand_range(0, 7);
        s[start + i] ^= (1 << bit);
    }
}

// Rastgele bir bozma (corruption) metodu seçme
void apply_random_corruption(unsigned char *data, int *dlen) {
    int method = rand_range(1, 7);
    switch (method) {
        case 1: bit_flip_once(data, *dlen); printf("[Bozma] Bit Flip (1 kez)\n"); break;
        case 2: char_substitution(data, *dlen); printf("[Bozma] Karakter Substitution\n"); break;
        case 3: char_deletion(data, dlen); printf("[Bozma] Karakter Silme\n"); break;
        case 4: char_insertion(data, dlen); printf("[Bozma] Rastgele Karakter Ekleme\n"); break;
        case 5: char_swapping(data, *dlen); printf("[Bozma] Karakter Yer Değiştirme\n"); break;
        case 6: multiple_bit_flips(data, *dlen); printf("[Bozma] Birden Fazla Bit Flip\n"); break;
        case 7: burst_error(data, dlen); printf("[Bozma] Burst Error\n"); break;
        default: bit_flip_once(data, *dlen); break;
    }
}

// Buffer içinde ilk ve ikinci '|' konumlarını bulma kısmı
// p1 ve p2 içine konumları yazar — bulunursa 1, bulunamazsa 0 döner
int find_field_dividers(const char *buf, int buflen, int *p1, int *p2) {
    int first = -1, second = -1;
    for (int i = 0; i < buflen; ++i) {
        if (buf[i] == '|') {
            if (first == -1) first = i;
            else { second = i; break; }
        }
    }
    if (first == -1 || second == -1) return 0;
    *p1 = first; *p2 = second;
    return 1;
}

int main() {
    srand(time(NULL));
    int listen_c1 = -1, listen_c2 = -1;
    int sock_c1 = -1, sock_c2 = -1;

    // Client1 için (gönderici) dinleyici oluştur
    if ((listen_c1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket c1");
        exit(1);
    }
    int opt = 1;
    setsockopt(listen_c1, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr_c1 = {0};
    addr_c1.sin_family = AF_INET;
    addr_c1.sin_addr.s_addr = INADDR_ANY;
    addr_c1.sin_port = htons(PORT_C1);

    if (bind(listen_c1, (struct sockaddr*)&addr_c1, sizeof(addr_c1)) < 0) {
        perror("bind c1");
        exit(1);
    }
    if (listen(listen_c1, BACKLOG) < 0) {
        perror("listen c1");
        exit(1);
    }
    printf("Client1 (gönderici) için %d portunda dinleniyor...\n", PORT_C1);

    // Client2 için (alıcı) dinleyici oluştur
    if ((listen_c2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket c2");
        exit(1);
    }
    setsockopt(listen_c2, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr_c2 = {0};
    addr_c2.sin_family = AF_INET;
    addr_c2.sin_addr.s_addr = INADDR_ANY;
    addr_c2.sin_port = htons(PORT_C2);

    if (bind(listen_c2, (struct sockaddr*)&addr_c2, sizeof(addr_c2)) < 0) {
        perror("bind c2");
        exit(1);
    }
    if (listen(listen_c2, BACKLOG) < 0) {
        perror("listen c2");
        exit(1);
    }
    printf("Client2 (alıcı) için %d portunda dinleniyor...\n", PORT_C2);

    // Her iki client bağlantısını bekle
    printf("Her iki client'ın bağlanması bekleniyor...\n");

    fd_set master, read_fds;
    int fdmax = (listen_c1 > listen_c2 ? listen_c1 : listen_c2);
    FD_ZERO(&master);
    FD_SET(listen_c1, &master);
    FD_SET(listen_c2, &master);

    // Her iki client bağlanana kadar bekle
    while (sock_c1 < 0 || sock_c2 < 0) {
        read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(1);
        }
        if (FD_ISSET(listen_c1, &read_fds) && sock_c1 < 0) {
            struct sockaddr_in cli; socklen_t len = sizeof(cli);
            sock_c1 = accept(listen_c1, (struct sockaddr*)&cli, &len);
            if (sock_c1 >= 0) {
                printf("Client1 bağlandı: %s:%d\n", inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));
            } else perror("accept c1");
        }
        if (FD_ISSET(listen_c2, &read_fds) && sock_c2 < 0) {
            struct sockaddr_in cli; socklen_t len = sizeof(cli);
            sock_c2 = accept(listen_c2, (struct sockaddr*)&cli, &len);
            if (sock_c2 >= 0) {
                printf("Client2 bağlandı: %s:%d\n", inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));
            } else perror("accept c2");
        }
    }

    printf("Her iki client bağlandı. Paket yönlendirme döngüsüne giriliyor.\n");

    // Ana döngü: Client1’den paketi al, boz ve Client2’ye gönder
    while (1) {
        char buf[BUF_SIZE];
        ssize_t r = recv(sock_c1, buf, sizeof(buf)-1, 0);
        if (r <= 0) {
            if (r == 0) printf("Client1 bağlantıyı kapattı.\n");
            else perror("recv from client1");
            close(sock_c1);
            sock_c1 = -1;

            printf("Yeni Client1 bekleniyor...\n");
            struct sockaddr_in cli; socklen_t len = sizeof(cli);
            sock_c1 = accept(listen_c1, (struct sockaddr*)&cli, &len);
            if (sock_c1 >= 0) printf("Client1 tekrar bağlandı: %s:%d\n", inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));
            else { perror("accept c1"); break; }
            continue;
        }
        buf[r] = '\0';

        while (r > 0 && (buf[r-1] == '\n' || buf[r-1] == '\r')) { buf[r-1] = '\0'; r--; }

        printf("\nClient1’den paket alındı (%zd bayt): %s\n", r, buf);

        int p1 = -1, p2 = -1;
        if (!find_field_dividers(buf, r, &p1, &p2)) {
            printf("Paket formatı geçersiz ('|' yok). Değiştirmeden yönlendiriliyor.\n");
            ssize_t s = send(sock_c2, buf, r, 0);
            if (s < 0) perror("send to client2");
            else printf("Paket Client2’ye değişmeden iletildi (%zd bayt).\n", s);
            continue;
        }

        // DATA kısmı 0..p1-1 arası
        int data_len = p1;
        unsigned char data[BUF_SIZE];
        if (data_len >= BUF_SIZE-1) data_len = BUF_SIZE-2;
        memcpy(data, buf, data_len);
        data[data_len] = '\0';

        // Suffix: |METHOD|CONTROL...
        char suffix[BUF_SIZE];
        int suffix_len = r - p1;
        if (suffix_len >= BUF_SIZE-1) suffix_len = BUF_SIZE-2;
        memcpy(suffix, &buf[p1], suffix_len);
        suffix[suffix_len] = '\0';

        printf("Ayrıştırıldı -> DATA(uz=%d): \"%.*s\"\n", data_len, data_len, data);
        printf("Ayrıştırıldı -> SUFFIX: \"%s\"\n", suffix);

        int new_data_len = data_len;
        unsigned char work[BUF_SIZE];
        memcpy(work, data, data_len);
        work[data_len] = '\0';

        apply_random_corruption(work, &new_data_len);

        char outbuf[BUF_SIZE];
        if (new_data_len + suffix_len >= BUF_SIZE-2) {
            printf("Bozulmuş paket çok büyük, DATA kısmı kesiliyor.\n");
            new_data_len = BUF_SIZE - 2 - suffix_len;
        }
        memcpy(outbuf, work, new_data_len);
        memcpy(outbuf + new_data_len, suffix, suffix_len);
        int outlen = new_data_len + suffix_len;
        outbuf[outlen] = '\0';

        printf("Bozulmuş paket Client2’ye gönderiliyor (%d bayt): %s\n", outlen, outbuf);

        ssize_t sent = send(sock_c2, outbuf, outlen, 0);
        if (sent < 0) {
            perror("send to client2");
            close(sock_c2);
            sock_c2 = -1;

            printf("Yeni Client2 bekleniyor...\n");
            struct sockaddr_in cli; socklen_t len = sizeof(cli);
            sock_c2 = accept(listen_c2, (struct sockaddr*)&cli, &len);
            if (sock_c2 >= 0)
                printf("Client2 tekrar bağlandı: %s:%d\n", inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));
            else { perror("accept c2"); break; }
        } else {
            printf("Client2’ye %zd bayt gönderildi.\n", sent);
        }
    }

    if (sock_c1 >= 0) close(sock_c1);
    if (sock_c2 >= 0) close(sock_c2);
    close(listen_c1);
    close(listen_c2);
    return 0;
}
