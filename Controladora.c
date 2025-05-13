#define aeroporto 0.5
#define tempo 10

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

void atualizar_memoria(){
    Parcela parcela;
    parcela.c_atual = aeronave.c_atual;
    parcela.pista = aeronave.pista;
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
        printf("Chegou!\n");
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
// verifica provaveis colisoes
void radar(){
    int pista = p[i].pista;
    if (trocou_pista == 1){     // troca a pista localmente
        if (pista == 0)
            pista = 1;
        else
            pista = 0;
    }
    Coordenada c = p[i].c_atual;
    for(int s = 0; s < qtd_aeronaves; s++){
        if (s != i){                            // verifica se nao eh ele mesmo
            if (p[s].pista == pista){           // verifica se sao vizinhos de quadrante 
                if (kill(lpid[s], 0) == 0){     // verifica se vizinho esta vivo
                    if (p[s].status == 1){      // verifica se o vizinho esta em movimento
                        if (((fabsf(p[s].c_atual.x - c.x)) < 0.1) && ((fabsf(p[s].c_atual.y - c.y)) < 0.1)){
                            printf("pid: %d, pista: %d, x: %.2f, y: %.2f\n", lpid[i], pista, c.x, c.y);
                            printf("pid: %d morreu\n", lpid[i]);
                            kill(lpid[i], SIGKILL); // pouso
                            return;
                        }
                        if (((fabsf(p[s].c_atual.x - c.x)) <= 0.2) && ((fabsf(p[s].c_atual.y - c.y)) <= 0.2)){
                            if (trocou_pista == 0){
                                kill(lpid[i], SIGUSR2); // troca pista
                                trocou_pista = 1;
                                printf("pid: %d trocou pista\n", lpid[i]);
                                radar();
                            }
                            else{
                                printf("pid: %d pausei\n", lpid[i]);
                                kill(lpid[i], SIGUSR1); // pausa
                            }
                            return;
                        }
                    }
                }
            }
        }
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
    int t = 40;
    
    // round robin
    while(cont-- != 0){
        for(i = 0; i < qtd_aeronaves; i++){
            if (kill(lpid[i],0) == 0){        // verifica se filho está vivo
                trocou_pista = 0;             // prepara o radar
                if(t < 0)
                    radar();
                t--;
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