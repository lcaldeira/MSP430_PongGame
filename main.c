/*
 *	Projeto desenvolvido por alunos do curso de graduação Engenharia de 
 * Computação, cursando a matéria de Sistemas Microcontrolados na 
 *	Universidadde Tecnológica Federal do Paraná - Campus Pato Branco.
 *
 * Integrantes:
 *		Lucas Caldeira de Oliveira;
 *		Mário Alexandre Rodrigues;
 *		Diogo Filipe Micali Mota;
 *		Tatiany Keiko Mori;
 *		Thiago H. Adami.
 *	
 * Descrição:
 *		Um jogo de pong usando matrizes de led 8x8 com o CI MAX7219,
 *		potenciômetros e chaves para interação, displays de 7 segmentos
 *		para exibição do placar e o microcontrolador MSP430g2553.
 *
 *		Toda a lógica foi desenvolvida para que seja de fácil expansão,
 *		sendo preciso apenas conectar mais matrizes de led e alterar o valor
 *		das constantes referentes à quantidade de matrizes em X e Y
 *
 *
 */ 
#include "msp430.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

// Registradores MAX7219
#define MAX_NOOP        0x00 //Modo sem operacao
#define MAX_DIGIT0      0x01 //Coluna 1
#define MAX_DIGIT1      0x02
#define MAX_DIGIT2      0x03
#define MAX_DIGIT3      0x04
#define MAX_DIGIT4      0x05
#define MAX_DIGIT5      0x06
#define MAX_DIGIT6      0x07
#define MAX_DIGIT7      0x08
#define MAX_DECODEMODE  0x09 //Modo BCD
#define MAX_INTENSITY   0x0A //Intensidade
#define MAX_SCANLIMIT   0x0B //Quantidade de Bits por vez
#define MAX_SHUTDOWN    0x0C //Reinicia
#define MAX_DISPLAYTEST 0x0F //Testa a tela

//constantes do jogo
#define RES0    8		//resolução padrão da matriz de led
#define QT_X    2		//quantidade de matrizes na horizontal (eixo X)
#define QT_Y    2		//quantidade de matrizes na vertical (eixo Y)

const char QT_M = QT_X * QT_Y;
const char BAR_H = (QT_Y * RES0)/4 - 1;		//altura da barra
const float SCALE_FACTOR = (RES0 * QT_Y - BAR_H) / 1024.0;	//fator de escala da leitura do ADC

typedef struct{
    int x, y;
} point2d;

point2d bounds;
point2d pos_bar1;
point2d pos_bar2;
point2d pos_ball;
point2d vel_ball;

char game_stopped;
char countdown;
char pts1, pts2;
char aux1, aux2;
char level;

unsigned char i, j, k, idx;
unsigned int val_adc_ch0 = 0, val_adc_ch1 = 0;

void game_init(void);
void game_start(void);
void game_update(void);
void game_draw(void);
void game_score(int player);

//Cabeçalho de funções
void ini_uCon();
void ini_P1_P2();
void ini_TA0_PWM_ADC10();
void ini_TA1_CLK_DISP(void);
void ini_ADC10();
void ini_USCI_A0_UART();
void ini_MAX();
void spi_MAX(unsigned char address, unsigned char string);
void spi_MAX_n(unsigned char address, unsigned char string, unsigned char n);

int main(void) {

    ini_uCon();
    ini_P1_P2();
    ini_TA0_PWM_ADC10();
    ini_TA1_CLK_DISP();
    ini_ADC10();
    ini_USCI_A0_UART();
    ini_MAX();

    srand(time(0));
    game_init();

    while (1);
}

void ini_uCon(void){

    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer
    /* Config. do BCS
     *
     * MCLK  -> 16 MHz
     * SMCLK ->  8 MHz
     * ACLK  -> 32768 Hz
     *
     * VLO - Nao utilizado
     * XT2 - Nao esta presente
     * LFXT1 - xtal 32k
     * DCO ~ 16 MHz
     */
    DCOCTL = CALDCO_16MHZ;
    BCSCTL1 = CALBC1_16MHZ;
    BCSCTL2 = DIVS0;
    BCSCTL3 = XCAP0 + XCAP1;

    while(BCSCTL3 & LFXT1OF);
    __enable_interrupt();
}

