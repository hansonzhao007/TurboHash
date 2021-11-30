#ifndef KV_UTIL_TRACE_H
#define KV_UTIL_TRACE_H

#include <stdint.h>

#include <thread>
#include <vector>

#include "geninfo.h"
#include "slice.h"
namespace util {
const uint64_t kRAND64_MAX = ((((uint64_t)RAND_MAX) << 31) + ((uint64_t)RAND_MAX));
const double kRAND64_MAX_D = ((double)(kRAND64_MAX));
const uint64_t kRANDOM_RANGE = UINT64_C (2000000000000);

class Trace {
public:
    Trace (int seed) : seed_ (seed), init_ (seed), gi_ (nullptr) {
        if (seed_ == 0) {
            seed_ = random ();
        }
    }

    virtual ~Trace () {
        if (gi_ != nullptr) delete gi_;
    }
    virtual uint64_t Next () = 0;
    void Reset () { seed_ = init_; }
    uint32_t Random () {
        static thread_local const uint32_t M = 2147483647L;  // 2^31-1
        static thread_local const uint64_t A = 16807;        // bits 14, 8, 7, 5, 2, 1, 0
        // We are computing
        //       seed_ = (seed_ * A) % M,    where M = 2^31-1
        //
        // seed_ must not be zero or M, or else all subsequent computed values
        // will be zero or M respectively.  For all other values, seed_ will end
        // up cycling through every number in [1,M-1]
        uint64_t product = seed_ * A;

        // Compute (product % M) using the fact that ((x << 31) % M) == x.
        seed_ = static_cast<uint32_t> ((product >> 31) + (product & M));
        // The first reduction may overflow by 1 bit, so we may need to
        // repeat.  mod == M is not possible; using > allows the faster
        // sign-bit-based test.
        if ((uint32_t)seed_ > M) {
            seed_ -= M;
        }
        return seed_;
    }

    inline uint64_t Random64 () {
        // 62 bit random value;
        const uint64_t rand64 = (((uint64_t)Random ()) << 31) + ((uint64_t)Random ());
        return rand64;
    }

    inline double RandomDouble () {
        // random between 0.0 - 1.0
        const double r = (double)Random64 ();
        const double rd = r / kRAND64_MAX_D;
        return rd;
    }

    int seed_;
    int init_;
    GenInfo* gi_;
};

class TraceSeq : public Trace {
public:
    explicit TraceSeq (uint64_t start_off = 0, uint64_t interval = 1, uint64_t minimum = 0,
                       uint64_t maximum = kRANDOM_RANGE)
        : Trace (0) {
        start_off_ = start_off;
        interval_ = interval;
        min_ = minimum;
        max_ = maximum;
        cur_ = start_off_;
    }
    inline uint64_t Next () override {
        cur_ += interval_;
        if (cur_ >= max_) cur_ = 0;
        return cur_;
    }

private:
    uint64_t start_off_;
    uint64_t interval_;
    uint64_t min_;
    uint64_t max_;
    uint64_t cur_;
};

class TraceUniform : public Trace {
public:
    explicit TraceUniform (int seed, uint64_t minimum = 0, uint64_t maximum = kRANDOM_RANGE)
        : Trace (seed) {
        gi_ = new GenInfo ();
        gi_->gen.uniform.min = minimum;
        gi_->gen.uniform.max = maximum;
        gi_->gen.uniform.interval = (double)(maximum - minimum);
        gi_->type = GEN_UNIFORM;
    }

