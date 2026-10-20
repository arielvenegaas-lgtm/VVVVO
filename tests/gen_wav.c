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
    snprintf(p, sizeof p, "%s/stereo16_48000.wav", argv[1]); write_wav(p, 48000, 2, 16, 440, 200);
    snprintf(p, sizeof p, "%s/mono16_8000.wav", argv[1]); write_wav(p, 8000, 1, 16, 200, 200);

    /* a non-WAV file that must be rejected */
    snprintf(p, sizeof p, "%s/notawav.wav", argv[1]);
    { FILE *f = fopen(p, "wb"); fwrite("NOPEnotawavfile", 1, 15, f); fclose(f); }

    /* RIFF/WAVE magic but no fmt/data chunks: must be rejected. */
    snprintf(p, sizeof p, "%s/empty.wav", argv[1]);
    { FILE *f = fopen(p, "wb");
      fwrite("RIFF", 1, 4, f); wr32(f, 4); fwrite("WAVE", 1, 4, f);
      fclose(f); }

    /* fmt present but data chunk declared bigger than the file (truncated). */
    {
        snprintf(p, sizeof p, "%s/truncated.wav", argv[1]);
        uint32_t frames = 1000, rate = 22050;
        FILE *f = fopen(p, "wb");
        fwrite("RIFF", 1, 4, f); wr32(f, 36 + frames*2); fwrite("WAVE", 1, 4, f);
        fwrite("fmt ", 1, 4, f); wr32(f, 16); wr16(f, 1); wr16(f, 1);
        wr32(f, rate); wr32(f, rate*2); wr16(f, 2); wr16(f, 16);
        fwrite("data", 1, 4, f); wr32(f, frames*2);   /* claims 2000 bytes */
        for (uint32_t i = 0; i < 100; i++) wr16(f, (uint16_t)(i*100)); /* only 200 */
        fclose(f);
    }

    /* 24-bit PCM: unsupported, must be rejected. */
    {
        snprintf(p, sizeof p, "%s/bits24.wav", argv[1]);
        uint32_t frames = 500, rate = 22050;
        FILE *f = fopen(p, "wb");
        fwrite("RIFF", 1, 4, f); wr32(f, 36 + frames*3); fwrite("WAVE", 1, 4, f);
        fwrite("fmt ", 1, 4, f); wr32(f, 16); wr16(f, 1); wr16(f, 1);
        wr32(f, rate); wr32(f, rate*3); wr16(f, 3); wr16(f, 24);
        fwrite("data", 1, 4, f); wr32(f, frames*3);
        for (uint32_t i = 0; i < frames*3; i++) fputc(0x11, f);
        fclose(f);
    }

    /* data chunk before fmt chunk: must still decode. */
    {
        snprintf(p, sizeof p, "%s/datafirst.wav", argv[1]);
        uint32_t frames = 300, rate = 16000;
        FILE *f = fopen(p, "wb");
        fwrite("RIFF", 1, 4, f); wr32(f, 36 + frames*2); fwrite("WAVE", 1, 4, f);
        fwrite("data", 1, 4, f); wr32(f, frames*2);
        for (uint32_t i = 0; i < frames; i++) wr16(f, (uint16_t)(i * 100));
        fwrite("fmt ", 1, 4, f); wr32(f, 16); wr16(f, 1); wr16(f, 1);
        wr32(f, rate); wr32(f, rate*2); wr16(f, 2); wr16(f, 16);
        fclose(f);
    }
    return 0;
}
