#!/usr/bin/env python3
"""Compare the float pipeline with a deterministic Q15 radix-2 model.

This model quantifies algorithmic fixed-point error before hardware is
available. It is not a substitute for the target CMSIS-DSP measurement.
"""

from __future__ import annotations

import argparse
import math

import numpy as np

FFT_SIZE = 1024
SAMPLE_RATE_HZ = 100_000


def saturate_q15(value: int) -> int:
    return max(-32768, min(32767, value))


def multiply_q15(left: int, right: int) -> int:
    return saturate_q15((left * right) >> 15)


def bit_reverse(real: list[int], imag: list[int]) -> None:
    target = 0
    for source in range(1, FFT_SIZE):
        bit = FFT_SIZE >> 1
        while target & bit:
            target ^= bit
            bit >>= 1
        target ^= bit
        if source < target:
            real[source], real[target] = real[target], real[source]
            imag[source], imag[target] = imag[target], imag[source]


def q15_fft(samples: list[int]) -> tuple[list[float], float]:
    window = [round((0.5 - 0.5 * math.cos(2.0 * math.pi * index /
                                         (FFT_SIZE - 1))) * 32767.0)
              for index in range(FFT_SIZE)]
    real = [multiply_q15(sample, window[index])
            for index, sample in enumerate(samples)]
    imag = [0] * FFT_SIZE
    bit_reverse(real, imag)
    length = 2
    while length <= FFT_SIZE:
        half = length // 2
        for base in range(0, FFT_SIZE, length):
            for offset in range(half):
                angle = -2.0 * math.pi * offset / length
                wr = round(math.cos(angle) * 32767.0)
                wi = round(math.sin(angle) * 32767.0)
                odd = base + offset + half
                even = base + offset
                tr = multiply_q15(wr, real[odd]) - multiply_q15(wi, imag[odd])
                ti = multiply_q15(wr, imag[odd]) + multiply_q15(wi, real[odd])
                even_real = real[even]
                even_imag = imag[even]
                # One-bit scaling per stage models a length-N Q15 FFT.
                real[even] = saturate_q15((even_real + tr) >> 1)
                imag[even] = saturate_q15((even_imag + ti) >> 1)
                real[odd] = saturate_q15((even_real - tr) >> 1)
                imag[odd] = saturate_q15((even_imag - ti) >> 1)
        length *= 2
    magnitudes = [4.0 * math.hypot(real[index], imag[index]) / 32768.0
                  for index in range(FFT_SIZE // 2)]
    rms = (math.sqrt(sum(sample * sample for sample in samples) / FFT_SIZE) /
           32768.0)
    return magnitudes, rms


def interpolated_frequency(magnitudes: list[float]) -> float:
    center = max(range(1, len(magnitudes)), key=magnitudes.__getitem__)
    delta = 0.0
    if center + 1 < len(magnitudes):
        left, middle, right = magnitudes[center - 1:center + 2]
        denominator = left - 2.0 * middle + right
        if abs(denominator) > 1.0e-15:
            delta = max(-0.5, min(0.5,
                                  0.5 * (left - right) / denominator))
    return (center + delta) * SAMPLE_RATE_HZ / FFT_SIZE


def compare(frequency_hz: float, amplitude: float) -> tuple[float, ...]:
    indices = np.arange(FFT_SIZE)
    analog = amplitude * np.sin(2.0 * np.pi * frequency_hz * indices /
                                SAMPLE_RATE_HZ) + 0.17
    adc = np.clip(np.rint(2048.0 + analog * 1500.0), 0, 4095).astype(np.int32)
    centered = adc - int(round(float(np.mean(adc))))
    q15_samples = np.clip(centered * 16, -32768, 32767).astype(np.int16)
    normalized = q15_samples.astype(np.float64) / 32768.0

    float_magnitudes = (4.0 / FFT_SIZE) * np.abs(
        np.fft.rfft(normalized * np.hanning(FFT_SIZE)))[:FFT_SIZE // 2]
    float_rms = float(np.sqrt(np.mean(normalized * normalized)))
    q15_magnitudes, q15_rms = q15_fft([int(value) for value in q15_samples])
    float_frequency = interpolated_frequency(float_magnitudes.tolist())
    q15_frequency = interpolated_frequency(q15_magnitudes)
    return (float_frequency, q15_frequency,
            abs(q15_frequency - float_frequency),
            float_rms, q15_rms, abs(q15_rms - float_rms),
            max(float_magnitudes), max(q15_magnitudes),
            abs(max(q15_magnitudes) - max(float_magnitudes)))


def report() -> str:
    cases = [(440.0, 0.10), (440.0, 0.50), (440.0, 0.90),
             (976.5625, 0.10), (976.5625, 0.50), (976.5625, 0.90),
             (1000.0, 0.10), (1000.0, 0.50), (1000.0, 0.90),
             (4000.0, 0.10), (4000.0, 0.50), (4000.0, 0.90)]
    lines = [
        "| Input Hz | Amplitude | Float Hz | Q15-model Hz | abs df Hz | "
        "Float RMS | Q15 RMS | abs dRMS | abs dPeak |",
        "|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for frequency, amplitude in cases:
        values = compare(frequency, amplitude)
        lines.append(
            f"| {frequency:.4f} | {amplitude:.2f} | {values[0]:.4f} | "
            f"{values[1]:.4f} | {values[2]:.4f} | {values[3]:.6f} | "
            f"{values[4]:.6f} | {values[5]:.6f} | {values[8]:.6f} |"
        )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="fail if the model exceeds pre-hardware limits")
    args = parser.parse_args()
    text = report()
    print(text)
    if args.check:
        for frequency in (440.0, 976.5625, 1000.0, 4000.0):
            for amplitude in (0.10, 0.50, 0.90):
                values = compare(frequency, amplitude)
                if values[2] > 1.5 or values[5] > 0.0001:
                    return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
