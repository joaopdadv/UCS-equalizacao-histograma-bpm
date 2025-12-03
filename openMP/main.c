#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <omp.h>

typedef struct
{
    uint16_t bfType;
    uint32_t bfSize; // Tamanho do arquivo
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits; // Offset para os dados da imagem
} BMPHeader;

typedef struct
{
    uint32_t biSize;        // Tamanho do cabeçalho info
    int32_t biWidth;        // Largura
    int32_t biHeight;       // Altura
    uint16_t biPlanes;      // Planos
    uint16_t biBitCount;    // Bits por pixel
    uint32_t biCompression; // Compressão
    uint32_t biSizeImage;   // Tamanho da imagem comprimida
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMPInfoHeader;

typedef struct
{
    int width;
    int height;
    unsigned char *data;
} Image;

Image *leBitMap(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        printf("Erro ao abrir arquivo %s\n", filename);
        return NULL;
    }

    BMPHeader bmpHeader;
    fread(&bmpHeader.bfType, sizeof(uint16_t), 1, f);
    fread(&bmpHeader.bfSize, sizeof(uint32_t), 1, f);
    fread(&bmpHeader.bfReserved1, sizeof(uint16_t), 1, f);
    fread(&bmpHeader.bfReserved2, sizeof(uint16_t), 1, f);
    fread(&bmpHeader.bfOffBits, sizeof(uint32_t), 1, f);

    BMPInfoHeader bmpInfo;
    fread(&bmpInfo.biSize, sizeof(uint32_t), 1, f);
    fread(&bmpInfo.biWidth, sizeof(int32_t), 1, f);
    fread(&bmpInfo.biHeight, sizeof(int32_t), 1, f);
    fread(&bmpInfo.biPlanes, sizeof(uint16_t), 1, f);
    fread(&bmpInfo.biBitCount, sizeof(uint16_t), 1, f);
    fread(&bmpInfo.biCompression, sizeof(uint32_t), 1, f);
    fread(&bmpInfo.biSizeImage, sizeof(uint32_t), 1, f);
    fread(&bmpInfo.biXPelsPerMeter, sizeof(int32_t), 1, f);
    fread(&bmpInfo.biYPelsPerMeter, sizeof(int32_t), 1, f);
    fread(&bmpInfo.biClrUsed, sizeof(uint32_t), 1, f);
    fread(&bmpInfo.biClrImportant, sizeof(uint32_t), 1, f);

    if (bmpHeader.bfType != 0x4D42)
    {
        printf("Arquivo não é um BMP válido.\n");
        fclose(f);
        return NULL;
    }
    if (bmpInfo.biBitCount != 24)
    {
        printf("Este programa suporta apenas BMP 24 bits.\n");
        fclose(f);
        return NULL;
    }

    Image *img = (Image *)malloc(sizeof(Image));
    img->width = bmpInfo.biWidth;
    img->height = abs(bmpInfo.biHeight);

    img->data = (unsigned char *)malloc(img->width * img->height * 3);

    int padding = (4 - (img->width * 3) % 4) % 4;

    fseek(f, bmpHeader.bfOffBits, SEEK_SET);

    // Ler pixels
    for (int y = 0; y < img->height; y++)
    {
        for (int x = 0; x < img->width; x++)
        {
            // Ler B, G, R
            int index = (y * img->width + x) * 3;
            fread(&img->data[index], 3, 1, f);
        }
        fseek(f, padding, SEEK_CUR);
    }

    fclose(f);
    return img;
}