void ini_P1_P2(void){
    /* 	Configuração das portas
     *
     *      P1.0: entrada do potenciômetro 1
     *      P1.1: entrada do potenciômetro 2
     *      P1.2: DIN
     *      P1.3: CS
     *      P1.4: CLK
     *      P1.5: chave PLAY/PAUSE
     *      P1.6: saída A pro conversor BCD
     *      P1.7: saída B pro conversor BCD
     *
     *      P2.0: chave para seleção de dificuldade (resistor pull-up)
     *      P2.1: ativação do display 7-seg #1
     *      P2.2: ativação do display 7-seg #2
     *      P2.3:
     *      P2.4: saída C pro conversor BCD
     *      P2.5: saída D pro conversor BCD
     *      P2.6:
     *      P2.7:
     */
    P1DIR = ~(BIT0 + BIT1 + BIT5);
    P1OUT = BIT5;
    P1REN = BIT5;
    P1SEL |= BIT0 + BIT1 + BIT2 + BIT4;
    P1SEL2 |= BIT2 + BIT4;
    P1IES = 0;
    P1IFG &= ~BIT5;
    P1IE |= BIT5;

    P2DIR = ~(BIT0);
    P2OUT = BIT0 + BIT1;  //ativação do display 7-seg #1 primeiro + pull-up
    P2REN = BIT0;
    P2IES = 0;
    P2IFG = 0;
    P2IE = BIT0;
}

void ini_TA0_PWM_ADC10(void) {
    /* PWM para o ADC10
     *
     * CONTADOR
     *      - Modo: UP
     *      - Clock: ACLK = 32768 Hz
     *
     * MODULO 0
     *      - Periodo: 50ms
     *      - TA0CCR0 = 1637
     *
     * MODULO 1
     *      - Modo PWM: Set/Reset (7)
     *      - Razao Ciclica: 50%
     *      - TA0CCR1 = 818;
     */
    TA0CTL = TASSEL0;
    TA0CCTL1 = OUTMOD0 + OUTMOD1 + OUTMOD2 + OUT;
    TA0CCR0 = 1637;
    TA0CCR1 = 818;
}

void ini_TA1_CLK_DISP(void){
    /* TIMER PARA PLACAR E GAME-LOOP
     *
     * CONTADOR
     *      - Modo: UP
     *      - Clock: SMCLK = 8/8 = 1 MHz
     *
     * MODULO 0
     *      - Periodo: 10ms
     *      - TA1CCR0: = 10m * (8M/8) - 1 = 9999
     *      - Frequência do jogo: entre 160ms e 80ms
     */
    TA1CTL = TASSEL1 + ID0 + ID1;
    TA1CCTL0 = CCIE;
    TA1CCR0 = 9999;     //frequência de atualização do placar  = 10 ms
}

void ini_ADC10(void){
    /* ADC modo: sequencia de canais (A1 -> A0)
     * Gatilho por Hardware via Mod. 1 do Timer0
     * Clock: ADC10OSC - fadc -> 6,3 MHz
     *
     * Canais de entrada: A0 e A1
     * Impedancia: Pot. de 10 kohms
     *
     */

    ADC10CTL0 = SREF0 + ADC10SHT0 + ADC10SHT1 + MSC + REF2_5V + REFON + ADC10ON + ADC10IE;
    ADC10CTL1 = INCH0 + SHS0 + CONSEQ0;
    ADC10AE0 = BIT0 + BIT1;
    ADC10CTL0 |= ENC;
}

void ini_USCI_A0_UART(void){
    UCA0CTL1 |= UCSWRST;        // USCI in Reset State (for config)
    // Leading edge + MSB first + Master + Sync mode (spi)
    UCA0CTL0 = UCCKPH + UCMSB + UCMST + UCSYNC;
    UCA0CTL1 |= UCSSEL_2;       // SMCLK as clock source
    UCA0BR0 |= 0x01;        // SPI speed (same as SMCLK)
    UCA0BR1 = 0;
    UCA0CTL1 &= ~UCSWRST;       // Clear USCI Reset State;
}

