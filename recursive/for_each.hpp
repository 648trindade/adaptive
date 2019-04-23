
#pragma once

#include "adapt.hpp"
#include <algorithm>
#include <cstdio>
#include <atomic>

namespace adapt {

template <class It, class Op> class ForeachWork : public Work {
    It _first;
    It _last;
    Op _op;

    std::atomic<size_t> _beg;
    std::atomic<size_t> _end;
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
        _seqgrain = std::log2(static_cast<size_t>(_end));
        _beg_local = _end_local = _beg;
    }

    bool extract_nextseq() {
        const size_t old_beg = _beg;
        _beg_local = old_beg + _seqgrain;
        _beg_local = std::min(_beg_local, static_cast<size_t>(_end));
        _beg = _beg_local;
        if ((_beg_local < _end) && (_beg_local > _end_local)) {
            _end_local = _beg_local;
            _beg_local = old_beg;
            return true;
        }
        /*conflict detected: rollback and lock*/
        _beg = old_beg;
        omp_set_lock(&_lock);
        _beg_local = old_beg;
        if (_beg_local < _end)
            _beg = _end_local = _end;
        omp_unset_lock(&_lock);
        return (_beg_local < _beg);
        
//         if (_beg < _end) {
//             _beg_local = _beg;
//             size_t m_beg = _beg + _seqgrain;
//             m_beg = std::min(m_beg, static_cast<size_t>(_end));
            
//             _beg = m_beg;
//             //__sync_synchronize();
//             if (_beg > _end){ //conflict: rollback and try again
//                 _beg = _beg_local;
//                 return extract_nextseq();
//             }
            
//             _end_local = _beg;
// #if defined(CONFIG_VERBOSE)
//             printf("%d:%s beg_local=%zu end_local=%zu\n", omp_get_thread_num(),
//                    __FUNCTION__, _beg_local, _end_local);
// #endif
//             return true;
//         }
//         return false;
    }

    bool extract_par(WorkPtr &sw) {
        omp_set_lock(&_lock);
        if (_end - _seqgrain > _beg){
            const size_t chunk_size = (_end - _beg) >> 1;
            const size_t new_end = _end - chunk_size;
            _end = new_end;
            if (new_end <= _beg)
                /* rollback and abort */
                _end = new_end + chunk_size;
            else{
                size_t i = new_end;
                size_t j = new_end + chunk_size;
                std::shared_ptr<ForeachWork> wp(new ForeachWork<It, Op>(_first, _first + j, _op, i));
                sw = std::move(wp);
                omp_unset_lock(&_lock);
                return true;
            }
        }
        omp_unset_lock(&_lock);
        return false;

//         size_t work_left = _end - _beg;
//         if (work_left > _pargrain && (_end > _beg)) {
//             size_t i, j;
//             size_t mid;
//             //FIXME: testes desnecessários
//             //if((_end - _beg) > std::sqrt(_end - _beg))
//               mid = _end - (work_left / 2);
//             //else
//             //  mid = _end - std::sqrt(_end - _beg);

//             i = mid;
//             j = _end;
//             _end = mid;
//             //__sync_synchronize();
//             // Conflict: rollback and abort
//             if (mid < _beg){
//                 _end = j;
//                 return false;
//             }

// #if defined(CONFIG_VERBOSE)
//             printf("%d:%s i=%zu j=%zu\n", omp_get_thread_num(), __FUNCTION__, i,
//                    j);
// #endif
//             std::shared_ptr<ForeachWork> wp(
//                 new ForeachWork<It, Op>(_first, _first + j, _op, i));
//             sw = std::move(wp);
//             // sw = new ForeachWork<It, Op>(_first, _first+j, _op, i);
//             return true;
//         }
//         return false;
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

template <class InputIt, class Function>
void for_each(InputIt first, InputIt last, Function op) {
    run_adapt(std::make_shared<ForeachWork<InputIt, Function>>(first, last, op, 0));
}
};
