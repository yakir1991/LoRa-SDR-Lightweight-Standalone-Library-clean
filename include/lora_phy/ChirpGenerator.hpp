// Copyright (c) 2016-2016 Lime Microsystems
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <complex>
#include <cmath>

/*!
 * Generate a chirp into a caller supplied buffer.  `samps` must reference at
 * least `NN` elements; the function writes the generated complex samples but
 * never allocates or frees memory.  All buffers remain owned by the caller.
 *
 * \param [out] samps pointer to the output samples
 * \param N samples per chirp sans the oversampling
 * \param osr oversampling ratio (1 = base rate)
 * \param NN the number of samples to generate
 * \param f0 the phase offset/transmit symbol
 * \param down true for downchirp, false for up
 * \param ampl the chrip amplitude
 * \param [inout] phaseAccum running phase accumulator value
 * \return the number of samples generated
 */
template <typename Type>
int genChirp(std::complex<Type> *samps, int N, int osr, int NN, Type f0, bool down,
             const Type ampl, Type &phaseAccum, Type bw_scale = Type(1))
{
    const Type fMin = -M_PI * bw_scale / osr;
    const Type fMax = M_PI * bw_scale / osr;
    const Type fStep = (2 * M_PI * bw_scale) / (N * osr * osr);
    float f = fMin + f0;
    int i;
    if (down) {
        for (i = 0; i < NN; i++) {
            f += fStep;
            if (f > fMax) f -= (fMax - fMin);
            phaseAccum -= f;
            samps[i] = std::polar(ampl, phaseAccum);
        }
    }
    else {
        for (i = 0; i < NN; i++) {
            f += fStep;
            if (f > fMax) f -= (fMax - fMin);
            phaseAccum += f;
            samps[i] = std::polar(ampl, phaseAccum);
        }
    }
    phaseAccum -= floor(phaseAccum / (2 * M_PI)) * 2 * M_PI;
    return i;
}
