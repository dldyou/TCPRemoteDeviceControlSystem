#include "wav_reader.h"

#include <stdbool.h>
#include <string.h>

/**
 * Reads a little-endian unsigned 16-bit integer from raw bytes.
 * @param bytes The two input bytes.
 * @return The decoded integer.
 */
static uint16_t read_le_u16(const unsigned char bytes[2]) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

/**
 * Reads a little-endian unsigned 32-bit integer from raw bytes.
 * @param bytes The four input bytes.
 * @return The decoded integer.
 */
static uint32_t read_le_u32(const unsigned char bytes[4]) {
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24);
}

/**
 * Reads a RIFF chunk header from the WAV stream.
 * @param file The WAV file stream.
 * @param id Receives a null-terminated four-byte chunk id.
 * @param size Receives the chunk payload size.
 * @return 1 on success, 0 on read failure.
 */
static int read_chunk_header(
    FILE *file,
    char id[5],
    uint32_t *size
) {
    unsigned char size_bytes[4];

    if (fread(id, 1, 4, file) != 4) {
        return 0;
    }

    if (fread(size_bytes, 1, 4, file) != 4) {
        return 0;
    }

    id[4] = '\0';
    *size = read_le_u32(size_bytes);
    return 1;
}

/**
 * Skips a RIFF chunk payload, including padding for odd-sized chunks.
 * @param file The WAV file stream.
 * @param size The chunk payload size.
 * @return 1 on success, 0 on seek failure.
 */
static int skip_bytes(FILE *file, uint32_t size) {
    if (fseek(file, (long)size, SEEK_CUR) != 0) {
        return 0;
    }

    if ((size & 1U) != 0 && fseek(file, 1L, SEEK_CUR) != 0) {
        return 0;
    }

    return 1;
}

size_t wav_frame_size(const WavInfo *info) {
    size_t bytes_per_sample;

    if (info == NULL || info->channels == 0 || (info->bits_per_sample % 8U) != 0) {
        return 0;
    }

    bytes_per_sample = info->bits_per_sample / 8U;
    return (size_t)info->channels * bytes_per_sample;
}

/**
 * Reads one channel sample and converts it to signed 16-bit PCM.
 * @param file The WAV file stream.
 * @param info The parsed WAV metadata.
 * @param sample Receives the converted sample.
 * @return 1 on success, 0 on read or format failure.
 */
static int read_sample_as_i16(
    FILE *file,
    const WavInfo *info,
    int16_t *sample
) {
    if (info->bits_per_sample == 8) {
        unsigned char byte;

        if (fread(&byte, 1, sizeof(byte), file) != sizeof(byte)) {
            return 0;
        }

        *sample = (int16_t)(((int)byte - 128) << 8);
        return 1;
    }

    if (info->bits_per_sample == 16) {
        unsigned char bytes[2];

        if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes)) {
            return 0;
        }

        *sample = (int16_t)read_le_u16(bytes);
        return 1;
    }

    return 0;
}

int wav_read_header(FILE *file, WavInfo *info) {
    char chunk_id[5];
    unsigned char riff_size[4];
    char wave_id[5];
    bool found_fmt = false;
    bool found_data = false;

    memset(info, 0, sizeof(*info));

    if (fread(chunk_id, 1, 4, file) != 4 ||
        fread(riff_size, 1, 4, file) != 4 ||
        fread(wave_id, 1, 4, file) != 4) {
        return DEVICE_RESULT_INVALID_VALUE;
    }

    chunk_id[4] = '\0';
    wave_id[4] = '\0';

    if (memcmp(chunk_id, "RIFF", 4) != 0 || memcmp(wave_id, "WAVE", 4) != 0) {
        return DEVICE_RESULT_INVALID_VALUE;
    }

    while (!found_data) {
        char subchunk_id[5];
        uint32_t subchunk_size;

        if (!read_chunk_header(file, subchunk_id, &subchunk_size)) {
            break;
        }

        if (memcmp(subchunk_id, "fmt ", 4) == 0) {
            unsigned char fmt_header[16];

            if (subchunk_size < sizeof(fmt_header)) {
                return DEVICE_RESULT_INVALID_VALUE;
            }

            if (fread(fmt_header, 1, sizeof(fmt_header), file) != sizeof(fmt_header)) {
                return DEVICE_RESULT_INVALID_VALUE;
            }

            info->audio_format = read_le_u16(&fmt_header[0]);
            info->channels = read_le_u16(&fmt_header[2]);
            info->sample_rate = read_le_u32(&fmt_header[4]);
            info->block_align = read_le_u16(&fmt_header[12]);
            info->bits_per_sample = read_le_u16(&fmt_header[14]);
            found_fmt = true;

            if (subchunk_size > sizeof(fmt_header) &&
                !skip_bytes(file, subchunk_size - (uint32_t)sizeof(fmt_header))) {
                return DEVICE_RESULT_INVALID_VALUE;
            }
            else if ((subchunk_size & 1U) != 0 && fseek(file, 1L, SEEK_CUR) != 0) {
                return DEVICE_RESULT_INVALID_VALUE;
            }
        }
        else if (memcmp(subchunk_id, "data", 4) == 0) {
            if (!found_fmt) {
                return DEVICE_RESULT_INVALID_VALUE;
            }

            info->data_offset = (uint32_t)ftell(file);
            info->data_size = subchunk_size;
            found_data = true;
        }
        else if (!skip_bytes(file, subchunk_size)) {
            return DEVICE_RESULT_INVALID_VALUE;
        }
    }

    if (!found_fmt || !found_data) {
        return DEVICE_RESULT_INVALID_VALUE;
    }

    if (info->audio_format != 1 ||
        info->channels == 0 ||
        info->channels > 2 ||
        info->sample_rate == 0 ||
        (info->bits_per_sample != 8 && info->bits_per_sample != 16) ||
        wav_frame_size(info) == 0 ||
        info->block_align != wav_frame_size(info)) {
        return DEVICE_RESULT_INVALID_VALUE;
    }

    return DEVICE_RESULT_OK;
}

int wav_read_samples(
    FILE *file,
    const WavInfo *info,
    int16_t *samples,
    size_t max_samples,
    uint32_t *remaining_data_size
) {
    size_t samples_read = 0;
    const size_t frame_size = wav_frame_size(info);
    const size_t bytes_per_sample = info->bits_per_sample / 8U;

    if (frame_size == 0) {
        return 0;
    }

    while (samples_read < max_samples && *remaining_data_size >= frame_size) {
        int32_t mixed_sample;
        uint16_t channel;

        mixed_sample = 0;

        for (channel = 0; channel < info->channels; channel++) {
            int16_t channel_sample;

            if (!read_sample_as_i16(file, info, &channel_sample)) {
                break;
            }

            *remaining_data_size -= (uint32_t)bytes_per_sample;
            mixed_sample += channel_sample;
        }

        if (channel != info->channels) {
            break;
        }

        mixed_sample /= info->channels;
        samples[samples_read] = (int16_t)mixed_sample;
        samples_read += 1;
    }

    return (int)samples_read;
}