void escreveBitMap(const char *filename, Image *img)
{
    FILE *f = fopen(filename, "wb");
    if (!f)
    {
        printf("Erro ao criar arquivo %s\n", filename);
        return;
    }

    int padding = (4 - (img->width * 3) % 4) % 4;
    int dataSize = (img->width * 3 + padding) * img->height;

    BMPHeader bmpHeader;
    bmpHeader.bfType = 0x4D42;
    bmpHeader.bfSize = 14 + 40 + dataSize;
    bmpHeader.bfReserved1 = 0;
    bmpHeader.bfReserved2 = 0;
    bmpHeader.bfOffBits = 14 + 40;

    BMPInfoHeader bmpInfo;
    bmpInfo.biSize = 40;
    bmpInfo.biWidth = img->width;
    bmpInfo.biHeight = img->height;
    bmpInfo.biPlanes = 1;
    bmpInfo.biBitCount = 24;
    bmpInfo.biCompression = 0;
    bmpInfo.biSizeImage = dataSize;
    bmpInfo.biXPelsPerMeter = 0;
    bmpInfo.biYPelsPerMeter = 0;
    bmpInfo.biClrUsed = 0;
    bmpInfo.biClrImportant = 0;

    fwrite(&bmpHeader.bfType, sizeof(uint16_t), 1, f);
    fwrite(&bmpHeader.bfSize, sizeof(uint32_t), 1, f);
    fwrite(&bmpHeader.bfReserved1, sizeof(uint16_t), 1, f);
    fwrite(&bmpHeader.bfReserved2, sizeof(uint16_t), 1, f);
    fwrite(&bmpHeader.bfOffBits, sizeof(uint32_t), 1, f);

    fwrite(&bmpInfo.biSize, sizeof(uint32_t), 1, f);
    fwrite(&bmpInfo.biWidth, sizeof(int32_t), 1, f);
    fwrite(&bmpInfo.biHeight, sizeof(int32_t), 1, f);
    fwrite(&bmpInfo.biPlanes, sizeof(uint16_t), 1, f);
    fwrite(&bmpInfo.biBitCount, sizeof(uint16_t), 1, f);
    fwrite(&bmpInfo.biCompression, sizeof(uint32_t), 1, f);
    fwrite(&bmpInfo.biSizeImage, sizeof(uint32_t), 1, f);
    fwrite(&bmpInfo.biXPelsPerMeter, sizeof(int32_t), 1, f);
    fwrite(&bmpInfo.biYPelsPerMeter, sizeof(int32_t), 1, f);
    fwrite(&bmpInfo.biClrUsed, sizeof(uint32_t), 1, f);
    fwrite(&bmpInfo.biClrImportant, sizeof(uint32_t), 1, f);

    for (int y = 0; y < img->height; y++)
    {
        for (int x = 0; x < img->width; x++)
        {
            int index = (y * img->width + x) * 3;
            fwrite(&img->data[index], 3, 1, f);
        }
        unsigned char pad[3] = {0, 0, 0};
        fwrite(pad, 1, padding, f);
    }

    fclose(f);
}

int compare(const void *a, const void *b)
{
    return (*(unsigned char *)a - *(unsigned char *)b);
}

void filtroMediana(Image *img, int n_filter)
{
    int w = img->width;
    int h = img->height;
    unsigned char *newData = (unsigned char *)malloc(w * h * 3);

    int offset = n_filter / 2;
    const int windowSize = n_filter * n_filter;

#pragma omp parallel for
    for (int y = 0; y < h; y++)
    {
        unsigned char windowR[windowSize];
        unsigned char windowG[windowSize];
        unsigned char windowB[windowSize];

        for (int x = 0; x < w; x++)
        {
            int currentIdx = (y * w + x) * 3;

            if (y < offset || y >= h - offset || x < offset || x >= w - offset)
            {
                newData[currentIdx] = img->data[currentIdx];
                newData[currentIdx + 1] = img->data[currentIdx + 1];
                newData[currentIdx + 2] = img->data[currentIdx + 2];
                continue;
            }

            int count = 0;
            for (int ky = -offset; ky <= offset; ky++)
            {
                for (int kx = -offset; kx <= offset; kx++)
                {
                    int neighborIdx = ((y + ky) * w + (x + kx)) * 3;
                    windowB[count] = img->data[neighborIdx];
                    windowG[count] = img->data[neighborIdx + 1];
                    windowR[count] = img->data[neighborIdx + 2];
                    count++;
                }
            }

            qsort(windowB, windowSize, sizeof(unsigned char), compare);
            qsort(windowG, windowSize, sizeof(unsigned char), compare);
            qsort(windowR, windowSize, sizeof(unsigned char), compare);

            newData[currentIdx] = windowB[windowSize / 2];
            newData[currentIdx + 1] = windowG[windowSize / 2];
            newData[currentIdx + 2] = windowR[windowSize / 2];
        }
    }

    free(img->data);
    img->data = newData;
    printf("1. Filtro Mediana %dx%d aplicado (Paralelo).\n", n_filter, n_filter);
}

