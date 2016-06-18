/*****************************************************************************

YASK: Yet Another Stencil Kernel
Copyright (c) 2014-2016, Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

*****************************************************************************/

// This file defines a union to use for folded vectors of floats or doubles.

#ifndef _RealV_H
#define _RealV_H

// Control assert() by turning on with DEBUG instead of turning off with
// NDEBUG. This makes it off by default.
#ifndef DEBUG
#define NDEBUG
#endif

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef WIN32
#include <unistd.h>
#include <stdint.h>
#include <immintrin.h>
#endif

#include <cstdint>
#include <iostream>
#include <string>
#include <stdexcept>

// safe integer divide and mod.
#include "idiv.hpp"

// macros for 1D<->nD transforms.
#include "layout_macros.hpp"

using namespace std;

// values for 32-bit, single-precision reals.
#if REAL_BYTES == 4
typedef float Real;
#define CTRL_INT ::uint32_t
#define CTRL_IDX_MASK 0xf
#define CTRL_SEL_BIT 0x10
#define MMASK __mmask16
#ifdef USE_INTRIN256
#define VEC_ELEMS 8
#define INAME(op) _mm256_ ## op ## _ps
#define INAMEI(op) _mm256_ ## op ## _epi32
#define IMEM_TYPE float
#elif defined(USE_INTRIN512)
#define VEC_ELEMS 16
#define INAME(op) _mm512_ ## op ## _ps
#define INAMEI(op) _mm512_ ## op ## _epi32
#define IMEM_TYPE void
#endif

// values for 64-bit, double-precision reals.
#elif REAL_BYTES == 8
typedef double Real;
#define CTRL_INT ::uint64_t
#define CTRL_IDX_MASK 0x7
#define CTRL_SEL_BIT 0x8
#define MMASK __mmask8
#ifdef USE_INTRIN256
#define VEC_ELEMS 4
#define INAME(op) _mm256_ ## op ## _pd
#define INAMEI(op) _mm256_ ## op ## _epi64
#define IMEM_TYPE double
#elif defined(USE_INTRIN512)
#define VEC_ELEMS 8
#define INAME(op) _mm512_ ## op ## _pd
#define INAMEI(op) _mm512_ ## op ## _epi64
#define IMEM_TYPE void
#endif

#else
#error "REAL_BYTES not set to 4 or 8"
#endif

// Type to use for indexing grids and realvs.
// Must be signed to allow negative indices in halos.
typedef int64_t idx_t;

// Vector sizes.
// This are defaults to override those generated by foldBuilder
// in stencil_macros.hpp.
#ifndef VLEN_T
#define VLEN_T (1)
#endif
#ifndef VLEN_N
#define VLEN_N (1)
#endif
#ifndef VLEN_X
#define VLEN_X (1)
#endif
#ifndef VLEN_Y
#define VLEN_Y (1)
#endif
#ifndef VLEN_Z
#define VLEN_Z (1)
#endif
#ifndef VLEN
#define VLEN ((VLEN_T) * (VLEN_N) * (VLEN_X) * (VLEN_Y) * (VLEN_Z))
#endif

#if VLEN_T != 1
#error "Vector folding in time dimension not currently supported."
#endif

// Emulate instrinsics for unsupported VLEN.
// Only 256 and 512-bit vectors supported.
// VLEN == 1 also supported as scalar.
#if VLEN == 1
#define NO_INTRINSICS
// note: no warning here because intrinsics aren't wanted in this case.

#elif !defined(VEC_ELEMS)
#warning "Emulating intrinsics because HW vector length not defined; set USE_INTRIN256 or USE_INTRIN512"
#define NO_INTRINSICS

#elif VLEN != VEC_ELEMS
#warning "Emulating intrinsics because VLEN != HW vector length"
#define NO_INTRINSICS
#endif

// Macro for looping through an aligned realv.
#if defined(DEBUG) || (VLEN==1) || !defined(__INTEL_COMPILER) 
#define RealV_LOOP(i)                            \
    for (int i=0; i<VLEN; i++)
