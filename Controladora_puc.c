#define aeroporto 0.5
#define tempo 10
#define max_pausas 2

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <signal.h>

typedef struct
{
    float x;
    float y;
}Coordenada;

typedef struct 
{
    int id;
    int status;         //0-parada  1-voando
    int lado_entrada;   //0-E   1-W
    int pista;          //0-pista1  1-pista2
    float atraso;
    float velocidade;   
    float tempo_voo;
    float tempo_total;
    Coordenada c_entrada;   //coordenada de entrada
    Coordenada c_atual;     // coordenada atual
}Aeronave;

typedef struct 
{
    int status;
    Coordenada c_atual;
    int pista;
}Parcela;

typedef struct 
{
    float a; // coeficiente angular
    float b; // coeficiente linear
}Reta;


// variaveis globais
Aeronave aeronave;
Parcela* p;  // ponteiro para a memoria compartilhada
int i; // ordem das aeronaves
Reta reta;
int qtd_aeronaves;
int* lpid;  // lista de PID
int trocou_pista = 0; // 0-> nao trocou. 1-> trocou
int* lista_pausas;  

//Funcoes filho
void cria_aeronave(){
    // Inicializa a semente do gerador com o tempo atual
    srand(time(NULL));

    aeronave.id = getpid();
    aeronave.status = 0;
    aeronave.lado_entrada = rand() % 2;
    aeronave.pista = rand() % 2;
    aeronave.atraso = (rand() % 21) / 10.0f;
    aeronave.velocidade = 0.05;
    aeronave.tempo_voo = 0;
    aeronave.c_entrada.x = 0;
    aeronave.c_entrada.y = (rand() % 21) * 0.05f;
    aeronave.c_atual = aeronave.c_entrada;  
    aeronave.tempo_total = tempo;
    aeronave.tempo_voo = 0;
}

// retorna uma pista com base nos 4 quadrantes
int pista_real(){
    if (aeronave.lado_entrada == 0){
        if (aeronave.pista == 0)
            return 1;
        else
            return 2;
    }
    else{
        if (aeronave.pista == 0)
            return 3;
        else
            return 4;
    }

}

void atualizar_memoria(){
    Parcela parcela;
    parcela.c_atual = aeronave.c_atual;
    parcela.pista = pista_real();
    parcela.status = aeronave.status;
    
    p[i] = parcela;
}

void sleep_decimal(float segundos) {
    struct timespec req;
    req.tv_sec = (time_t)segundos; // parte inteira em segundos
    req.tv_nsec = (long)((segundos - req.tv_sec) * 1e9); // parte fracionária em nanossegundos  
    nanosleep(&req, NULL);
}

void calcula_reta(){
    Coordenada c = aeronave.c_entrada;
    reta.a = (aeroporto - c.y) / (aeroporto - c.x);
    reta.b = aeroporto - (aeroporto * reta.a);

}

void calcula_c_atual(){
    float x, y;    // posicao no eixo x e y
    aeronave.tempo_voo += 1;
    x = aeronave.velocidade * aeronave.tempo_voo;
    y = reta.a*x + reta.b;
    printf("pid: %d, p_nasc: %.2f, aeroporto: %d, lado: %d, velocidade: %.2f, tempo: %.2f, x: %.2f, y: %.2f\n", aeronave.id, aeronave.c_entrada.y, aeronave.pista, aeronave.lado_entrada, aeronave.velocidade, aeronave.tempo_voo, x, y);
    aeronave.c_atual.x = x;
    aeronave.c_atual.y = y;
}

void verifica_chegada(){
    if (aeronave.c_atual.x >= aeroporto){
        printf("pid: %d chegou!\n", aeronave.id);
        exit(0);
    }
}

void trata_alarme(int sig){
    calcula_c_atual();
    verifica_chegada();
    atualizar_memoria();
    alarm(1);
}

void comeca(){
    aeronave.status = 1;
    atualizar_memoria();
    sleep_decimal(aeronave.atraso);
    alarm(1);   
}

void tratador1(int sig){  // tratador para SIGUSR1
    aeronave.status = 0;
    atualizar_memoria();
    kill(aeronave.id, SIGSTOP);
}

void tratador2(int sig){  // tratador para SIGUSR2
    if (aeronave.pista == 0)
        aeronave.pista = 1;
    else
        aeronave.pista = 0;
}


// Funcoes do pai
//0-> nao interfere, 1-> interfere
int verifica_radar_valido(int i, int s, int pista){
     if (s != i){                            // verifica se nao eh ele mesmo
        if (p[s].pista == pista){           // verifica se sao vizinhos de quadrante 
            if (kill(lpid[s], 0) == 0){     // verifica se vizinho esta vivo
                return 1;
            }    
        }
    }
    return 0;
}

// 0-> nao interfere, 1-> interfere
int verifica_radar_proximo(int s, Coordenada c){
    if (((fabsf(p[s].c_atual.x - c.x)) < 0.1f) && ((fabsf(p[s].c_atual.y - c.y)) < 0.1f)){    // verifica se esta proximo
        if(p[s].c_atual.x > c.x){       // verifica se vizinho esta a frente
            return 1;
        }
    }
    return 0;
}

