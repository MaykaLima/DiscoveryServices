#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"

#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "simple-udp.h"


//#include "powertrace.h"

#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET) && !defined (CONTIKI_TARGET_NATIVE)
#warning "Compiling with static routing!"
#include "static-routing.h"
#endif

#include "erbium.h"

#include "dev/button-sensor.h"

#if WITH_COAP == 3
#include "er-coap-03-engine.h"
#elif WITH_COAP == 6
#include "er-coap-06-engine.h"
#elif WITH_COAP == 7
#include "er-coap-07-engine.h"
#else
#error "CoAP version defined by WITH_COAP not implemented"
#endif

#define SERVER_NODE(ipaddr)   uip_ip6addr(&ipaddr, 0xfe80, 0, 0, 0, 0x0212, 0x7402, 0x0002, 0x0202) /* cooja2 */

#define LOCAL_PORT      UIP_HTONS(COAP_DEFAULT_PORT+1)
#define REMOTE_PORT     UIP_HTONS(COAP_DEFAULT_PORT)

#define MY_IP "65152,0,0,0,530,29699,3,771,"

#define LER_IP_PARTE(str)  i_l = 0; \
                           while (str [comeco] != ',') \
                           { \
                             ip_parte [i_l++] = str[comeco++]; \
                           } \ 
                           ip_parte [i_l] = 0; \
                           comeco++

#define LER_IP(str, inic) int comeco = inic, j, i_l; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_a[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_b[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_c[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_d[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_e[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_f[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_g[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_h[j] = ip_parte[j]; \
                          i_l = comeco
                           
#define REST_PUT_BROADCAST(ip, url, handler, corpo, atr) coap_init_message(request, COAP_TYPE_CON, COAP_PUT, 0); \
                                                         coap_set_header_uri_path(request, url); \
                                                         coap_set_header_uri_query(request, atr); \
                                                         coap_set_header_content_type(request, APPLICATION_XML); \
                                                         coap_set_payload(request, (uint8_t *)corpo, sizeof(corpo)-1); \
                                                         COAP_BLOCKING_REQUEST(ip, REMOTE_PORT, request, handler)

#define REST_GET(ip, url, handler, corpo, atr) coap_init_message(requisicao, COAP_TYPE_CON, COAP_GET, 0); \
                                               coap_set_header_uri_path(requisicao, url); \
                                               coap_set_header_uri_query(requisicao, atr); \
                                               coap_set_header_content_type(requisicao, APPLICATION_XML); \
                                               coap_set_payload(requisicao, (uint8_t *)corpo, sizeof(corpo)-1); \
                                               COAP_BLOCKING_REQUEST(ip, REMOTE_PORT, requisicao, handler)

//#define COMPRA "<compra><cliente>900</cliente><item>112</item><quantidade>2</quantidade></compra>"

#define COMPRA "<cp><cli>900</cli><item>112</item><qtd>2</qtd></cp>"                           
                         


static uip_ipaddr_t
  addr;

static struct etimer 
  tempo;

static int  
  servicoDescoberto,
  compraEfetuada;

uint8_t 
  *conteudo;

char 
  ip_a [10], 
  ip_b [10], 
  ip_c [10], 
  ip_d [10], 
  ip_e [10], 
  ip_f [10], 
  ip_g [10], 
  ip_h [10], 
  ip_parte [10];

static coap_packet_t 
  requisicao [1],
  request [1];

// Manipula as respostas do servidor de melhor preco para testar o vocabulario
void discovery_vocabulario_handler (void *response)
{
  printf ("[GELADEIRA] (discovery_vocabulario) Recebeu resposta de servidor de melhor preco.\n");
  
  // Se receber resposta do servidor de melhor preco, pode modificar o flag
  servicoDescoberto = 1;
  
}