#define RealV_LOOP_UNALIGNED(i)                  \
    for (int i=0; i<VLEN; i++)
#else
#define RealV_LOOP(i)                            \
    _Pragma("vector aligned") _Pragma("vector always") _Pragma("simd")  \
    for (int i=0; i<VLEN; i++)
#define RealV_LOOP_UNALIGNED(i)                  \
    _Pragma("vector always") _Pragma("simd")     \
    for (int i=0; i<VLEN; i++)
#endif

// conditional inlining
#ifdef DEBUG
#define ALWAYS_INLINE inline
#else
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#endif

    
// The following must be an aggregate type to allow aggregate initialization,
// so no user-provided ctors, copy operator, virtual member functions, etc.
union realv_data {
    Real r[VLEN];
    CTRL_INT ci[VLEN];

#ifndef NO_INTRINSICS
    
    // 32-bit integer vector overlay.
#if defined(USE_INTRIN256)
    __m256i mi;
#elif defined(USE_INTRIN512)
    __m512i mi;
#endif

    // real vector.
#if REAL_BYTES == 4 && defined(USE_INTRIN256)
    __m256  mr;
#elif REAL_BYTES == 4 && defined(USE_INTRIN512)
    __m512  mr;
#elif REAL_BYTES == 8 && defined(USE_INTRIN256)
    __m256d mr;
#elif REAL_BYTES == 8 && defined(USE_INTRIN512)
    __m512d mr;
#endif
#endif
};
  

// Type for a vector block.
struct realv {

    // union of data types.
    realv_data u;

    // default ctor does not init data!
    ALWAYS_INLINE realv() {}

    // copy vector.
    ALWAYS_INLINE realv(const realv& val) {
        operator=(val);
    }
    ALWAYS_INLINE realv(const realv_data& val) {
        operator=(val);
    }
    
    // broadcast scalar.
    ALWAYS_INLINE realv(float val) {
        operator=(val);
    }
    ALWAYS_INLINE realv(double val) {
        operator=(val);
    }
    ALWAYS_INLINE realv(int val) {
        operator=(val);
    }
    ALWAYS_INLINE realv(long val) {
        operator=(val);
    }

    // copy whole vector.
    ALWAYS_INLINE realv& operator=(const realv& rhs) {
#ifdef NO_INTRINSICS
        RealV_LOOP(i) u.r[i] = rhs[i];
#else
        u.mr = rhs.u.mr;
#endif
        return *this;
    }
    ALWAYS_INLINE realv& operator=(const realv_data& rhs) {
        u = rhs;
        return *this;
    }

    // assignment: single value broadcast.
    ALWAYS_INLINE void operator=(double val) {
#ifdef NO_INTRINSICS
        RealV_LOOP(i) u.r[i] = Real(val);
#else
        u.mr = INAME(set1)(Real(val));
#endif
    }
    ALWAYS_INLINE void operator=(float val) {
#ifdef NO_INTRINSICS
        RealV_LOOP(i) u.r[i] = Real(val);
#else
        u.mr = INAME(set1)(Real(val));
#endif
    }

    // broadcast with conversions.
    ALWAYS_INLINE void operator=(int val) {
        operator=(Real(val));
    }
    ALWAYS_INLINE void operator=(long val) {
        operator=(Real(val));
    }

    
    // access a Real linearly.
    ALWAYS_INLINE Real& operator[](idx_t l) {
        assert(l >= 0);
        assert(l < VLEN);
        return u.r[l];
    }
    ALWAYS_INLINE const Real& operator[](idx_t l) const {
        assert(l >= 0);
        assert(l < VLEN);
        return u.r[l];
    }

