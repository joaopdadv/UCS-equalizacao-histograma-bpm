#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <mpi.h>

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

unsigned char *leBitMap(const char *filename, int *w, int *h, BMPHeader *outHead, BMPInfoHeader *outInfo)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
        return NULL;

    fread(&outHead->bfType, sizeof(uint16_t), 1, f);
    fread(&outHead->bfSize, sizeof(uint32_t), 1, f);
    fread(&outHead->bfReserved1, sizeof(uint16_t), 1, f);
    fread(&outHead->bfReserved2, sizeof(uint16_t), 1, f);
    fread(&outHead->bfOffBits, sizeof(uint32_t), 1, f);

    fread(&outInfo->biSize, sizeof(uint32_t), 1, f);
    fread(&outInfo->biWidth, sizeof(int32_t), 1, f);
    fread(&outInfo->biHeight, sizeof(int32_t), 1, f);
    fread(&outInfo->biPlanes, sizeof(uint16_t), 1, f);
    fread(&outInfo->biBitCount, sizeof(uint16_t), 1, f);
    fread(&outInfo->biCompression, sizeof(uint32_t), 1, f);
    fread(&outInfo->biSizeImage, sizeof(uint32_t), 1, f);
    fread(&outInfo->biXPelsPerMeter, sizeof(int32_t), 1, f);
    fread(&outInfo->biYPelsPerMeter, sizeof(int32_t), 1, f);
    fread(&outInfo->biClrUsed, sizeof(uint32_t), 1, f);
    fread(&outInfo->biClrImportant, sizeof(uint32_t), 1, f);

    if (outHead->bfType != 0x4D42 || outInfo->biBitCount != 24)
    {
        fclose(f);
        return NULL;
    }

    *w = outInfo->biWidth;
    *h = abs(outInfo->biHeight);

    unsigned char *data = (unsigned char *)malloc((*w) * (*h) * 3);
    int padding = (4 - ((*w) * 3) % 4) % 4;

    fseek(f, outHead->bfOffBits, SEEK_SET);

    for (int y = 0; y < *h; y++)
    {
        for (int x = 0; x < *w; x++)
        {
            int index = (y * (*w) + x) * 3;
            fread(&data[index], 3, 1, f);
        }
        fseek(f, padding, SEEK_CUR);
    }
    fclose(f);
    return data;
}

