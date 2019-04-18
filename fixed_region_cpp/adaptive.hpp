#include <vector> 
#include <omp.h>    //OpenMP
#include <string.h> //memset
#include <atomic>
#include <cmath>
#include <algorithm>
#include <memory>

#define ALPHA 1

#define WorkerPtr Worker<Function, Data>
#define WorkerVec std::shared_ptr<std::vector<WorkerPtr>>

template <class Function, class Data>
class Worker{
private:
    size_t _nthr;
    size_t _id; // ID desta thread
    size_t _seq_chunk; // Tamanho do chunk
    size_t _working_first;
    size_t _working_last;
    //std::vector<bool> _visited; // vetor de histórico de acesso
    WorkerVec _workers_array;

public:
    std::atomic<size_t> first; // Início do intervalo
    std::atomic<size_t> last; // Fim do intervalo
    size_t min_steal; // Raiz quadrada do tamanho total
    omp_lock_t lock; // trava

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
        
        const size_t m   = last - first;  // Tamanho original do intervalo
        _seq_chunk = std::log2(m);  // Tamanho do chunk a ser extraído serialmente
        min_steal  = std::sqrt(m);  // Tamanho mínimo a ser roubado
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
 *   wrkr_d : estrutura com info. do worker
 *        f : o índice inicial do trabalho extraído
 *        l : o índice final do trabalho extraído
 * 
 *   returns : o tamanho do intervalo extraído
*/
    bool extract_seq(){
        if (first < last){
            _working_first = first;
            _working_last = _working_first + _seq_chunk;
            _working_last = std::min(static_cast<size_t>(last), _working_last);
            first = _working_last;
            //__sync_synchronize();
            if (first > last) { // Conflito
                omp_set_lock(&lock);
                    _working_last = std::max(std::min(static_cast<size_t>(last), _working_first + _seq_chunk), _working_first);
                    first = _working_last;
                omp_unset_lock(&lock);
            }
            return true;
        }
        return false;
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
 *    theft_d : estrutura com info. do ladrão
 *   v_wrkr_d : vetor de estruturas com info. dos workers
 *       nthr : número total de threads da aplicação
 * 
 *   returns : 1 se o roubo for bem sucedido, 0 do contrário
*/
    bool extract_par(){
        bool success = false;
        size_t remaining = _nthr - 1;
        size_t i;

        std::vector<bool> _visited(_nthr, false);
        //std::fill(_visited.begin(), _visited.end(), true);
        _visited[_id] = true;

        // Enquanto houver intervalos que não foram inspecionadas
        while(remaining && !success){
            //i = rand() % _nthr; // Sorteia uma intervalo
            for (i = rand() % _nthr; _visited[i]; i = (i+1) % _nthr); // Avança até um intervalo ainda não visto
            WorkerPtr& victim = _workers_array->at(i);

            // Testa se o intervalo não está travado e trava se estiver livre.
            if (omp_test_lock(&(victim.lock))){
                const size_t vic_l = victim.last, vic_f = victim.first; // cópias
                const size_t half_left = (vic_l - vic_f) >> 1; // Metade do trabalho restante
                const size_t steal_size = (half_left >= victim.min_steal) ? half_left : 0; // Tamanho do roubo
                const size_t begin = vic_l - steal_size;

                // O tamanho do roubo é maior que 1?
                // O início é menor que o final para a vítima? - evita erros integer overflow
                if ((steal_size > 1) && (vic_f < vic_l)){
                    victim.last = begin;
                    //__sync_synchronize();
                    if (victim.last >= victim.first){ // Inicio a frente do inicio da vitima
                        // Efetua o roubo, atualizando os intervalos
                        omp_set_lock(&lock);
                            last      = vic_l;
                            first     = begin;
                            min_steal = std::sqrt(steal_size);
                        omp_unset_lock(&lock);
                        _seq_chunk = std::log2(steal_size);
                        success = true;
                    }
                    else // Conflito: desfaz e aborta
                        victim.last = vic_l;
                }
                else { // Caso não houver trabalho suficiente
                    _visited[i] = true; // Marca como visitado
                    --remaining;        // Decrementa o total de intervalos restantes para inspeção
                }
                omp_unset_lock(&(victim.lock)); // Destrava o intervalo
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
 */
template <class Function, class Data>
void adpt_parallel_for(
    Function kernel, Data& data, size_t first, size_t last
){
    WorkerVec workers = std::make_shared<std::vector<WorkerPtr>>(omp_get_max_threads()); // Dados para os workers
    //for (int i=0; i < omp_get_max_threads(); i++)
    //    workers->push_back(std::move(Worker<Function,Data>()));

    #pragma omp parallel shared(kernel, data, workers, first, last)
    {
        size_t  thr_id = omp_get_thread_num();  // ID desta thread
        WorkerPtr& m_worker = workers->at(thr_id); // pega o worker desta thread
        m_worker.initialize(thr_id, first, last, workers); // inicia o worker
        
        #pragma omp barrier
        m_worker.work(kernel, data);
    }
}