    // access a Real by n,x,y,z vector-block indices.
    ALWAYS_INLINE const Real& operator()(idx_t n, idx_t i, idx_t j, idx_t k) const {
        assert(n >= 0);
        assert(n < VLEN_N);
        assert(i >= 0);
        assert(i < VLEN_X);
        assert(j >= 0);
        assert(j < VLEN_Y);
        assert(k >= 0);
        assert(k < VLEN_Z);

#if VLEN_FIRST_DIM_IS_UNIT_STRIDE

        // n dim is unit stride, followed by x, y, z.
        idx_t l = LAYOUT_4321(n, i, j, k, VLEN_N, VLEN_X, VLEN_Y, VLEN_Z);
#else

        // z dim is unit stride, followed by y, x, n.
        idx_t l = LAYOUT_1234(n, i, j, k, VLEN_N, VLEN_X, VLEN_Y, VLEN_Z);
#endif
        
        return u.r[l];
    }
    ALWAYS_INLINE Real& operator()(idx_t n, idx_t i, idx_t j, idx_t k) {
        const realv* ct = const_cast<const realv*>(this);
        const Real& cr = (*ct)(n, i, j, k);
        return const_cast<Real&>(cr);
    }

    // unary negate.
    ALWAYS_INLINE realv operator-() const {
        realv res;
#ifdef NO_INTRINSICS
        RealV_LOOP(i) res[i] = -u.r[i];
#else
        // subtract from zero.
        res.u.mr = INAME(sub)(INAME(setzero)(), u.mr);
#endif
        return res;
    }

    // add.
    ALWAYS_INLINE realv operator+(realv rhs) const {
        realv res;
#ifdef NO_INTRINSICS
        RealV_LOOP(i) res[i] = u.r[i] + rhs[i];
#else
        res.u.mr = INAME(add)(u.mr, rhs.u.mr);
#endif
        return res;
    }
    ALWAYS_INLINE realv operator+(Real rhs) const {
        realv rn;
        rn = rhs;               // broadcast.
        return (*this) + rn;
    }

    // sub.
    ALWAYS_INLINE realv operator-(realv rhs) const {
        realv res;
#ifdef NO_INTRINSICS
        RealV_LOOP(i) res[i] = u.r[i] - rhs[i];
#else
        res.u.mr = INAME(sub)(u.mr, rhs.u.mr);
#endif
        return res;
    }
    ALWAYS_INLINE realv operator-(Real rhs) const {
        realv rn;
        rn = rhs;               // broadcast.
        return (*this) - rn;
    }

    // mul.
    ALWAYS_INLINE realv operator*(realv rhs) const {
        realv res;
#ifdef NO_INTRINSICS
        RealV_LOOP(i) res[i] = u.r[i] * rhs[i];
#else
        res.u.mr = INAME(mul)(u.mr, rhs.u.mr);
#endif
        return res;
    }
    ALWAYS_INLINE realv operator*(Real rhs) const {
        realv rn;
        rn = rhs;               // broadcast.
        return (*this) * rn;
    }
    
    // div.
    ALWAYS_INLINE realv operator/(realv rhs) const {
        realv res, rcp;
#ifdef NO_INTRINSICS
        RealV_LOOP(i) res[i] = u.r[i] / rhs[i];
#elif USE_RCP14
        rcp.u.mr = INAME(rcp14)(rhs.u.mr);
        res.u.mr = INAME(mul)(u.mr, rcp.u.mr);
#elif USE_RCP28
        rcp.u.mr = INAME(rcp28)(rhs.u.mr);
        res.u.mr = INAME(mul)(u.mr, rcp.u.mr);
#else
        res.u.mr = INAME(div)(u.mr, rhs.u.mr);
#endif
        return res;
    }
    ALWAYS_INLINE realv operator/(Real rhs) const {
        realv rn;
        rn = rhs;               // broadcast.
        return (*this) / rn;
    }

    // less-than comparator.
    bool operator<(const realv& rhs) const {
        for (int j = 0; j < VLEN; j++) {
            if (u.r[j] < rhs.u.r[j])
                return true;
            else if (u.r[j] > rhs.u.r[j])
                return false;
        }
        return false;
    }

