
#pragma once

#include "adapt.hpp"
#include <algorithm>
#include <cstdio>
#include <atomic>

namespace adapt {

template <class Idx, class Op> class ParallelForWork : public Work {
    Idx _first;
    Idx _last;
    Op _op;

    std::atomic<size_t> _beg;
    std::atomic<size_t> _end;
    size_t _seqgrain;
    size_t _pargrain;
    size_t _c;
    size_t _beg_local;
    size_t _end_local;

  public:
    ParallelForWork(Idx first, Idx last, Op op, size_t beg, size_t pargrain = 2)
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
        
    }

    bool extract_par(WorkPtr &sw) {
        omp_set_lock(&_lock);
        const size_t chunk_size = (_end - _beg) >> 1;
        const size_t steal_size = (chunk_size >= _seqgrain) ? chunk_size : static_cast<size_t>(_end); // _end to cancel steal
        const size_t new_end    = _end - steal_size;
        if (new_end > _beg){
            _end = new_end;
            if (new_end <= _beg)
                /* rollback and abort */
                _end = new_end + steal_size;
            else{
                const size_t i = new_end;
                const size_t j = new_end + steal_size;
                std::shared_ptr<ParallelForWork> wp(new ParallelForWork<Idx, Op>(_first, _first + j, _op, i));
                sw = std::move(wp);
                omp_unset_lock(&_lock);
                return true;
            }
        }
        omp_unset_lock(&_lock);
        return false;

    }

    void localcompute() {
        Idx ibegin = _first + _beg_local;
        Idx iend = _first + _end_local;
#if defined(CONFIG_VERBOSE)
        printf("%d:%s (%zu, %zu)\n", omp_get_thread_num(), __FUNCTION__,
               _beg_local, _end_local);
#endif
        for (Idx i = ibegin; i != iend; i++)
            _op(i);
    }
};

template <class Index, class Function>
void parallel_for(Index first, Index last, Function op) {
    run_adapt(std::make_shared<ParallelForWork<Index, Function>>(first, last, op, 0));
}
};