void grayscale(Image *img)
{
    int w = img->width;
    int h = img->height;
    int totalPixels = w * h;

#pragma omp parallel for
    for (int i = 0; i < totalPixels; i++)
    {
        int idx = i * 3;
        unsigned char b = img->data[idx];
        unsigned char g = img->data[idx + 1];
        unsigned char r = img->data[idx + 2];

        unsigned char gray = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);

        img->data[idx] = gray;
        img->data[idx + 1] = gray;
        img->data[idx + 2] = gray;
    }
    printf("2. Conversão para Tons de Cinza aplicada (Paralelo).\n");
}

void equalizacao(Image *img)
{
    int w = img->width;
    int h = img->height;
    int totalPixels = w * h;
    int histogram[256] = {0};

#pragma omp parallel
    {
        int local_histogram[256] = {0};

#pragma omp for
        for (int i = 0; i < totalPixels; i++)
        {
            unsigned char val = img->data[i * 3];
            local_histogram[val]++;
        }

#pragma omp critical
        {
            for (int j = 0; j < 256; j++)
            {
                histogram[j] += local_histogram[j];
            }
        }
    }

    int cdf[256] = {0};
    cdf[0] = histogram[0];
    for (int i = 1; i < 256; i++)
    {
        cdf[i] = cdf[i - 1] + histogram[i];
    }

    int cdfMin = 0;
    for (int i = 0; i < 256; i++)
    {
        if (cdf[i] > 0)
        {
            cdfMin = cdf[i];
            break;
        }
    }

    unsigned char map[256];
    for (int i = 0; i < 256; i++)
    {
        float num = (float)(cdf[i] - cdfMin);
        float den = (float)(totalPixels - cdfMin);
        int val = (int)round((num / den) * 255.0);

        if (val < 0)
            val = 0;
        if (val > 255)
            val = 255;
        map[i] = (unsigned char)val;
    }

#pragma omp parallel for
    for (int i = 0; i < totalPixels; i++)
    {
        int idx = i * 3;
        unsigned char oldVal = img->data[idx];
        unsigned char newVal = map[oldVal];

        img->data[idx] = newVal;
        img->data[idx + 1] = newVal;
        img->data[idx + 2] = newVal;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Uso: %s <tamanho_filtro_N> <num_threads>\n", argv[0]);
        return 1;
    }

    int n_filter = atoi(argv[1]);
    if (n_filter % 2 == 0)
        n_filter++; // Garante ímpar

    int num_threads = atoi(argv[2]);

    omp_set_num_threads(num_threads);

    char inputFilename[] = "../bitmaps/small.bmp";
    char outputFilename[] = "output_paralelo.bmp";

    printf("Threads maximas disponiveis: %d\n", omp_get_max_threads());

    Image *img = leBitMap(inputFilename);
    if (!img)
    {
        printf("Erro: Arquivo '%s' nao encontrado.\n", inputFilename);
        return 1;
    }

    double start_time = omp_get_wtime();

    filtroMediana(img, n_filter);
    grayscale(img);
    equalizacao(img);

    double end_time = omp_get_wtime();
    printf("Tempo total de processamento: %.4f segundos.\n", end_time - start_time);

    escreveBitMap(outputFilename, img);
    printf("Imagem salva em '%s'.\n", outputFilename);

    free(img->data);
    free(img);

    return 0;
}