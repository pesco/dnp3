#include <stdio.h>
#include <unistd.h>
#include <dnp3.h>

#define BUFLEN 16

int main(int argc, char *argv[])
{
    uint8_t buf[BUFLEN];
    size_t n=0, m;

    // while stdin open, read additional input into buf
    do {
        m = read(0, buf+n, BUFLEN-n);

        n += m;
        if(n==BUFLEN || m==0) {
            uint16_t crc = dnp3_crc(buf, n);
            uint8_t lo=crc&0xFF, hi=crc>>8;
            printf("0x%.4X  \"\\x%.2X\\x%.2X\"  %.2X%.2X\n", crc, lo,hi, lo,hi);
            n = 0;
        }
    } while(m); // not eof

    return 0;
}