    // greater-than comparator.
    bool operator>(const realv& rhs) const {
        for (int j = 0; j < VLEN; j++) {
            if (u.r[j] > rhs.u.r[j])
                return true;
            else if (u.r[j] < rhs.u.r[j])
                return false;
        }
        return false;
    }
    
    // equal-to comparator for validation.
    bool operator==(const realv& rhs) const {
        for (int j = 0; j < VLEN; j++) {
            if (u.r[j] != rhs.u.r[j])
                return false;
        }
        return true;
    }
    
    // aligned load.
    ALWAYS_INLINE void loadFrom(const realv* __restrict__ from) {
#if defined(NO_INTRINSICS) || defined(NO_LOAD_INTRINSICS)
        RealV_LOOP(i) u.r[i] = (*from)[i];
#else
        u.mr = INAME(load)((IMEM_TYPE const*)from);
#endif
    }

    // unaligned load.
    ALWAYS_INLINE void loadUnalignedFrom(const realv* __restrict__ from) {
#if defined(NO_INTRINSICS) || defined(NO_LOAD_INTRINSICS)
        RealV_LOOP_UNALIGNED(i) u.r[i] = (*from)[i];
#else
        u.mr = INAME(loadu)((IMEM_TYPE const*)from);
#endif
    }

    // aligned store.
    ALWAYS_INLINE void storeTo(realv* __restrict__ to) const {

        // Using an explicit loop here instead of a store intrinsic may
        // allow the compiler to do more optimizations.  This is true
        // for icc 2016 r2--it may change for later versions. Try
        // defining and not defining NO_STORE_INTRINSICS and comparing
        // the sizes of the stencil computation loop and the overall
        // performance.
#if defined(NO_INTRINSICS) || defined(NO_STORE_INTRINSICS)
#if defined(__INTEL_COMPILER) && (VLEN > 1) && defined(USE_STREAMING_STORE)
        _Pragma("vector nontemporal")
#endif
            RealV_LOOP(i) (*to)[i] = u.r[i];
#elif !defined(USE_STREAMING_STORE)
        INAME(store)((IMEM_TYPE*)to, u.mr);
#elif defined(ARCH_KNC)
        INAME(storenrngo)((IMEM_TYPE*)to, u.mr);
#else
        INAME(stream)((IMEM_TYPE*)to, u.mr);
#endif
    }

    // Output.
    void print_ctrls(ostream& os, bool doEnd=true) const {
        for (int j = 0; j < VLEN; j++) {
            if (j) os << ", ";
            os << "[" << j << "]=" << u.ci[j];
        }
        if (doEnd)
            os << endl;
    }

    void print_reals(ostream& os, bool doEnd=true) const {
        for (int j = 0; j < VLEN; j++) {
            if (j) os << ", ";
            os << "[" << j << "]=" << u.r[j];
        }
        if (doEnd)
            os << endl;
    }

}; // realv.

// Output using '<<'.
inline ostream& operator<<(ostream& os, const realv& rn) {
    rn.print_reals(os, false);
    return os;
}

// More operator overloading.
ALWAYS_INLINE realv operator+(Real lhs, const realv& rhs) {
    return realv(lhs) + rhs;
}
ALWAYS_INLINE realv operator-(Real lhs, const realv& rhs) {
    return realv(lhs) - rhs;
}
ALWAYS_INLINE realv operator*(Real lhs, const realv& rhs) {
    return realv(lhs) * rhs;
}
ALWAYS_INLINE realv operator/(Real lhs, const realv& rhs) {
    return realv(lhs) / rhs;
}

// wrappers around some intrinsics w/non-intrinsic equivalents.
// TODO: make these methods in the realv union.

