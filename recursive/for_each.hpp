
#pragma once

#include "adapt.hpp"
#include <algorithm>
#include <cstdio>

namespace adapt {

template <class It, class Op> class ForeachWork : public Work {
    It _first;
    It _last;
    Op _op;

    volatile size_t _beg;
    volatile size_t _end;
    size_t _seqgrain;
    size_t _pargrain;
    size_t _c;
    size_t _beg_local;
    size_t _end_local;

  public:
    ForeachWork(It first, It last, Op op, size_t beg, size_t pargrain = 2)
        : Work(), _first(first), _last(last), _op(op), _beg(beg),
          _pargrain(pargrain) {
        _end = last - first;
        _seqgrain = std::log2(_end);
    }

    bool extract_nextseq() {
        if (_beg < _end) {
            _beg_local = _beg;
            _beg += _seqgrain;
            if (_beg > _end)
                _beg = _end;
            _end_local = _beg;
#if defined(CONFIG_VERBOSE)
            printf("%d:%s beg_local=%zu end_local=%zu\n", omp_get_thread_num(),
                   __FUNCTION__, _beg_local, _end_local);
#endif
            return true;
        }
        return false;
    }

    bool extract_par(WorkPtr &sw) {
        if ((_end - _beg) > _pargrain) {
            size_t i, j;
            size_t mid;
            if((_end - _beg) > std::sqrt(_end - _beg))
              mid = _end - ((_end - _beg) / 2);
            else
              mid = _end - std::sqrt(_end - _beg);

            i = mid;
            j = _end;
            _end = mid;

#if defined(CONFIG_VERBOSE)
            printf("%d:%s i=%zu j=%zu\n", omp_get_thread_num(), __FUNCTION__, i,
                   j);
#endif
            std::shared_ptr<ForeachWork> wp(
                new ForeachWork<It, Op>(_first, _first + j, _op, i));
            sw = std::move(wp);
            // sw = new ForeachWork<It, Op>(_first, _first+j, _op, i);
            return true;
        }
        return false;
    }

    void localcompute() {
        It ibegin = _first + _beg_local;
        It iend = _first + _end_local;
#if defined(CONFIG_VERBOSE)
        printf("%d:%s (%zu, %zu)\n", omp_get_thread_num(), __FUNCTION__,
               _beg_local, _end_local);
#endif
        std::for_each((It)ibegin, (It)iend, _op);
    }
};

template <class InputIt, class UnaryFunction>
void for_each(InputIt first, InputIt last, UnaryFunction op) {
    run_adapt(std::make_shared<ForeachWork<InputIt, UnaryFunction>>(first, last, op, 0));
}
};