void ini_MAX () {
    // Inicialização do CI MAX7219 CI PARA MATRIZ DE LEDS
    spi_MAX_n(MAX_NOOP,         0x00, QT_M); // operação, dado
    spi_MAX_n(MAX_SCANLIMIT,    0x07, QT_M); // quantos bits por vez
    spi_MAX_n(MAX_INTENSITY,    0x00, QT_M); // não brilhar
    spi_MAX_n(MAX_DECODEMODE,   0x00, QT_M);
    spi_MAX_n(MAX_DIGIT0,       0x00, QT_M); // zera os registradores (apaga os leds)
    spi_MAX_n(MAX_DIGIT1,       0x00, QT_M);
    spi_MAX_n(MAX_DIGIT2,       0x00, QT_M);
    spi_MAX_n(MAX_DIGIT3,       0x00, QT_M);
    spi_MAX_n(MAX_DIGIT4,       0x00, QT_M);
    spi_MAX_n(MAX_DIGIT5,       0x00, QT_M);
    spi_MAX_n(MAX_DIGIT6,       0x00, QT_M);
    spi_MAX_n(MAX_DIGIT7,       0x00, QT_M);
    spi_MAX_n(MAX_SHUTDOWN,     0x01, QT_M);
}

void spi_MAX(unsigned char address, unsigned char string) {
    /* o SPI envia 16bits, sendo que:
     *      -> os 4 iniciais são ignorados,
     *      -> os 4 seguintes são de comando,
     *      -> os últimos 8 sao dados.
     *  A cada byte enviado, o byte anteriormente armazenado é passado para frente
     *  Após cont=QT_M bytes, um pulso de LOAD é disparado e carrega tudo nos registradores
     */
    static int cont = 0;

    UCA0TXBUF = address; //recebe address qual led vai passar , oito bits que preenche a matriz
    while (UCA0STAT & UCBUSY); //aguardando envio dos dados
    UCA0TXBUF = string; //passa os dados pra a tela
    while (UCA0STAT & UCBUSY);

    cont++;

    //pulso LOAD (CS)
    if(cont == QT_M){
        P1OUT |=  BIT3; // borda de subida
        P1OUT &= ~BIT3; // borda de descida
        cont = 0;
    }
}

void spi_MAX_n(unsigned char address, unsigned char string, unsigned char n) {
    //envio de n bytes sequenciais
    for(k=0; k<n; k++)
        spi_MAX(address, string);
}

#pragma vector=ADC10_VECTOR
__interrupt void RTI_ADC10(void){
    //leitura dos potenciômetros
    ADC10CTL0 &= ~ENC;

    switch(ADC10CTL1 & (INCH0 + INCH1)){
    case INCH0:
        val_adc_ch0 = (ADC10MEM >= 64 ? ADC10MEM - 64 : 0);
        break;
    case INCH1:
        val_adc_ch1 = (ADC10MEM >= 64 ? ADC10MEM - 64 : 0);
        break;
    }

    //troca de canais e habilitação de nova leitura
    ADC10CTL1 ^= INCH0 + INCH1;
    ADC10CTL0 |= ENC;
}

#pragma vector=TIMER1_A0_VECTOR
__interrupt void RTI_T1_A0_GAME_LOOP(void){
    static char cont_loop = 0;

    //debouncer das chaves
    P1IE |= BIT5;
    P2IE |= BIT0;

    P2OUT ^= (BIT1 + BIT2); //troca de display 7-seg
    if(P2OUT & BIT1){
        //exibe pts1
        P1OUT &= ~(BIT6 + BIT7);
        P1OUT |= (pts1 & BIT0) << 6;    //P1.6 (pino A)
        P1OUT |= (pts1 & BIT1) << 6;    //P1.7 (pino B)

        P2OUT &= ~(BIT4 + BIT5);
        P2OUT |= (pts1 & BIT2) << 2;    //P2.4 (pino C)
        P2OUT |= (pts1 & BIT3) << 2;    //P2.5 (pino D)
    }
    else if(P2OUT & BIT2){
        //exibe pts2
        P1OUT &= ~(BIT6 + BIT7);
        P1OUT |= (pts2 & BIT0) << 6;    //P1.6 (pino A)
        P1OUT |= (pts2 & BIT1) << 6;    //P1.7 (pino B)

        P2OUT &= ~(BIT4 + BIT5);
        P2OUT |= (pts2 & BIT2) << 2;    //P2.4 (pino C)
        P2OUT |= (pts2 & BIT3) << 2;    //P2.5 (pino D)
    }

    cont_loop++;

    if(cont_loop >= (QT_X*RES0) - (4*level)){
        //aguarda o fim da conversão
        while(ADC10CTL0 & ADC10BUSY == ADC10BUSY);
        //loop principal do jogo
        game_update();
        game_draw();
        cont_loop = 0;
    }
}

