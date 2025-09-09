// Copyright (c) 2016-2016 Lime Microsystems
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <complex>
#include <lora_phy/kissfft.hh>

/**
 * Lightweight FFT based detector.  The caller supplies the FFT input/output
 * buffers and the kissfft instance; the class does not allocate or free memory
 * and merely reads or writes to the provided arrays for the duration of the
 * call.
 */

template <typename Type>
class LoRaDetector
{
public:
    LoRaDetector(const size_t N,
        std::complex<Type>* fft_in,
        std::complex<Type>* fft_out,
        kissfft<Type>& fft):
        N(N),
        fft_in(fft_in),
        fft_out(fft_out),
        _fft(fft)
    {
        _powerScale = 20*std::log10(N);
    }

    //! feed simply sets an input sample
    void feed(const size_t i, const std::complex<Type> &samp)
    {
        fft_in[i] = samp;
    }

    //! calculates argmax(abs(fft(input)))
    size_t detect(Type &power, Type &powerAvg, Type &fIndex, std::complex<Type> *fftOutput = nullptr)
    {
        if (fftOutput == nullptr) fftOutput = fft_out;
        _fft.transform(fft_in, fftOutput);
        size_t maxIndex = 0;
        Type maxValue = 0;
        double total = 0;
        for (size_t i = 0; i < N; i++)
        {
            auto bin = fftOutput[i];
            auto re = bin.real();
            auto im = bin.imag();
            auto mag2 = re*re + im*im;
            total += mag2;
            if (mag2 > maxValue)
            {
                maxIndex = i;
                maxValue = mag2;
            }
        }

        const auto noise = std::sqrt(Type(total - maxValue));
        const auto fundamental = std::sqrt(maxValue);

        powerAvg = 20*std::log10(noise) - _powerScale;
        power = 20*std::log10(fundamental) - _powerScale;

        auto left = std::abs(fftOutput[maxIndex > 0?maxIndex-1:N-1]);
        auto right = std::abs(fftOutput[maxIndex < N-1?maxIndex+1:0]);

        const auto demon = (2.0 * fundamental) - right - left;
        if (demon == 0.0) fIndex = 0.0; //check for divide by 0
        else fIndex = 0.5 * (right - left) / demon;

        return maxIndex;
    }

private:
    const size_t N;
    Type _powerScale;
    std::complex<Type>* fft_in;
    std::complex<Type>* fft_out;
    kissfft<Type>& _fft;
};