// Get consecutive elements from two vectors.
// Concat a and b, shift right by count elements, keep rightmost elements.
// Thus, shift of 0 returns b; shift of VLEN returns a.
// Must be a template because count has to be known at compile-time.
template<int count>
ALWAYS_INLINE void realv_align(realv& res, const realv& a, const realv& b) {
#ifdef TRACE_INTRINSICS
    cout << "realv_align w/count=" << count << ":" << endl;
    cout << " a: ";
    a.print_reals(cout);
    cout << " b: ";
    b.print_reals(cout);
#endif

#if defined(NO_INTRINSICS)
    // must make temp copies in case &res == &a or &b.
    realv tmpa = a, tmpb = b;
    for (int i = 0; i < VLEN-count; i++)
        res.u.r[i] = tmpb.u.r[i + count];
    for (int i = VLEN-count; i < VLEN; i++)
        res.u.r[i] = tmpa.u.r[i + count - VLEN];
    
#elif defined(USE_INTRIN256)
    // Not really an intrinsic, but not element-wise, either.
    // Put the 2 parts in a local array, then extract the desired part
    // using an unaligned load.
    Real r2[VLEN * 2];
    *((realv*)(&r2[0])) = b;
    *((realv*)(&r2[VLEN])) = a;
    res = *((realv*)(&r2[count]));
    
#elif REAL_BYTES == 8 && defined(ARCH_KNC) && defined(USE_INTRIN512)
    // For KNC, for 64-bit align, use the 32-bit op w/2x count.
    res.u.mi = _mm512_alignr_epi32(a.u.mi, b.u.mi, count*2);

#else
    res.u.mi = INAMEI(alignr)(a.u.mi, b.u.mi, count);
#endif

#ifdef TRACE_INTRINSICS
    cout << " res: ";
    res.print_reals(cout);
#endif
}

// Get consecutive elements from two vectors w/masking.
// Concat a and b, shift right by count elements, keep rightmost elements.
// Elements in res corresponding to 0 bits in k1 are unchanged.
template<int count>
ALWAYS_INLINE void realv_align_masked(realv& res, const realv& a, const realv& b,
                                      unsigned int k1) {
#ifdef TRACE_INTRINSICS
    cout << "realv_align w/count=" << count << " w/mask:" << endl;
    cout << " a: ";
    a.print_reals(cout);
    cout << " b: ";
    b.print_reals(cout);
    cout << " res(before): ";
    res.print_reals(cout);
    cout << " mask: 0x" << hex << k1 << endl;
#endif

#ifdef NO_INTRINSICS
    // must make temp copies in case &res == &a or &b.
    realv tmpa = a, tmpb = b;
    for (int i = 0; i < VLEN-count; i++)
        if ((k1 >> i) & 1)
            res.u.r[i] = tmpb.u.r[i + count];
    for (int i = VLEN-count; i < VLEN; i++)
        if ((k1 >> i) & 1)
            res.u.r[i] = tmpa.u.r[i + count - VLEN];
#else
    res.u.mi = INAMEI(mask_alignr)(res.u.mi, MMASK(k1), a.u.mi, b.u.mi, count);
#endif

#ifdef TRACE_INTRINSICS
    cout << " res(after): ";
    res.print_reals(cout);
#endif
}

// Rearrange elements in a vector.
ALWAYS_INLINE void realv_permute(realv& res, const realv& ctrl, const realv& a) {

#ifdef TRACE_INTRINSICS
    cout << "realv_permute:" << endl;
    cout << " ctrl: ";
    ctrl.print_ctrls(cout);
    cout << " a: ";
    a.print_reals(cout);
#endif

#if defined(NO_INTRINSICS) || !defined(USE_INTRIN512)
    // must make a temp copy in case &res == &a.
    realv tmp = a;
    for (int i = 0; i < VLEN; i++)
        res.u.r[i] = tmp.u.r[ctrl.u.ci[i]];
#else
    res.u.mi = INAMEI(permutexvar)(ctrl.u.mi, a.u.mi);
#endif

#ifdef TRACE_INTRINSICS
    cout << " res: ";
    res.print_reals(cout);
#endif
}

