
#pragma once

#include "adapt.hpp"
#include <algorithm>
#include <cstdio>

namespace adapt {

template <class It, class Op, class Data> class ForeachWork : public Work {
    It _first;
    It _last;
    Op _op;
    Data _data;

    volatile size_t _beg;
    volatile size_t _end;
    size_t _seqgrain;
    size_t _pargrain;
    size_t _c;
    size_t _beg_local;
    size_t _end_local;

  public:
    ForeachWork(It first, It last, Op op, size_t beg, Data data, size_t pargrain = 2)
        : Work(), _first(first), _last(last), _op(op), _beg(beg), _data(data),
          _pargrain(pargrain) {
        _end = last - first;
        _seqgrain = std::log2(_end);
    }

    bool extract_nextseq() {
        if (_beg < _end) {
            _beg_local = _beg;
            size_t m_beg = _beg + _seqgrain;
            m_beg = std::min(m_beg, static_cast<size_t>(_end));
            
            _beg = m_beg;
            __sync_synchronize();
            if (_beg > _end){ //conflict: rollback and try again
                _beg = _beg_local;
                return extract_nextseq();
            }
            
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
        size_t work_left = _end - _beg;
        if (work_left > _pargrain && (_end > _beg)) {
            size_t i, j;
            size_t mid;
            //FIXME: testes desnecessários
            //if((_end - _beg) > std::sqrt(_end - _beg))
              mid = _end - (work_left / 2);
            //else
            //  mid = _end - std::sqrt(_end - _beg);

            i = mid;
            j = _end;
            _end = mid;
            __sync_synchronize();
            // Conflict: rollback and abort
            if (_end < _beg){
                _end = j;
                return false;
            }

#if defined(CONFIG_VERBOSE)
            printf("%d:%s i=%zu j=%zu\n", omp_get_thread_num(), __FUNCTION__, i,
                   j);
#endif
            std::shared_ptr<ForeachWork> wp(
                new ForeachWork<It, Op, Data>(_first, _first + j, _op, i, _data));
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
        //std::for_each((It)ibegin, (It)iend, _op);
        for(It &it = ibegin; it != iend; it++)
            _op(*it, _data);
    }
};

template <class InputIt, class Function, class Data>
void for_each(InputIt first, InputIt last, Function op, Data data) {
    run_adapt(std::make_shared<ForeachWork<InputIt, Function, Data>>(first, last, op, 0, data));
}
};
