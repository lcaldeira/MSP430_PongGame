<h1 align="center"> :joystick: PongGame no MSP430</h1>

<h6 align="center">Desenvolvido pelos acadêmicos de Engenharia de Computação da UTFPR CAMPUS Pato Branco</h6>

Este projeto se trata de um jogo desenvolvido durante a disciplina de *Sistemas Microcontrolados* utilizando o microcontrolador MSP430. O objetivo do jogo é simples: usar a raquete virtual para passar a bola e pontuar mais que o adversário. Conforme for passando o tempo do jogo maior a velocidade da bolinha. No desenho abaixo pode ser visualizada a ideia do jogo.

    ----------------------  ----------------------  ----------------------
    |                   ||  |                    |  |                     |
    ||o                 ||  |                   ||  |                     |
    ||                   |  ||                  ||  ||               o   ||
    |                    |  ||      o            |  ||                   ||
    ----------------------  ----------------------  ----------------------
 
Integrantes:
 *		Lucas Caldeira de Oliveira;
 *		Mário Alexandre Rodrigues;
 *		Diogo Filipe Micali Mota;
 *		Tatiany Keiko Mori;
 *		Thiago H. Adami.

Descrição:
 *		Um jogo de pong usando matrizes de led 8x8 com o CI MAX7219,
  		potenciômetros e chaves para interação, displays de 7 segmentos
  		para exibição do placar e o microcontrolador MSP430g2553;
 *		Toda a lógica foi desenvolvida para que seja de fácil expansão,
  		sendo preciso apenas conectar mais matrizes de led e alterar o valor
  		das constantes referentes à quantidade de matrizes em X e Y.
