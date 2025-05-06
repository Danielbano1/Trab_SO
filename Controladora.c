#define aeroporto 0.5

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

typedef struct
{
    float x;
    float y;
}Coordenada;


typedef struct
{
    int id;
    float distancia;
}Radar;


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
    Radar radar[];          // id e distancia de possiveis colissoes
}Aeronave;

typedef struct 
{
    int status;
    Coordenada c_atual;
    int pista;
}Parcela;

//Funcoes filho
void cria_aeronave(Aeronave* aeronave){
    // Inicializa a semente do gerador com o tempo atual
    srand(time(NULL));

    aeronave->id = getpid();
    aeronave->status = 0;
    aeronave->lado_entrada = rand() % 2;
    aeronave->pista = rand() % 2;
    aeronave->atraso = (rand() % 21) / 10.0f;
    aeronave->velocidade = 0.05;
    aeronave->tempo_voo = 0;
    aeronave->c_entrada.x = 0;
    aeronave->c_entrada.y = (rand() % 21) * 0.05f;
    aeronave->c_atual = aeronave->c_entrada;  
    aeronave->tempo_total = tempo_estimado(aeronave->c_entrada);
    aeronave->tempo_voo = 0;
}

float tempo_estimado(Coordenada c){
    float cateto1 = aeroporto - c.y;
    float cateto2 = aeroporto - c.x;
    float hipotenusa = sqrt(cateto1*cateto1+cateto2*cateto2);
    return (int)(hipotenusa * 100) / 100.0f;
}

void atualizar_memoria(Aeronave* aeronave, Parcela* p, int posicao){
    Parcela parcela;
    parcela.c_atual = aeronave->c_atual;
    parcela.pista = aeronave->pista;
    parcela.status = aeronave->status;
    
    p[posicao] = parcela;
}


int main(void){
    int segmento, qtd_aeronaves, pid, tamanho_alocado;
    int* p, * lpid;

    // numero de processos filhos
    printf("Quantas aeronaves: \n");
    scanf("%d", &qtd_aeronaves);

    tamanho_alocado = sizeof(Parcela) * qtd_aeronaves;

    // aloca a memória compartilhada
    segmento = shmget (IPC_PRIVATE, tamanho_alocado, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    //if (lpid = (int*)malloc(sizeof(int)*qtd_aeronaves) == NULL){
    //    exit(-1); 
    //}

    // associa a memória compartilhada ao processo
    if( p = (Parcela *) shmat (segmento, 0, 0) == -1){
        exit(-3);
    } 


    for (int i = 0; i < qtd_aeronaves; i++){
        if (pid = fork() == NULL){
            exit(-2);
        }        

        if(pid = 0){ // filho
            // associa a memória compartilhada ao processo
            if( p = (Parcela *) shmat (segmento, 0, 0) == -1){
                exit(-3);
            } 
            Aeronave* aeronave;
            cria_aeronave(aeronave);
            atualizar_memoria(aeronave, p, i);

        }
        else{
            lpid[i] = pid;
        }
    }


    return 0;
}