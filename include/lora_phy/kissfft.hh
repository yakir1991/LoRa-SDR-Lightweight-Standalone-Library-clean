// Derived from the KISS FFT library; see THIRD_PARTY.md for the BSD-3-Clause license details.
// SPDX-License-Identifier: BSD-3-Clause
#ifndef KISSFFT_CLASS_HH
#define KISSFFT_CLASS_HH
#include <complex>
#include <cstddef>
#include <cmath>

// The implementation below is a minimal subset of KISS FFT adapted for use in
// fixed-size workspaces. All memory required for the transform is provided by a
// plan structure containing statically sized arrays.  Callers are responsible
// for allocating the plan and the input/output buffers; no dynamic allocations
// are performed and the library never frees caller owned memory.

namespace kissfft_utils {

// Traits helper used for generating twiddle factors.
template <typename T_scalar>
struct traits
{
    using scalar_type = T_scalar;
    using cpx_type = std::complex<scalar_type>;

    static void fill_twiddles(cpx_type* dst, int nfft, bool inverse)
    {
        scalar_type phinc = (inverse?2:-2)*acos((scalar_type)-1)/nfft;
        for (int i = 0; i < nfft; ++i)
            dst[i] = std::exp(cpx_type(0, i*phinc));
    }
};

// Maximum supported FFT length and factorization depth. These values cover the
// LoRa demodulator use cases (N <= 4096).
constexpr std::size_t KISSFFT_MAX_N = 4096;
constexpr std::size_t KISSFFT_MAX_FACTORS = 32;
constexpr std::size_t KISSFFT_MAX_FFT_RADIX = 32;

} // namespace kissfft_utils

// Plan structure holding all preallocated buffers required by the FFT. The
// arrays are statically sized to avoid any dynamic memory allocation at run
// time. A plan must be initialised with `kissfft::init` before use.
template <typename T_scalar>
struct kissfft_plan
{
    using scalar_type = T_scalar;
    using cpx_type = std::complex<scalar_type>;

    int nfft{};                           // FFT length
    bool inverse{};                       // true for inverse transform
    int stages{};                         // number of factorization stages
    cpx_type twiddles[kissfft_utils::KISSFFT_MAX_N];
    int stageRadix[kissfft_utils::KISSFFT_MAX_FACTORS];
    int stageRemainder[kissfft_utils::KISSFFT_MAX_FACTORS];
};

template <typename T_Scalar,
         typename T_traits = kissfft_utils::traits<T_Scalar>
         >
class kissfft
{
public:
    using traits_type = T_traits;
    using scalar_type = typename traits_type::scalar_type;
    using cpx_type = std::complex<scalar_type>;
    using plan_type = kissfft_plan<T_Scalar>;

    explicit kissfft(plan_type& plan)
        : _p(plan) {}

    static void init(plan_type& plan, int nfft, bool inverse,
                     const traits_type& traits = traits_type())
    {
        plan.nfft = nfft;
        plan.inverse = inverse;

        // Generate twiddle factors
        traits.fill_twiddles(plan.twiddles, nfft, inverse);

        // Factorize nfft and store in plan
        int n = nfft;
        int p = 4;
        plan.stages = 0;
        do {
            while (n % p) {
                switch (p) {
                    case 4: p = 2; break;
                    case 2: p = 3; break;
                    default: p += 2; break;
                }
                if (p*p > n) p = n; // no more factors
            }
            n /= p;
            plan.stageRadix[plan.stages] = p;
            plan.stageRemainder[plan.stages] = n;
            ++plan.stages;
        } while (n > 1);
    }

    void transform(const cpx_type* src, cpx_type* dst) const
    {
        kf_work(0, dst, src, 1, 1);
    }

private:
    void kf_work(int stage, cpx_type* Fout, const cpx_type* f,
                 size_t fstride, size_t in_stride) const
    {
        int p = _p.stageRadix[stage];
        int m = _p.stageRemainder[stage];
        cpx_type* Fout_beg = Fout;
        cpx_type* Fout_end = Fout + p*m;

        if (m == 1)
        {
            do {
                *Fout = *f;
                f += fstride*in_stride;
            } while (++Fout != Fout_end);
        }
        else
        {
            do {
                // recursive call:
                // DFT of size m*p performed by doing
                // p instances of smaller DFTs of size m,
                // each one takes a decimated version of the input
                kf_work(stage+1, Fout, f, fstride*p, in_stride);
                f += fstride*in_stride;
            } while ((Fout += m) != Fout_end);
        }

        Fout = Fout_beg;

        // recombine the p smaller DFTs
        switch (p) {
            case 2: kf_bfly2(Fout, fstride, m); break;
            case 3: kf_bfly3(Fout, fstride, m); break;
            case 4: kf_bfly4(Fout, fstride, m); break;
            case 5: kf_bfly5(Fout, fstride, m); break;
            default: kf_bfly_generic(Fout, fstride, m, p); break;
        }
    }

