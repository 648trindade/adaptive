
#pragma once

#include <cmath>
#include <memory>
#include <omp.h>

namespace adapt {

class Work;
class JoinableWork;
using WorkPtr = std::shared_ptr<Work>;

inline unsigned int log_2(unsigned int a) {
    unsigned int res = 0;
    for (; a != 0; a >>= 1, res++)
        ;
    return res;
}

class Work {
    omp_lock_t _lock;

  protected:
    virtual bool extract_nextseq() = 0;
    virtual bool extract_par(WorkPtr &) = 0;
    virtual void localcompute() = 0;

  public:
    Work() {
        omp_init_lock(&_lock);
    }
    virtual ~Work() {
        omp_destroy_lock(&_lock);
    }

    bool ExtractNextSeq() {
        omp_set_lock(&_lock);
        bool local_extract_is_possible = extract_nextseq();
        omp_unset_lock(&_lock);
        return local_extract_is_possible;
    }

    bool ExtractPar(WorkPtr &wp) {
        //omp_set_lock(&_lock);
        while(omp_test_lock(&_lock) == 0) {}

        bool steal_extract_is_possible = extract_par(wp);
        omp_unset_lock(&_lock);
        return steal_extract_is_possible;
    }
    void LocalCompute() {
        localcompute();
    }
};

struct AdaptSteal {
    void operator()(WorkPtr wp) {
//      printf("adapt!\n");
        WorkPtr wp2(nullptr);
        if (wp->ExtractPar(wp2)) {
#pragma omp task shared(wp) untied
            AdaptSteal()(wp);
#pragma omp task shared(wp2) untied
            AdaptSteal()(wp2);
            while (wp2->ExtractNextSeq())
                wp2->LocalCompute();
#pragma omp taskwait
        }
    }
};

inline void run_adapt(WorkPtr wp) {
#pragma omp parallel
#pragma omp master
    {
#pragma omp task shared(wp) untied
        AdaptSteal()(wp);
        while (wp->ExtractNextSeq())
            wp->LocalCompute(); // nanoloop
#pragma omp taskwait
    }
}
};