// Funcao para manipular uma compra (resposta de um servidor de melhor compra)
// O objetivo dessa funcao e efetuar uma compra. Isso e feito quando um valor
// e retornado do servidor de melhor compra.
void compra_handler (void *response)
{
  uint8_t
      *chunk = NULL;
  uint16_t
      len = 0;
  char 
    msg [100];
  
  //printf ("[GELADEIRA] (compra_handler) Entrou!!!\n");
  
   // Recupera os dados da mensagem recebida do servidor de melhor compra
   // len e o comprimento da mensagem
   // response e a mensagem recebida
   // chunk sao os dados da mensagem recebida
   len = coap_get_payload(response, &chunk);
   
   // Se o conteudo da mensagem nao for vazio
   if (chunk) 
   {
     // msg = chunk
     memcpy(msg, chunk, len);
     
     // Finaliza a string msg com \0
     msg[len] = 0;
     
     //printf ("[GELADEIRA] (compra_handler) Conteudo da compra: %s\n", msg);
    
    // Converte o numero que esta em msg e armazena em len
    len = atoi(msg);
    
    //printf ("[GELADEIRA] (compra_handler) Valor da compra: %d\n", len);
    
    // Se houver um valor armazenador em len significa que a compra foi efetuada
    if (len) 
    {
      printf ("[GELADEIRA] (compra_handler) Compra efetuada. Valor total: %d\n",len);
    
      compraEfetuada = 1;
    }
    // Caso contrario houve falha na tentativa de compra
    else
    {
    	printf ("[GELADEIRA] (compra_handler) Falha na tentativa de compra.\n"); 
    	
    	compraEfetuada = 0;
    }
   }  
}

// Definicoes da thread principal do no sensor
// Isso e padrao no Contiki
PROCESS (geladeira, "Geladeira");

AUTOSTART_PROCESSES (&geladeira);

// Thread principal do no sensor - TUDO INICIA AQUI EM QUALQUER NO SENSOR
PROCESS_THREAD (geladeira, ev, data)
{
  // Inicio da thread principal
  PROCESS_BEGIN ();

	//powertrace_start (CLOCK_SECOND * 2); // Nao esta funcionando porque nao cabe na memoria do no sensor

  // Inicia a engine REST
  rest_init_engine ();
  
  // Atribui zero a compra efetuada
  compraEfetuada = 0;
  
  // Atribui zero a servico descoberto
  servicoDescoberto = 0;

  // Inicia um timer para executar a cada 10 segundos
  etimer_set (&tempo, 10 * CLOCK_SECOND);
  
  // Atribui o endereco IPv6 do servidor de melhor preco
  SERVER_NODE (addr);
  
  // Laco principal da thread principal
  while (1) {
    
    // Aguarda por um evento, nesse caso receber um endereco IPv6 de um servidor em discover_handler
    PROCESS_WAIT_EVENT();

    // Se o tempo expirou - nesse caso se passou 10 segundos
    if (etimer_expired (&tempo)) 
    {
      // Se o servico nao estiver descoberto, envia GET para discovery do servidor de melhor preco
      if (!servicoDescoberto)
      {
        // Exibe mensagem de consulta de vocabulario
        printf ("[GELADEIRA] (principal) Solicitando servicos - teste de vocabulario.\n");
        
        // Faz GET no servidor de menor preco para testar o vocabulario
        REST_GET(&addr, "discovery", discovery_vocabulario_handler, "SERVICES", " ");
        
      }
      
      // Se a compra nao foi efetuada, efetua a mesma
      if (!compraEfetuada && servicoDescoberto)
      {
        // Exibe mensagem de solicitacao de compra
        printf ("[GELADEIRA] (principal) Solicitando compra.\n");
        
        // Faz um GET no mercado com o menor preco que foi selecionado e usa a funcao compra_handler definida neste arquivo
        REST_GET(&addr, "server/compra", compra_handler, COMPRA, " ");
      }
      
      // Reseta o timer
      etimer_reset (&tempo);
    }
    
  }

  // Fim da thread principal
  PROCESS_END ();
}