    // these were #define macros in the original kiss_fft
    static void C_ADD(cpx_type& c, const cpx_type& a, const cpx_type& b) { c = a + b; }
    static void C_MUL(cpx_type& c, const cpx_type& a, const cpx_type& b) { c = a * b; }
    static void C_SUB(cpx_type& c, const cpx_type& a, const cpx_type& b) { c = a - b; }
    static void C_ADDTO(cpx_type& c, const cpx_type& a) { c += a; }
    static void C_FIXDIV(cpx_type&, int) {} // NO-OP for float types
    static scalar_type S_MUL(const scalar_type& a, const scalar_type& b) { return a*b; }
    static scalar_type HALF_OF(const scalar_type& a) { return a*scalar_type(.5); }
    static void C_MULBYSCALAR(cpx_type& c, const scalar_type& a) { c *= a; }

    void kf_bfly2(cpx_type* Fout, const size_t fstride, int m) const
    {
        for (int k = 0; k < m; ++k) {
            cpx_type t = Fout[m+k] * _p.twiddles[k*fstride];
            Fout[m+k] = Fout[k] - t;
            Fout[k] += t;
        }
    }

    void kf_bfly4(cpx_type* Fout, const size_t fstride, const size_t m) const
    {
        cpx_type scratch[7];
        int negative_if_inverse = _p.inverse * -2 + 1;
        for (size_t k = 0; k < m; ++k) {
            scratch[0] = Fout[k+m] * _p.twiddles[k*fstride];
            scratch[1] = Fout[k+2*m] * _p.twiddles[k*fstride*2];
            scratch[2] = Fout[k+3*m] * _p.twiddles[k*fstride*3];
            scratch[5] = Fout[k] - scratch[1];

            Fout[k] += scratch[1];
            scratch[3] = scratch[0] + scratch[2];
            scratch[4] = scratch[0] - scratch[2];
            scratch[4] = cpx_type(scratch[4].imag()*negative_if_inverse,
                                 -scratch[4].real()*negative_if_inverse);

            Fout[k+2*m] = Fout[k] - scratch[3];
            Fout[k] += scratch[3];
            Fout[k+m] = scratch[5] + scratch[4];
            Fout[k+3*m] = scratch[5] - scratch[4];
        }
    }

    void kf_bfly3(cpx_type* Fout, const size_t fstride, const size_t m) const
    {
        size_t k = m;
        const size_t m2 = 2*m;
        cpx_type *tw1, *tw2;
        cpx_type scratch[5];
        cpx_type epi3;
        epi3 = _p.twiddles[fstride*m];

        tw1 = tw2 = &_p.twiddles[0];

        do {
            C_FIXDIV(*Fout,3); C_FIXDIV(Fout[m],3); C_FIXDIV(Fout[m2],3);

            C_MUL(scratch[1], Fout[m], *tw1);
            C_MUL(scratch[2], Fout[m2], *tw2);

            C_ADD(scratch[3], scratch[1], scratch[2]);
            C_SUB(scratch[0], scratch[1], scratch[2]);
            tw1 += fstride;
            tw2 += fstride*2;

            Fout[m] = cpx_type(Fout->real() - HALF_OF(scratch[3].real()),
                               Fout->imag() - HALF_OF(scratch[3].imag()));

            C_MULBYSCALAR(scratch[0], epi3.imag());

            C_ADDTO(*Fout, scratch[3]);

            Fout[m2] = cpx_type(Fout[m].real() + scratch[0].imag(),
                                Fout[m].imag() - scratch[0].real());

            C_ADDTO(Fout[m], cpx_type(-scratch[0].imag(), scratch[0].real()));
            ++Fout;
        } while (--k);
    }