void escreveBitMap(const char *filename, int w, int h, unsigned char *data, BMPHeader head, BMPInfoHeader info)
{
    FILE *f = fopen(filename, "wb");
    if (!f)
        return;

    int padding = (4 - (w * 3) % 4) % 4;
    int dataSize = (w * 3 + padding) * h;

    head.bfSize = 14 + 40 + dataSize;
    info.biSizeImage = dataSize;

    fwrite(&head.bfType, sizeof(uint16_t), 1, f);
    fwrite(&head.bfSize, sizeof(uint32_t), 1, f);
    fwrite(&head.bfReserved1, sizeof(uint16_t), 1, f);
    fwrite(&head.bfReserved2, sizeof(uint16_t), 1, f);
    fwrite(&head.bfOffBits, sizeof(uint32_t), 1, f);

    fwrite(&info.biSize, sizeof(uint32_t), 1, f);
    fwrite(&info.biWidth, sizeof(int32_t), 1, f);
    fwrite(&info.biHeight, sizeof(int32_t), 1, f);
    fwrite(&info.biPlanes, sizeof(uint16_t), 1, f);
    fwrite(&info.biBitCount, sizeof(uint16_t), 1, f);
    fwrite(&info.biCompression, sizeof(uint32_t), 1, f);
    fwrite(&info.biSizeImage, sizeof(uint32_t), 1, f);
    fwrite(&info.biXPelsPerMeter, sizeof(int32_t), 1, f);
    fwrite(&info.biYPelsPerMeter, sizeof(int32_t), 1, f);
    fwrite(&info.biClrUsed, sizeof(uint32_t), 1, f);
    fwrite(&info.biClrImportant, sizeof(uint32_t), 1, f);

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int index = (y * w + x) * 3;
            fwrite(&data[index], 3, 1, f);
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

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (argc < 2)
    {
        if (world_rank == 0)
            printf("Uso: mpirun -np X %s <tamanho_filtro_N>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    int n_filter = atoi(argv[1]);
    if (n_filter % 2 == 0)
        n_filter++;
    int offset = n_filter / 2;

    int w, h;
    unsigned char *full_img = NULL;
    BMPHeader bmpHead;
    BMPInfoHeader bmpInfo;

    if (world_rank == 0)
    {
        full_img = readBMP("../bitmaps/small.bmp", &w, &h, &bmpHead, &bmpInfo);
        if (!full_img)
        {
            printf("Erro ao ler ../bitmaps/small.bmp\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        printf("MPI iniciado com %d processos. Filtro: %dx%d\n", world_size, n_filter, n_filter);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();

    MPI_Bcast(&w, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&h, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int rows_per_proc = h / world_size;
    int remainder = h % world_size;

    int *sendcounts = NULL;
    int *displs = NULL;
    int *recvcounts_res = NULL;
    int *displs_res = NULL;

    if (world_rank == 0)
    {
        sendcounts = (int *)malloc(world_size * sizeof(int));
        displs = (int *)malloc(world_size * sizeof(int));
        recvcounts_res = (int *)malloc(world_size * sizeof(int));
        displs_res = (int *)malloc(world_size * sizeof(int));

        int current_row = 0;
        for (int i = 0; i < world_size; i++)
        {
            int rows = rows_per_proc + (i < remainder ? 1 : 0);

            recvcounts_res[i] = rows * w * 3;
            displs_res[i] = current_row * w * 3;

            int start_r = current_row - offset;
            int end_r = current_row + rows + offset;

            if (start_r < 0)
                start_r = 0;
            if (end_r > h)
                end_r = h;

            int rows_to_send = end_r - start_r;
            sendcounts[i] = rows_to_send * w * 3;
            displs[i] = start_r * w * 3;

            current_row += rows;
        }
    }

    int my_rows_output = rows_per_proc + (world_rank < remainder ? 1 : 0);

    int my_start_global_y = 0;
    for (int i = 0; i < world_rank; i++)
        my_start_global_y += (rows_per_proc + (i < remainder ? 1 : 0));

    int start_r_local = my_start_global_y - offset;
    int end_r_local = my_start_global_y + my_rows_output + offset;
    if (start_r_local < 0)
        start_r_local = 0;
    if (end_r_local > h)
        end_r_local = h;
    int my_rows_input = end_r_local - start_r_local;

    unsigned char *local_input_buf = (unsigned char *)malloc(my_rows_input * w * 3);

    MPI_Scatterv(full_img, sendcounts, displs, MPI_UNSIGNED_CHAR,
                 local_input_buf, my_rows_input * w * 3, MPI_UNSIGNED_CHAR,
                 0, MPI_COMM_WORLD);

    unsigned char *local_output_buf = (unsigned char *)malloc(my_rows_output * w * 3);

    int window_size = n_filter * n_filter;
    unsigned char *winR = (unsigned char *)malloc(window_size);
    unsigned char *winG = (unsigned char *)malloc(window_size);
    unsigned char *winB = (unsigned char *)malloc(window_size);

    for (int y = 0; y < my_rows_output; y++)
    {
        int global_y = my_start_global_y + y;

        int input_y_base = global_y - start_r_local;

        for (int x = 0; x < w; x++)
        {
            int out_idx = (y * w + x) * 3;

            if (global_y < offset || global_y >= h - offset || x < offset || x >= w - offset)
            {
                int in_idx = (input_y_base * w + x) * 3;
                local_output_buf[out_idx] = local_input_buf[in_idx];
                local_output_buf[out_idx + 1] = local_input_buf[in_idx + 1];
                local_output_buf[out_idx + 2] = local_input_buf[in_idx + 2];
                continue;
            }

            int count = 0;
            for (int ky = -offset; ky <= offset; ky++)
            {
                for (int kx = -offset; kx <= offset; kx++)
                {
                    int ny = input_y_base + ky;
                    int nx = x + kx;

                    int in_idx = (ny * w + nx) * 3;
                    winB[count] = local_input_buf[in_idx];
                    winG[count] = local_input_buf[in_idx + 1];
                    winR[count] = local_input_buf[in_idx + 2];
                    count++;
                }
            }

            qsort(winB, window_size, sizeof(unsigned char), compare);
            qsort(winG, window_size, sizeof(unsigned char), compare);
            qsort(winR, window_size, sizeof(unsigned char), compare);

            local_output_buf[out_idx] = winB[window_size / 2];
            local_output_buf[out_idx + 1] = winG[window_size / 2];
            local_output_buf[out_idx + 2] = winR[window_size / 2];
        }
    }

    free(winR);
    free(winG);
    free(winB);
    free(local_input_buf);

    for (int i = 0; i < my_rows_output * w; i++)
    {
        int idx = i * 3;
        unsigned char b = local_output_buf[idx];
        unsigned char g = local_output_buf[idx + 1];
        unsigned char r = local_output_buf[idx + 2];

        unsigned char gray = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);

        local_output_buf[idx] = gray;
        local_output_buf[idx + 1] = gray;
        local_output_buf[idx + 2] = gray;
    }

    long local_hist[256] = {0};
    for (int i = 0; i < my_rows_output * w; i++)
    {
        local_hist[local_output_buf[i * 3]]++;
    }

    long global_hist[256] = {0};
    MPI_Allreduce(local_hist, global_hist, 256, MPI_LONG, MPI_SUM, MPI_COMM_WORLD);

    long cdf[256] = {0};
    cdf[0] = global_hist[0];
    for (int i = 1; i < 256; i++)
        cdf[i] = cdf[i - 1] + global_hist[i];

    long cdfMin = 0;
    for (int i = 0; i < 256; i++)
    {
        if (cdf[i] > 0)
        {
            cdfMin = cdf[i];
            break;
        }
    }

    unsigned char map[256];
    long totalPixels = w * h;
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

    for (int i = 0; i < my_rows_output * w; i++)
    {
        int idx = i * 3;
        unsigned char oldVal = local_output_buf[idx];
        unsigned char newVal = map[oldVal];
        local_output_buf[idx] = newVal;
        local_output_buf[idx + 1] = newVal;
        local_output_buf[idx + 2] = newVal;
    }

    MPI_Gatherv(local_output_buf, my_rows_output * w * 3, MPI_UNSIGNED_CHAR,
                full_img, recvcounts_res, displs_res, MPI_UNSIGNED_CHAR,
                0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double end_time = MPI_Wtime();

    if (world_rank == 0)
    {
        printf("Tempo Total: %.6f s\n", end_time - start_time);
        writeBMP("output_mpi.bmp", w, h, full_img, bmpHead, bmpInfo);
        printf("Imagem salva em output_mpi.bmp\n");
        free(full_img);
        free(sendcounts);
        free(displs);
        free(recvcounts_res);
        free(displs_res);
    }

    free(local_output_buf);
    MPI_Finalize();
    return 0;
}