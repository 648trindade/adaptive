#include <vector> 
#include <omp.h>    //OpenMP
#include <string.h> //memset
#include <atomic>
#include <cmath>
#include <algorithm>
#include <memory>
#include <limits>

#define ALPHA 1

#define WorkerPtr Worker<Function, Data>
#define WorkerVec std::shared_ptr<std::vector<WorkerPtr>>

#ifdef DDEBUG
typedef struct{
    double time;
    char type;
    size_t wf, wl, f, l;
}trace_t;
#endif

template <class Function, class Data>
class Worker{
private:
    size_t _nthr;
    size_t _id; // ID desta thread
    size_t _seq_chunk; // Tamanho do chunk
    size_t _working_first;
    size_t _working_last;
    WorkerVec _workers_array;

public:
    std::atomic<size_t> first; // Início do intervalo
    std::atomic<size_t> last; // Fim do intervalo
    size_t min_steal; // Raiz quadrada do tamanho total
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
        size_t thr_id, size_t global_first, size_t global_last, WorkerVec& workers_array
    ){
        _id = thr_id; // ID desta thread
        _nthr = omp_get_max_threads();
        //_visited.assign(_nthr, false);
        _workers_array = workers_array;
        
        const size_t chunk  = (global_last - global_first) / _nthr; // Tamanho da tarefa inicial (divisão inteira)
        const size_t remain = (global_last - global_first) % _nthr; // Sobras da divisão anterior
        first = chunk * _id + global_first + std::min(_id, remain); // Ponto inicial do intervalo
        last = first + chunk + static_cast<size_t>(_id < remain); // Ponto final
        _working_first = _working_last = first;
        
        const size_t m   = last - first;  // Tamanho original do intervalo
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
        const size_t old_first = first;
        _working_first = std::min(old_first + _seq_chunk, static_cast<size_t>(last));
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
            first = _working_last = last;//std::min(old_first + _seq_chunk, static_cast<size_t>(last));
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
        
        first = std::numeric_limits<size_t>::max();

        std::vector<bool> _visited(_nthr, false);
        _visited[_id] = true;

        // Enquanto houver intervalos que não foram inspecionadas
        while(remaining && !success){
            for (i = rand() % _nthr; _visited[i]; i = (i+1) % _nthr); // Avança até um intervalo ainda não visto
            WorkerPtr& victim = _workers_array->at(i);

            // Testa se o intervalo não está travado e trava se estiver livre.
            if ((victim.last - victim.min_steal) > victim.first) {
                if (omp_test_lock(&(victim.lock))){
                    const size_t chunk_size = (victim.last - victim.first) >> 1;
                    const size_t _last = victim.last - chunk_size;
                    last = _last;
                    victim.last = _last; //victim.last = last;
                    if (last <= victim.first){
                        /* rollback and abort */
                        victim.last = _last + chunk_size;
#ifdef DDEBUG
                        trace[count++] = {omp_get_wtime(), 't', _last, _last + chunk_size, victim.first, victim.last};
#endif
                        --remaining;
                        _visited[i] = true;
                    }
                    else{
                        first = _last;
                        last  = _last + chunk_size;
                        min_steal  = std::sqrt(chunk_size);
                        min_steal  = std::max(min_steal, 4ul);
                        _seq_chunk = std::log2(chunk_size);
                        _seq_chunk = std::max(_seq_chunk, 1ul);
#ifdef DDEBUG
                        trace[count++] = {omp_get_wtime(), 's', _last, _last + chunk_size, victim.first, victim.last};
#endif
                        success = true;
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

    void work(Function kernel, Data& data){
        while(true){ // Itera enquanto houver trabalho a ser feito
            // Itera enquanto houver trabalho sequencial a ser feito
            while (extract_seq())
                for (size_t i = _working_first; i < _working_last; i++)
                    kernel(data, i);
            
            // Tenta roubar trabalho, se não conseguir, sai do laço
            if (!extract_par())
                return;
        }
    }
};

/*
 * Function: adpt_parallel_for
 * ---------------------------
 * 
 *   kernel : função de processamento
 *     data : dados passados como argumentos à função
 *    first : início do laço
 *     last : final do laço
 */
template <class Function, class Data>
void adpt_parallel_for(
    Function kernel, Data& data, size_t first, size_t last
){
    WorkerVec workers = std::make_shared<std::vector<WorkerPtr>>(omp_get_max_threads()); // Dados para os workers

    #pragma omp parallel shared(kernel, data, workers, first, last)
    {
        size_t  thr_id = omp_get_thread_num();  // ID desta thread
        WorkerPtr& m_worker = workers->at(thr_id); // pega o worker desta thread
        m_worker.initialize(thr_id, first, last, workers); // inicia o worker
        
        #pragma omp barrier
        m_worker.work(kernel, data);
    }

#ifdef DDEBUG
    for (int i=0; i< omp_get_max_threads(); i++){
        WorkerPtr& worker = workers->at(i);
        for (int j=0; j<worker.count; j++)
            fprintf(stderr, "%.12lf %d %c %lu %lu %lu %lu\n", worker.trace[j].time, i, worker.trace[j].type, worker.trace[j].wf, worker.trace[j].wl, worker.trace[j].f, worker.trace[j].l);
    }
#endif
}