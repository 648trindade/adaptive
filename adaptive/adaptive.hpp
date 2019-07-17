#include <vector> 
#include <omp.h>    //OpenMP
#include <string.h> //memset
#include <atomic>
#include <cmath>
#include <algorithm>
#include <memory>
#include <limits>

#define ADPT_ALPHA 1

#define WorkerPtr Worker<Function, Index>
#define WorkerVec std::shared_ptr<std::vector<WorkerPtr>>

namespace adapt {

  namespace //anonymous namespace
  {

    #ifdef DDEBUG
    typedef struct{
      double time;
      char type;
      size_t wf, wl, f, l;
    }trace_t;
    #endif

    template <class Function, class Index>
    class Worker{
    private:
      Index _nthr;
      Index _id; // ID desta thread
      Index _seq_chunk; // Tamanho do chunk
      Index _working_first;
      Index _working_last;
      WorkerVec _workers_array;

    public:
      std::atomic<Index> first; // Início do intervalo
      std::atomic<Index> last; // Fim do intervalo
      Index min_steal; // Raiz quadrada do tamanho total
      omp_lock_t lock; // trava
      #ifdef DDEBUG
      trace_t trace[400];
      size_t count;
      #endif

      Worker(){
        omp_init_lock(&lock);
      }

      ~Worker(){
        omp_destroy_lock(&lock);
      }

      void initialize(
        size_t thr_id, Index global_first, Index global_last, WorkerVec& workers_array
      ){
        _id = thr_id; // ID desta thread
        _nthr = omp_get_max_threads();
        //_visited.assign(_nthr, false);
        _workers_array = workers_array;
        
        const Index chunk  = (global_last - global_first) / _nthr; // Tamanho da tarefa inicial (divisão inteira)
        const Index remain = (global_last - global_first) % _nthr; // Sobras da divisão anterior
        first = chunk * _id + global_first + std::min(_id, remain); // Ponto inicial do intervalo
        last = first + chunk + static_cast<Index>(_id < remain); // Ponto final
        _working_first = _working_last = first;
        
        const Index m   = last - first;  // Tamanho original do intervalo
        _seq_chunk = std::log2(m);  // Tamanho do chunk a ser extraído serialmente
        min_steal  = std::sqrt(m);  // Tamanho mínimo a ser roubado
        #ifdef DDEBUG
        count = 0;
        #endif
      }

    private:

    /*
    * Function: adpt_extract_seq
    * --------------------------
    *   Extrai trabalho sequencial do intervalo pertencente a thread corrente.
    *   A quantia de trabalho a ser extraída é ALPHA * log2(m), onde m é o tamanho do intervalo.
    *   Caso detecte um conflito (algum ladrão rouba parte do trabalho que ele 
    *   extraíria), tenta extrair novamente.
    * 
    *   returns : true se pode extrair um intervalo maior que 0, false caso contrário
    */
      bool extract_seq(){
        const Index old_first = first;
        _working_first = std::min(old_first + _seq_chunk, static_cast<Index>(last));
        first = _working_first;
        if ((_working_first < last) && (_working_first > _working_last)){
          _working_last  = _working_first;
          _working_first = old_first;
          #ifdef DDEBUG
          trace[count++] = {omp_get_wtime(), 'x', _working_first, _working_last, first, last};
          #endif
          return true;
        }
        /* conflict detected: rollback and lock */
        first = old_first;
        omp_set_lock(&(lock));
        _working_first = old_first;
        if (_working_first < last)
          first = _working_last = last;//std::min(old_first + _seq_chunk, static_cast<Index>(last));
        omp_unset_lock(&(lock));

        #ifdef DDEBUG
        trace[count++] = {omp_get_wtime(), 'r', _working_first, _working_last, first, last};
        #endif
        return (_working_first < first);

      }

