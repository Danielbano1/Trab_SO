#include<stdio.h>
#include<stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

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
    int status;
    int lado_entrada;
    int pista;
    float atraso;
    float velocidade;
    float tempo_voo;
    float tempo_total;
    Coordenada entrada;
    Coordenada atual;
    Radar radar;
}Aeronave;



int main(void){
    Aeronave aeronave;


    return 0;
}