    void kf_bfly5(cpx_type* Fout, const size_t fstride, const size_t m) const
    {
        cpx_type *Fout0, *Fout1, *Fout2, *Fout3, *Fout4;
        size_t u;
        cpx_type scratch[13];
        cpx_type* twiddles = &_p.twiddles[0];
        cpx_type *tw;
        cpx_type ya, yb;
        ya = twiddles[fstride*m];
        yb = twiddles[fstride*2*m];

        Fout0 = Fout;
        Fout1 = Fout0 + m;
        Fout2 = Fout0 + 2*m;
        Fout3 = Fout0 + 3*m;
        Fout4 = Fout0 + 4*m;

        tw = twiddles;
        for (u = 0; u < m; ++u) {
            C_FIXDIV(*Fout0,5); C_FIXDIV(*Fout1,5); C_FIXDIV(*Fout2,5);
            C_FIXDIV(*Fout3,5); C_FIXDIV(*Fout4,5);
            scratch[0] = *Fout0;

            C_MUL(scratch[1], *Fout1, tw[u*fstride]);
            C_MUL(scratch[2], *Fout2, tw[2*u*fstride]);
            C_MUL(scratch[3], *Fout3, tw[3*u*fstride]);
            C_MUL(scratch[4], *Fout4, tw[4*u*fstride]);

            C_ADD(scratch[7], scratch[1], scratch[4]);
            C_SUB(scratch[10], scratch[1], scratch[4]);
            C_ADD(scratch[8], scratch[2], scratch[3]);
            C_SUB(scratch[9], scratch[2], scratch[3]);

            C_ADDTO(*Fout0, scratch[7]);
            C_ADDTO(*Fout0, scratch[8]);

            scratch[5] = scratch[0] + cpx_type(
                S_MUL(scratch[7].real(), ya.real()) + S_MUL(scratch[8].real(), yb.real()),
                S_MUL(scratch[7].imag(), ya.real()) + S_MUL(scratch[8].imag(), yb.real()));

            scratch[6] = cpx_type(
                S_MUL(scratch[10].imag(), ya.imag()) + S_MUL(scratch[9].imag(), yb.imag()),
                -S_MUL(scratch[10].real(), ya.imag()) - S_MUL(scratch[9].real(), yb.imag()));

            C_SUB(*Fout1, scratch[5], scratch[6]);
            C_ADD(*Fout4, scratch[5], scratch[6]);

            scratch[11] = scratch[0] + cpx_type(
                S_MUL(scratch[7].real(), yb.real()) + S_MUL(scratch[8].real(), ya.real()),
                S_MUL(scratch[7].imag(), yb.real()) + S_MUL(scratch[8].imag(), ya.real()));

            scratch[12] = cpx_type(
                -S_MUL(scratch[10].imag(), yb.imag()) + S_MUL(scratch[9].imag(), ya.imag()),
                S_MUL(scratch[10].real(), yb.imag()) - S_MUL(scratch[9].real(), ya.imag()));

            C_ADD(*Fout2, scratch[11], scratch[12]);
            C_SUB(*Fout3, scratch[11], scratch[12]);

            ++Fout0; ++Fout1; ++Fout2; ++Fout3; ++Fout4;
        }
    }

    /* perform the butterfly for one stage of a mixed radix FFT */
    void kf_bfly_generic(cpx_type* Fout, const size_t fstride, int m, int p) const
    {
        int u, k, q1, q;
        cpx_type* twiddles = &_p.twiddles[0];
        cpx_type t;
        int Norig = _p.nfft;
        cpx_type scratchbuf[kissfft_utils::KISSFFT_MAX_FFT_RADIX];

        for (u = 0; u < m; ++u) {
            k = u;
            for (q1 = 0; q1 < p; ++q1) {
                scratchbuf[q1] = Fout[k];
                C_FIXDIV(scratchbuf[q1], p);
                k += m;
            }

            k = u;
            for (q1 = 0; q1 < p; ++q1) {
                int twidx = 0;
                Fout[k] = scratchbuf[0];
                for (q = 1; q < p; ++q) {
                    twidx += fstride * k;
                    if (twidx >= Norig) twidx -= Norig;
                    C_MUL(t, scratchbuf[q], twiddles[twidx]);
                    C_ADDTO(Fout[k], t);
                }
                k += m;
            }
        }
    }

    plan_type& _p;
};
#endif