// 0-> pouso, 1-> nada
int verifica_pouso(int i, int pista, Coordenada c){    
    if (lista_pausas[i] >= max_pausas){
        printf("pid: %d, pista: %d, x: %.2f, y: %.2f\n", lpid[i], pista, c.x, c.y);
        printf("pid: %d morreu\n", lpid[i]);
        kill(lpid[i], SIGKILL); // pouso
        return 0;
    }
    return 1;
}

void verifica_pausas(int i, int bloqueios, int pista, Coordenada c){
    if (bloqueios > 0){
        lista_pausas[i]+=1;
        printf("pid: %d pausei\n", lpid[i]);
        kill(lpid[i], SIGUSR1); // pausa
    }
    else{
        lista_pausas[i] = 0;
    }
    
    if (lista_pausas[i] > max_pausas){
        printf("pid: %d, pista: %d, x: %.2f, y: %.2f\n", lpid[i], pista, c.x, c.y);
        printf("pid: %d morreu\n", lpid[i]);
        kill(lpid[i], SIGKILL); // pouso
    }
}

void inicializa_lista_pausas(){
    // aloca memoria para o vetor de pausas
    if ((lista_pausas = (int*)malloc(sizeof(int)*qtd_aeronaves)) == NULL){
        exit(-1); 
    }
    // inicializa a o vetor com zeros
    for (i = 0; i < qtd_aeronaves; i++){
        lista_pausas[i] = 0;
    }
}

int trocar_pista_local(int pista){
    if (pista == 1)
        return 2;
    else if (pista == 2)
        return 1;
    else if (pista == 3)
        return 4;
    else if (pista == 4)
        return 3;
}

// verifica provaveis colisoes
void radar(){
    int bloqueios = 0;
    // resgata infs da nave
    int pista = p[i].pista;
    Coordenada c = p[i].c_atual;
    if (trocou_pista == 1){     // troca a pista localmente
        pista = trocar_pista_local(pista);
    }
    for(int s = 0; s < qtd_aeronaves; s++){
        if (verifica_radar_valido(i, s, pista) == 1){
            if(verifica_radar_proximo(s, c) == 1){
                // tenta trocar pista
                if (trocou_pista == 0){
                    kill(lpid[i], SIGUSR2); // troca pista
                    trocou_pista = 1;
                    printf("pid: %d trocou pista\n", lpid[i]);
                    radar();
                    if (lista_pausas[i] == 0){
                        return;
                    }
                    else{
                        lista_pausas[i]-=1;
                        bloqueios+= 1;
                    }
                }
                // volta para a pista original e conta um bloqueio
                else if (trocou_pista == 1){
                    trocou_pista+= 1;
                    kill(lpid[i], SIGUSR2); // troca pista
                    printf("pid: %d voltei para pista original\n", lpid[i]);
                    lista_pausas[i] += 1;
                    return;
                }
                else{
                    if(verifica_pouso(i, pista, c) == 1){
                        bloqueios+= 1;
                    }
                }
            }
        }
    } 
    if (kill(lpid[i], 0) == 0){
        verifica_pausas(i, bloqueios, pista, c);
    }
}




int main(void){
    int segmento, pid, tamanho_alocado;    

    // numero de processos filhos
    printf("Quantas aeronaves: \n");
    scanf("%d", &qtd_aeronaves);

    tamanho_alocado = sizeof(Parcela) * qtd_aeronaves;

    // aloca a memória compartilhada
    segmento = shmget (IPC_PRIVATE, tamanho_alocado, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    
    // aloca memoria para o vetor de PID
    if ((lpid = (int*)malloc(sizeof(int)*qtd_aeronaves)) == NULL){
        exit(-1); 
    }

    // ignora o retorno dos filhos      APAGAR!!!!
    signal(SIGCHLD, SIG_IGN);

    for (i = 0; i < qtd_aeronaves; i++){
        if ((pid = fork()) < 0){
            exit(-2);
        }        

        if(pid == 0){ // filho
            kill(getpid(), SIGSTOP);  // Filho se pausa logo no início

            // associa a memória compartilhada ao processo
            p = (Parcela *) shmat (segmento, 0, 0);
            signal(SIGALRM, trata_alarme);
            signal(SIGUSR1, tratador1);
            signal(SIGUSR2, tratador2);
            cria_aeronave();
            calcula_reta();
            atualizar_memoria();
            comeca();
            while(aeronave.tempo_voo != 10) pause();
        }
        else{
            lpid[i] = pid;
        }
    }

    // associa a memória compartilhada ao processo
    p = (Parcela *) shmat (segmento, 0, 0);
    int cont = 60;
    
    inicializa_lista_pausas();
    
    // round robin
    while(cont-- != 0){
        for(i = 0; i < qtd_aeronaves; i++){
            if (kill(lpid[i],0) == 0){        // verifica se filho está vivo
                trocou_pista = 0;             // prepara o radar
                radar();
                // filho acorda
                kill(lpid[i], SIGCONT);
                sleep(1);
                // filho dorme
                kill(lpid[i], SIGSTOP);
            }    
        }         
    }
    for(i = 0; i < qtd_aeronaves; i++){
        kill(lpid[i], SIGKILL);
    }
    shmdt(p);
    shmctl(segmento, IPC_RMID, NULL); 



    return 0;
}