#pragma vector=PORT1_VECTOR
__interrupt void RTI_P1(void) {
    P1IE &= ~(BIT5);

    if(P1IFG & BIT5)
        game_stopped = !game_stopped;

    P1IFG &= ~(BIT5);
}

#pragma vector=PORT2_VECTOR
__interrupt void RTI_P2(void) {
    P2IE &= ~(BIT0);

    if(P2IFG & BIT0)
        level = (level < 2 ? level+1 : 0);

    P2IFG &= ~(BIT0);
}

void game_init(void){
    /*
     *  ESPECIFICAÇÕES DO JOGO:
     *  -> constantes pré-definidas
     *          # RES0 = resolução da matriz de leds
     *          # QT_X = quantidade de matrizes na horizontal
     *          # QT_Y = quantidade de matrizes na vertical
     *                      a velocidade que a bola cruza a tela na horizontal
     *  -> dimensões do jogo = [QT_X * RES0] X [QT_Y * RES0]
     *          # o limite inferior corresponte ao ponto (0,0)
     *          # o limite superior corresponte ao ponto (QT_X*RES0-1,QT_Y*RES0-1)
     *   -> altura das barras = [QT_Y * RES0]/4-1
     *   -> bola de lado unitario (1 led)
     *
     *    0_______________________________bounds.x
     *    |                               |
     *    |                               |
     *    v                               v
     *
     *    +---------------+---------------+  <----bounds.y
     *   F|               |               |       |
     *   E|               |               |       |
     *   D|               |               |       |
     *   C|               |               |       |
     *   B|               |               |       |
     *   A|               |               |       |
     *   9| []            |               |       |
     *   8| []            |               |       |
     *    +---------------+---------------+       |
     *   7| []            |            [] |       |
     *   6| []            |            [] |       |
     *   5|            o  |            [] |       |
     *   4|               |            [] |       |
     *   3|               |               |       |
     *   2|               |               |       |
     *   1|               |               |       |
     *   0|               |               |       |
     *    +---------------|---------------+  <----0
     *     0 1 2 3 4 5 6 7 8 9 A B C D E F
     *
     */
    bounds = (point2d) { QT_X*RES0 - 1 , QT_Y*RES0 - 1 };
    pts1 = pts2 = 0;
    level = 0;
    game_stopped = true;	//a partida sempre inicia pausada
    game_start();

    TA0CTL |= MC0;
    TA1CTL |= MC0;
}

void game_start(void){
    //sortear valores na faixa {-1,0,+1} para a velocidade
    vel_ball.x = vel_ball.y = 0;
    while(vel_ball.x == 0)
        vel_ball.x = rand()%3 - 1;
    while(vel_ball.y == 0)
        vel_ball.y = rand()%3 - 1;
    //iniciar a posição dos objetos
    pos_ball = (point2d) {(bounds.x+1)/2 , (bounds.y+1)/2};
    pos_ball.y += rand()%(RES0 - BAR_H) - (BAR_H-1);
    pos_bar1 = (point2d) {0x0      , (int) (val_adc_ch0 * SCALE_FACTOR)};
    pos_bar2 = (point2d) {bounds.x , (int) (val_adc_ch1 * SCALE_FACTOR)};
    //contador de início de jogo (1 segundo)
    countdown = QT_X * RES0;
}