// Rearrange elements in a vector w/masking.
// Elements in res corresponding to 0 bits in k1 are unchanged.
ALWAYS_INLINE void realv_permute_masked(realv& res, const realv& ctrl, const realv& a,
                                        unsigned int k1) {
#ifdef TRACE_INTRINSICS
    cout << "realv_permute w/mask:" << endl;
    cout << " ctrl: ";
    ctrl.print_ctrls(cout);
    cout << " a: ";
    a.print_reals(cout);
    cout << " mask: 0x" << hex << k1 << endl;
    cout << " res(before): ";
    res.print_reals(cout);
#endif

#if defined(NO_INTRINSICS) || !defined(USE_INTRIN512)
    // must make a temp copy in case &res == &a.
    realv tmp = a;
    for (int i = 0; i < VLEN; i++) {
        if ((k1 >> i) & 1)
            res.u.r[i] = tmp.u.r[ctrl.u.ci[i]];
    }
#else
    res.u.mi = INAMEI(mask_permutexvar)(res.u.mi, MMASK(k1), ctrl.u.mi, a.u.mi);
#endif

#ifdef TRACE_INTRINSICS
    cout << " res(after): ";
    res.print_reals(cout);
#endif
}

// Rearrange elements in 2 vectors.
// (The masking versions of these instrs do not preserve the source,
// so we don't have a masking version of this function.)
ALWAYS_INLINE void realv_permute2(realv& res, const realv& ctrl,
                                  const realv& a, const realv& b) {
#ifdef TRACE_INTRINSICS
    cout << "realv_permute2:" << endl;
    cout << " ctrl: ";
    ctrl.print_ctrls(cout);
    cout << " a: ";
    a.print_reals(cout);
    cout << " b: ";
    b.print_reals(cout);
#endif

#if defined(NO_INTRINSICS) || !defined(USE_INTRIN512)
    // must make temp copies in case &res == &a or &b.
    realv tmpa = a, tmpb = b;
    for (int i = 0; i < VLEN; i++) {
        int sel = ctrl.u.ci[i] & CTRL_SEL_BIT; // 0 => a, 1 => b.
        int idx = ctrl.u.ci[i] & CTRL_IDX_MASK; // index.
        res.u.r[i] = sel ? tmpb.u.r[idx] : tmpa.u.r[idx];
    }

#elif defined(ARCH_KNC)
    cerr << "error: 2-input permute not supported on KNC" << endl;
    exit(1);
#else
    res.u.mi = INAMEI(permutex2var)(a.u.mi, ctrl.u.mi, b.u.mi);
#endif

#ifdef TRACE_INTRINSICS
    cout << " res: ";
    res.print_reals(cout);
#endif
}

// max abs difference in validation.
#ifndef EPSILON
#define EPSILON (1e-3)
#endif

// check whether two reals are close enough.
template<typename T>
inline bool within_tolerance(T val, T ref, T epsilon) {
    bool ok;
    double adiff = fabs(val - ref);
    if (fabs(ref) > 1.0)
        epsilon = fabs(ref * epsilon);
    ok = adiff < epsilon;
#ifdef DEBUG_TOLERANCE
    if (!ok)
        cerr << "outside tolerance of " << epsilon << ": " << val << " != " << ref <<
            " because " << adiff << " >= " << epsilon << endl;
#endif
    return ok;
}

// Compare two realv's.
inline bool within_tolerance(const realv& val, const realv& ref,
                             const realv& epsilon) {
        for (int j = 0; j < VLEN; j++) {
            if (!within_tolerance(val.u.r[j], ref.u.r[j], epsilon.u.r[j]))
                return false;
        }
        return true;
}

// aligned declarations.
#ifdef __INTEL_COMPILER
#define ALIGNED_RealV __declspec(align(sizeof(realv))) realv
#else
#define ALIGNED_RealV realv __attribute__((aligned(sizeof(realv)))) 
#endif

// zero a VLEN-sized array.
#define ZERO_VEC(v) do {                        \
        RealV_LOOP(i)                            \
            v[i] = (Real)0.0;                   \
    } while(0)

// declare and zero a VLEN-sized array.
#define MAKE_VEC(v)                             \
    ALIGNED_RealV v(0.0)


#endif