    /*
    * Function: adpt_extract_par
    * --------------------------
    *   Rouba trabalho sequencial de um intervalo pertencente a uma outra thread.
    *   O algoritmo seleciona aleatoriamente um intervalo e faz os seguintes testes:
    *    - Se já não foi visitado
    *    - Se pode ser travado
    *    - Se possui a quantia de trabalho necessária para roubo
    *   A quantia de trabalho necessária é ((m - m')/2) e não pode ser menor que (sqrt(m)),
    *   onde m é o tamanho do intervalo e m' é o tamanho do intervalo já trabalhado.
    * 
    *   returns : true se o roubo for bem sucedido, false caso contrário
    */
      bool extract_par(){
        bool success = false;
        size_t remaining = _nthr - 1;
        size_t i;
        
        first = std::numeric_limits<Index>::max();

        std::vector<bool> _visited(_nthr, false);
        _visited[_id] = true;

        // Enquanto houver intervalos que não foram inspecionadas
        while(remaining && !success){
          for (i = rand() % _nthr; _visited[i]; i = (i+1) % _nthr); // Avança até um intervalo ainda não visto
          WorkerPtr& victim = _workers_array->at(i);

          const Index pre_chunk_size = (victim.last - victim.first) >> 1;
          const Index pre_steal_size = (pre_chunk_size >= victim.min_steal) \
                                      ? pre_chunk_size : static_cast<Index>(victim.last);
          // Testa se o intervalo não está travado e trava se estiver livre.
          if ((victim.last - pre_steal_size) > victim.first) {
            if (omp_test_lock(&(victim.lock))){
              const Index chunk_size = (victim.last - victim.first) >> 1;
              const Index _last = victim.last - chunk_size;
              last = _last;
              victim.last = _last; //victim.last = last;
              if (last <= victim.first){
                /* rollback and abort */
                victim.last = _last + chunk_size;
                --remaining;
                _visited[i] = true;
                #ifdef DDEBUG
                trace[count++] = {omp_get_wtime(), 't', _last, _last + chunk_size, victim.first, victim.last};
                #endif
              }
              else{
                first = _last;
                last  = _last + chunk_size;
                min_steal  = std::sqrt(chunk_size);
                min_steal  = std::max(min_steal, static_cast<Index>(4));
                _seq_chunk = std::log2(chunk_size);
                _seq_chunk = std::max(_seq_chunk, static_cast<Index>(1));
                success = true;
                #ifdef DDEBUG
                trace[count++] = {omp_get_wtime(), 's', _last, _last + chunk_size, victim.first, victim.last};
                #endif
              }
              omp_unset_lock(&(victim.lock));
            }
          }
          else {
            --remaining;
            _visited[i] = true;
          }
        }
        return success;
      }

    public:

      void work(Function kernel){
        while(true){ // Itera enquanto houver trabalho a ser feito
          // Itera enquanto houver trabalho sequencial a ser feito
          while (extract_seq())
            for (Index i = _working_first; i < _working_last; i++)
              kernel(i);
          
          // Tenta roubar trabalho, se não conseguir, sai do laço
          if (!extract_par())
            return;
        }
      }
    };

  }

  /*
  * Function: adpt_parallel_for
  * ---------------------------
  * 
  *   kernel : função de processamento
  *    first : início do laço
  *     last : final do laço
  */
  template <class Function, class Index>
  void parallel_for(Index first, Index last, Function kernel){
    WorkerVec workers = std::make_shared<std::vector<WorkerPtr>>(omp_get_max_threads()); // Dados para os workers

    #pragma omp parallel shared(kernel, workers, first, last)
    {
      size_t  thr_id = omp_get_thread_num();  // ID desta thread
      WorkerPtr& m_worker = workers->at(thr_id); // pega o worker desta thread
      m_worker.initialize(thr_id, first, last, workers); // inicia o worker
      
      #pragma omp barrier
      m_worker.work(kernel);
    }

    #ifdef DDEBUG
    for (int i=0; i< omp_get_max_threads(); i++){
      WorkerPtr& worker = workers->at(i);
      for (int j=0; j<worker.count; j++)
        fprintf(stderr, "%.12lf %d %c %lu %lu %lu %lu\n", worker.trace[j].time, i, worker.trace[j].type, worker.trace[j].wf, worker.trace[j].wl, worker.trace[j].f, worker.trace[j].l);
    }
    #endif
    workers->clear();
  }

};

#undef WorkerPtr
#undef WorkerVec