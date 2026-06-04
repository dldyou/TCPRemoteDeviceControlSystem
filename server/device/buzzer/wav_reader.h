#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../device_result.h"

typedef struct {
    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} WavInfo;

/**
 * Calculates the number of bytes in one interleaved WAV frame.
 * @param info The parsed WAV metadata.
 * @return The frame size in bytes, or 0 if metadata is invalid.
 */
size_t wav_frame_size(const WavInfo *info);

/**
 * Parses and validates a PCM WAV file header.
 * @param file The WAV file stream.
 * @param info Receives parsed WAV metadata.
 * @return DEVICE_RESULT_OK on success, or an error code on failure.
 */
int wav_read_header(FILE *file, WavInfo *info);

/**
 * Reads interleaved WAV samples and converts them to mono signed 16-bit PCM.
 * @param file The WAV file stream.
 * @param info The parsed WAV metadata.
 * @param samples The destination sample buffer.
 * @param max_samples The maximum number of mono samples to read.
 * @param remaining_data_size The remaining WAV data byte count.
 * @return The number of mono samples read.
 */
int wav_read_samples(
    FILE *file,
    const WavInfo *info,
    int16_t *samples,
    size_t max_samples,
    uint32_t *remaining_data_size
);