    ~TraceUniform () {}
    uint64_t Next () override {
        uint64_t off = (uint64_t) (RandomDouble () * gi_->gen.uniform.interval);
        return gi_->gen.uniform.min + off;
    }
};

class TraceExponential : public Trace {
public:
#define FNV_OFFSET_BASIS_64 ((UINT64_C (0xCBF29CE484222325)))
#define FNV_PRIME_64 ((UINT64_C (1099511628211)))
    explicit TraceExponential (int seed, const double percentile = 50, double range = kRANDOM_RANGE)
        : Trace (seed), range_ (range) {
        range = range * 0.15;
        gi_ = new GenInfo ();
        gi_->gen.exponential.gamma = -log (1.0 - (percentile / 100.0)) / range;

        gi_->type = GEN_EXPONENTIAL;
    }
    ~TraceExponential () {}
    uint64_t Next () override {
        uint64_t d = (uint64_t) (-log (RandomDouble ()) / gi_->gen.exponential.gamma) % range_;
        return d;
    }

private:
    uint64_t range_;
};

class TraceNormal : public Trace {
public:
    explicit TraceNormal (int seed, uint64_t minimum = 0, uint64_t maximum = kRANDOM_RANGE)
        : Trace (seed) {
        gi_ = new GenInfo ();
        gi_->gen.normal.min = minimum;
        gi_->gen.normal.max = maximum;
        gi_->gen.normal.mean = (maximum + minimum) / 2;
        gi_->gen.normal.stddev = (maximum - minimum) / 4;
        gi_->type = GEN_NORMAL;
    }
    ~TraceNormal () {}
    uint64_t Next () override {
        double p;
        double val = 0;
        double random = 0;
        do {
            p = RandomDouble ();
            if (p < 0.8) {
                // F^-1(p) = - G^-1(p)
                double t = sqrt (-2.0 * log (p));
                random = -(t - ((0.010328 * t + 0.802853) * t + 2.515517) /
                                   (((0.001308 * t + 0.189269) * t + 1.432788) * t + 1.0));
            } else {
                double t = sqrt (-2.0 * log (1 - p));
                random = t - ((0.010328 * t + 0.802853) * t + 2.515517) /
                                 (((0.001308 * t + 0.189269) * t + 1.432788) * t + 1.0);
            }
            val = (random + 5) * (gi_->gen.normal.max - gi_->gen.normal.min) / 10;
        } while (val < gi_->gen.normal.min || val > gi_->gen.normal.max);

        return val;
    }
};

class TraceZipfian : public Trace {
public:
    explicit TraceZipfian (int seed, uint64_t minimum = 0,
                           uint64_t maximum = UINT64_C (0xc0000000000))
        : Trace (seed), range_ (maximum) {
        gi_ = new GenInfo ();
        struct GenInfo_Zipfian* const gz = &(gi_->gen.zipfian);

        const uint64_t items = maximum - minimum + 1;
        gz->nr_items = items;
        gz->base = minimum;
        gz->zipfian_constant = ZIPFIAN_CONSTANT;
        gz->theta = ZIPFIAN_CONSTANT;
        gz->zeta2theta = Zeta (2, ZIPFIAN_CONSTANT);
        gz->alpha = 1.0 / (1.0 - ZIPFIAN_CONSTANT);
        double zetan = Zeta (items, ZIPFIAN_CONSTANT);
        gz->zetan = zetan;
        gz->eta = (1.0 - std::pow (2.0 / (double)items, 1.0 - ZIPFIAN_CONSTANT)) /
                  (1.0 - (gz->zeta2theta / zetan));
        gz->countforzeta = items;
        gz->min = minimum;
        gz->max = maximum;

        gi_->type = GEN_ZIPFIAN;
    }
    ~TraceZipfian () {}
    uint64_t Next () override {
        // ScrambledZipfian. scatters the "popular" items across the itemspace.
        const uint64_t z = NextRaw ();
        const uint64_t xz = gi_->gen.zipfian.min + (FNVHash64 (z) % gi_->gen.zipfian.nr_items);
        return xz;
    }
    uint64_t NextRaw () {
        // simplified: no increamental update
        const GenInfo_Zipfian* gz = &(gi_->gen.zipfian);
        const double u = RandomDouble ();
        const double uz = u * gz->zetan;
        if (uz < 1.0) {
            return gz->base + 0lu;
        } else if (uz < (1.0 + pow (0.5, gz->theta))) {
            return gz->base + 1lu;
        }
        const double x = ((double)gz->nr_items) * pow (gz->eta * (u - 1.0) + 1.0, gz->alpha);
        const uint64_t ret = gz->base + (uint64_t)x;
        return ret;
    }
    double Zeta (const uint64_t n, const double theta) {
        // assert(theta == zetalist_theta);
        const uint64_t zlid0 = n / zetalist_step;
        const uint64_t zlid = (zlid0 > zetalist_count) ? zetalist_count : zlid0;
        const double sum0 = zetalist_double[zlid];
        const uint64_t start = zlid * zetalist_step;
        const uint64_t count = n - start;
        assert (n > start);
        const double sum1 = ZetaRange (start, count, theta);
        return sum0 + sum1;
    }
    double ZetaRange (const uint64_t start, const uint64_t count, const double theta) {
        double sum = 0.0;
        if (count > 0x10000000) {
            fprintf (stderr, "zeta_range would take a long time... kill me or wait\n");
        }
        for (uint64_t i = 0; i < count; i++) {
            sum += (1.0 / pow ((double)(start + i + 1), theta));
        }
        return sum;
    }
    uint64_t FNVHash64 (const uint64_t value) {
        uint64_t hashval = FNV_OFFSET_BASIS_64;
        uint64_t val = value;
        for (int i = 0; i < 8; i++) {
            const uint64_t octet = val & 0x00ff;
            val = val >> 8;
            // FNV-1a
            hashval = (hashval ^ octet) * FNV_PRIME_64;
        }
        return hashval;
    }

private:
    uint64_t zetalist_u64[17] = {
        0,
        UINT64_C (0x4040437dd948c1d9),
        UINT64_C (0x4040b8f8009bce85),
        UINT64_C (0x4040fe1121e564d6),
        UINT64_C (0x40412f435698cdf5),
        UINT64_C (0x404155852507a510),
        UINT64_C (0x404174d7818477a7),
        UINT64_C (0x40418f5e593bd5a9),
        UINT64_C (0x4041a6614fb930fd),
        UINT64_C (0x4041bab40ad5ec98),
        UINT64_C (0x4041cce73d363e24),
        UINT64_C (0x4041dd6239ebabc3),
        UINT64_C (0x4041ec715f5c47be),
        UINT64_C (0x4041fa4eba083897),
        UINT64_C (0x4042072772fe12bd),
        UINT64_C (0x4042131f5e380b72),
        UINT64_C (0x40421e53630da013),
    };

    double* zetalist_double = (double*)zetalist_u64;
    uint64_t zetalist_step = UINT64_C (0x10000000000);
    uint64_t zetalist_count = 16;
    // double zetalist_theta = 0.99;
    uint64_t range_;
};
}  // namespace util
   // hopman_fast

#endif