void game_score(int player){
    vel_ball.x = vel_ball.y = 0;
    pos_ball.x = pos_ball.y = 0;

    if(player == 1)
        pts1++;
    else
        pts2++;

	 //se alguém passar de 9 pontos, resetar a partida
    if(pts1 > 9 || pts2 > 9){
        pts1 = pts2 = 0;
        level = 0;
    	  game_stopped = true;
    }

    game_start();
}

void game_update(void){
    //////////////////////
    ////    UPDATE    ////
    //////////////////////
    if(game_stopped)
        return;

    pos_bar1.y = (int) (val_adc_ch0 * SCALE_FACTOR);
    pos_bar2.y = (int) (val_adc_ch1 * SCALE_FACTOR);

    if(countdown){
        countdown--;
        return;
    }

    pos_ball.x += vel_ball.x;
    pos_ball.y += vel_ball.y;

    //checar colisão em cima e embaixo
    if(pos_ball.y < 0 || pos_ball.y > bounds.y){
        vel_ball.y = -vel_ball.y;
        pos_ball.y += vel_ball.y * 2;
    }

    //checar colisão com a barra 1 e parede da esquerda
    if(pos_ball.x <= pos_bar1.x){
        if(pos_ball.y >= pos_bar1.y && pos_ball.y <= pos_bar1.y + BAR_H){
            vel_ball.x = -vel_ball.x;
            pos_ball.x += vel_ball.x * 2;
            //quina da barra: inverter velocidade vertical
            if(pos_ball.y == pos_bar1.y && vel_ball.y > 0){
                vel_ball.y = -vel_ball.y;
                pos_ball.y += vel_ball.y * 2;
            }
            else if(pos_ball.y == pos_bar1.y + BAR_H && vel_ball.y < 0){
                vel_ball.y = -vel_ball.y;
                pos_ball.y += vel_ball.y * 2;
            }
        }
        else if(pos_ball.x < 0)
            game_score(2);
    }
    //checar colisão com a barra 2 e parede da direita
    else if(pos_ball.x >= pos_bar2.x){
        if(pos_ball.y >= pos_bar2.y && pos_ball.y <= pos_bar2.y + BAR_H){
            vel_ball.x = -vel_ball.x;
            pos_ball.x += vel_ball.x * 2;
            //quina da barra: inverter velocidade vertical
            if(pos_ball.y == pos_bar2.y && vel_ball.y > 0){
                vel_ball.y = -vel_ball.y;
                pos_ball.y += vel_ball.y * 2;
            }
            else if(pos_ball.y == pos_bar2.y + BAR_H && vel_ball.y < 0){
                vel_ball.y = -vel_ball.y;
                pos_ball.y += vel_ball.y * 2;
            }
        }
        else if(pos_ball.x > bounds.x)
            game_score(1);
    }
}

