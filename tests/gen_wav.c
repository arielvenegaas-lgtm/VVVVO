/* gen_wav.c: emit small deterministic WAV files for the host audio tests. */
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static void wr32(FILE *f, uint32_t v){ unsigned char b[4]={v&0xff,(v>>8)&0xff,(v>>16)&0xff,(v>>24)&0xff}; fwrite(b,1,4,f); }
static void wr16(FILE *f, uint16_t v){ unsigned char b[2]={v&0xff,(v>>8)&0xff}; fwrite(b,1,2,f); }

/* 16-bit PCM, mono or stereo, at `rate`, a sine of `hz` for `ms` millis. */
static void write_wav(const char *path, int rate, int channels, int bits,
                      int hz, int ms) {
    uint32_t frames = (uint32_t)((uint64_t)rate * ms / 1000);
    uint32_t data_len = frames * channels * (bits / 8);
    FILE *f = fopen(path, "wb");
    fwrite("RIFF", 1, 4, f); wr32(f, 36 + data_len); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); wr32(f, 16); wr16(f, 1); wr16(f, channels);
    wr32(f, rate); wr32(f, rate * channels * (bits/8));
    wr16(f, channels * (bits/8)); wr16(f, bits);
    fwrite("data", 1, 4, f); wr32(f, data_len);
    for (uint32_t i = 0; i < frames; i++) {
        double a = sin(2.0 * M_PI * hz * i / rate);
        for (int c = 0; c < channels; c++) {
            if (bits == 16) wr16(f, (uint16_t)(int16_t)(a * 30000));
            else fputc((int)(128 + a * 120), f);
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    char p[512];
    snprintf(p, sizeof p, "%s/mono16_22050.wav", argv[1]); write_wav(p, 22050, 1, 16, 440, 200);
    snprintf(p, sizeof p, "%s/stereo16_44100.wav", argv[1]); write_wav(p, 44100, 2, 16, 660, 200);
    snprintf(p, sizeof p, "%s/mono8_11025.wav", argv[1]); write_wav(p, 11025, 1, 8, 220, 200);
    /* a non-WAV file that must be rejected */
    snprintf(p, sizeof p, "%s/notawav.wav", argv[1]);
    FILE *f = fopen(p, "wb"); fwrite("NOPEnotawavfile", 1, 15, f); fclose(f);
    return 0;
}
