#include <stdlib.h> //malloc, free
#include <math.h>   //sqrt
#include <omp.h>    //OpenMP
#include <string.h> //memset

#define MIN(a,b) ((a<=b)?a:b)
#define MAX(a,b) ((a<=b)?b:a)
#define ALPHA 1
#define LOG2(X) (63 - __builtin_clzll(X))

// Estrutura representando um intervalo
typedef struct{
    volatile size_t f; // Início do intervalo
    volatile size_t l; // Fim do intervalo
    size_t c; // Tamanho do chunk
    size_t r; // Raiz quadrada do tamanho total
} adpt_range_t;

typedef struct {
    size_t id;          // ID desta thread
    adpt_range_t range; // intervalo
    omp_lock_t lock;    // trava
    char* visited;      // vetor de histórico de acesso
} worker_data_t;

void initialize_worker_data(worker_data_t* wrkr_d, size_t thr_id, size_t nthr, size_t first, size_t last){
    wrkr_d->id = thr_id; // ID desta thread
    wrkr_d->visited = malloc(nthr); // Vetor de vetores de histórico de acesso

    size_t chunk = (last - first) / nthr; // Tamanho da tarefa inicial (divisão inteira)
    size_t remain = (last - first) % nthr; // Sobras da divisão anterior
    wrkr_d->range.f = chunk * thr_id + first + MIN(thr_id, remain); // Ponto inicial do intervalo
    wrkr_d->range.l = wrkr_d->range.f + chunk + (size_t)(thr_id < remain); // Ponto final
    size_t m = wrkr_d->range.l - wrkr_d->range.f;  // Tamanho original do intervalo
    wrkr_d->range.c = LOG2(m); // Tamanho do chunk a ser extraído serialmente
    wrkr_d->range.r = (size_t)sqrt(m); // Tamanho mínimo a ser roubado

    omp_init_lock(&(wrkr_d->lock)); // Inicializa a trava
}

void destroy_worker_data(worker_data_t* wrkr_d){
    free(wrkr_d->visited);
    omp_destroy_lock(&(wrkr_d->lock));
}

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
size_t adpt_extract_seq(worker_data_t* wrkr_d, size_t* f, size_t* l){
    *f = wrkr_d->range.f;
    *l = MIN(wrkr_d->range.l, *f + (size_t)(ALPHA * wrkr_d->range.c));
    wrkr_d->range.f = *l;
    __sync_synchronize();
    if ((wrkr_d->range.f > wrkr_d->range.l) || (*l < *f)) { // Conflito
        omp_set_lock(&(wrkr_d->lock));
            *l = MAX(MIN(wrkr_d->range.l, *f + (size_t)(ALPHA * wrkr_d->range.c)), *f);
            wrkr_d->range.f = *l;
        omp_unset_lock(&(wrkr_d->lock));
    }
    return *l - *f;
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
size_t adpt_extract_par(
    worker_data_t* theft_d, worker_data_t* v_wrkr_d, size_t nthr
){
    size_t success   = 0;
    size_t remaining = nthr - 1;
    size_t i;

    memset(theft_d->visited, 0, nthr);
    theft_d->visited[theft_d->id] = 1;

    // Enquanto houver intervalos que não foram inspecionadas
    while(remaining && !success){
        i = rand() % nthr; // Sorteia uma intervalo
        for (; theft_d->visited[i]; i = (i+1)%nthr); // Avança até um intervalo ainda não visto
        worker_data_t* victim_d = &(v_wrkr_d[i]);

        // Testa se o intervalo não está travado e trava se estiver livre.
        if (omp_test_lock(&(victim_d->lock))){
            size_t vic_l = victim_d->range.l, vic_f = victim_d->range.f; // cópias
            size_t half_left = (vic_l - vic_f) >> 1; // Metade do trabalho restante
            size_t steal_size = (half_left >= victim_d->range.r) ? half_left : 0; // Tamanho do roubo
            size_t begin = vic_l - steal_size;

            // O tamanho do roubo é maior que 1?
            // O início é menor que o final para a vítima? - evita erros integer overflow
            if ((steal_size > 1) && (vic_f < vic_l)){
                victim_d->range.l = begin;
                __sync_synchronize();
                if (victim_d->range.l >= victim_d->range.f){ // Inicio a frente do inicio da vitima
                    // Efetua o roubo, atualizando os intervalos
                    omp_set_lock(&(theft_d->lock));
                        theft_d->range.l = vic_l;
                        theft_d->range.f = begin;
                        theft_d->range.r = (size_t)sqrt(steal_size);
                    omp_unset_lock(&(theft_d->lock));
                    theft_d->range.c = LOG2(steal_size);
                    success = 1;
                }
                else // Conflito: desfaz e aborta
                    victim_d->range.l = vic_l;
            }
            else { // Caso não houver trabalho suficiente
                theft_d->visited[i] = 1; // Marca como visitado
                --remaining;      // Decrementa o total de intervalos restantes para inspeção
            }
            omp_unset_lock(&(victim_d->lock)); // Destrava o intervalo
        }
    }
    return success;
}

/*
 * Function: adpt_parallel_for
 * ---------------------------
 * 
 *   kernel : função de processamento
 *     data : dados passados como argumentos à função
 *    first : início do laço
 */
void adpt_parallel_for(
    void (*kernel)(void*, size_t), void* data, size_t first, size_t last
){
    size_t nthr = omp_get_max_threads(); // número de threads
    worker_data_t* workers_data = malloc(nthr * sizeof(worker_data_t)); // Dados para os workers

    #pragma omp parallel shared(kernel, data, workers_data, nthr, first, last)
    {
        size_t m_first = first, m_last = last; // cópia dos limites originais
        size_t thr_id = omp_get_thread_num();  // ID desta thread
        worker_data_t* m_worker_data = &workers_data[thr_id]; // pega o worker desta thread
        initialize_worker_data(m_worker_data, thr_id, nthr, first, last); // inicia o worker
        #pragma omp barrier

        while(1){ // Itera enquanto houver trabalho a ser feito
            // Itera enquanto houver trabalho sequencial a ser feito
            while (adpt_extract_seq(m_worker_data, &m_first, &m_last) > 0)
                for (size_t i = m_first; i < m_last; i++)
                    kernel(data, i);
            
            // Tenta roubar trabalho, se não conseguir, sai do laço
            if (!adpt_extract_par(m_worker_data, workers_data, nthr))
                break;
        }

        #pragma omp barrier
        destroy_worker_data(m_worker_data); // Limpa o worker
    }

    free(workers_data);
}