void game_draw(){
    //////////////////////
    ////     DRAW     ////
    //////////////////////

    /*
     *      Diagrama Genérico com várias matrizes
     *          > para efeitos de modelagem: QT_X = 4 e QT_Y = 3;
     *          > coordenadas da barra 1: (00,04)
     *          > coordenadas da barra 2: (31,16)
     *          > altura das barras: BAR_H = 6-1 = 5
     *
     *      +---+---+---+---+
     *      | 8 | 9 | A | B |
     *      +---+---+---+---+
     *      | 4 | 5 | 6 | 7 |
     *      +---+---+---+---+
     *      | 0 | 1 | 2 | 3 |
     *      +---+---+---+---+
     *
     *	   A ordem de envio dos dados através do loop
	  *
	  *			 for(i = 0; i < QT_M; i++)
	  *				  spi_MAX(addr[i], data[i]);
	  *		
	  *		depende da ordem de montagem das matrizes,
	  *		sendo a expressão acima considerando que a
	  *		última matriz do diagrama está conectada ao
	  *		micro e a a primeira do diagrama está na 
	  *		extremidade do encadeamento em série.
     *
     */
    static char data[QT_M];
    static char addr[QT_M];

    //limpeza das colunas da matriz
    for(i=MAX_DIGIT0; i <= MAX_DIGIT7; i++)
        spi_MAX_n(i, 0x00, QT_M);

    //efeito blink
    if(countdown > 0 && (countdown/4)%2 == 0)
        return;

    //barra #1
    for(i=0; i < QT_X; i++)
        for(j=0; j < QT_Y; j++){
            //valores auxiliares
            aux1 = RES0 * (j);
            aux2 = RES0 * (j+1);
            idx = i + QT_X*j;
            //zerando os bits de dados e enviando para NOOP
            addr[idx] = MAX_NOOP;
            data[idx] = 0x00;

            //checando posição vertical da barra 1
            //(na primeira coluna de matrizes)
            if(i == 0){
                addr[idx] = MAX_DIGIT0;

                //metade de baixo da barra
                if(pos_bar1.y >= aux1 && pos_bar1.y < aux2)
                    for(k = pos_bar1.y; (k < aux2 && k <= pos_bar1.y+BAR_H); k++)
                        data[idx] += BIT0 << (k - aux1);
                //metade de cima da barra
                else if(pos_bar1.y+BAR_H >= aux1 && pos_bar1.y+BAR_H < aux2)
                    for(k = aux1; k <= pos_bar1.y+BAR_H; k++)
                        data[idx] += BIT0 << (k - aux1);

                //checando sobreposição da bola na primeira coluna
                if(pos_ball.x == 0 && pos_ball.y >= aux1 && pos_ball.y < aux2)
                    data[idx] |= BIT0 << (pos_ball.y - aux1);
            }
        }

    for(i = 0; i < QT_M; i++)
        spi_MAX(addr[i], data[i]);

    //barra #2
    for(i=0; i < QT_X; i++)
        for(j=0; j < QT_Y; j++){
            //valores auxiliares
            aux1 = RES0 * (j);
            aux2 = RES0 * (j+1);
            idx = i + QT_X*j;
            //zerando os bits de dados e enviando para NOOP
            addr[idx] = MAX_NOOP;
            data[idx] = 0x00;

            //checando posição vertical da barra 1
            //(na primeira coluna de matrizes)
            if(i == QT_X-1){
                addr[idx] = MAX_DIGIT7;

                //metade de baixo da barra
                if(pos_bar2.y >= aux1 && pos_bar2.y < aux2)
                    for(k = pos_bar2.y; (k < aux2 && k <= pos_bar2.y+BAR_H); k++)
                        data[idx] += BIT0 << (k - aux1);
                //metade de cima da barra
                else if(pos_bar2.y+BAR_H >= aux1 && pos_bar2.y+BAR_H < aux2)
                    for(k = aux1; k <= pos_bar2.y+BAR_H; k++)
                        data[idx] += BIT0 << (k - aux1);

                //checando sobreposição da bola na última coluna
                if(pos_ball.x == bounds.x && pos_ball.y >= aux1 && pos_ball.y < aux2)
                    data[idx] |= BIT0 << (pos_ball.y - aux1);
            }
        }

    for(i = 0; i < QT_M; i++)
        spi_MAX(addr[i], data[i]);

    //bolinha
    if(pos_ball.x <= 0 || pos_ball.x >= bounds.x)
        return;

    for(i=0; i < QT_X; i++)
        for(j=0; j < QT_Y; j++){
            //valores auxiliares
            aux1 = RES0 * (j);
            aux2 = RES0 * (j+1);
            idx = i + QT_X*j;
            //zerando os bits de dados e enviando para NOOP
            addr[idx] = MAX_NOOP;
            data[idx] = 0x00;
            //checando posição vertical da barra 1
            if(pos_ball.y >= aux1 && pos_ball.y < aux2){
                aux1 = RES0 * (i);
                aux2 = RES0 * (i+1);
                if(pos_ball.x >= aux1 && pos_ball.x < aux2){
                    addr[idx] = MAX_DIGIT0 + (pos_ball.x-aux1);
                    data[idx] = BIT0 << (pos_ball.y - RES0*j);
                }
            }
        }

    for(i = 0; i < QT_M; i++)
        spi_MAX(addr[i], data[i]);
}
