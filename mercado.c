#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "node-id.h"

#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "simple-udp.h"


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

#define MY_IP "65152,0,0,0,530,29700,4,1028,"

#define SERVER_NODE(ipaddr)   uip_ip6addr(&ipaddr, 0xfe80, 0, 0, 0, 0x0212, 0x7402, 0x0002, 0x0202) /* cooja2 */

#define LOCAL_PORT      UIP_HTONS(COAP_DEFAULT_PORT+1)
#define REMOTE_PORT UIP_HTONS(COAP_DEFAULT_PORT)

#define REST_PUT_BROADCAST(ip,url, handler, corpo, atr) coap_init_message (requisicao, COAP_TYPE_CON, COAP_PUT, 0); \
                                                        coap_set_header_uri_path (requisicao, url); \
                                                        coap_set_header_content_type (requisicao, APPLICATION_XML);\
                                                        coap_set_header_uri_query (requisicao, atr);\
                                                        coap_set_payload (requisicao, (uint8_t *) corpo, sizeof (corpo)-1); /* coap_set_payload (requisicao, (char *) corpo, strlen (corpo)); */ \
                                                        COAP_BLOCKING_REQUEST (ip, REMOTE_PORT, requisicao, handler)

#define REST_GET(ip, url, handler, corpo, atr) coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0); \
                                               coap_set_header_uri_path(request, url); \
                                               coap_set_header_uri_query(request, atr); \
                                               coap_set_header_content_type(request, APPLICATION_XML); \
                                               coap_set_payload(request, (uint8_t *)corpo, sizeof(corpo)-1); \
                                               COAP_BLOCKING_REQUEST(ip, REMOTE_PORT, request, handler)

int 
  servicoDescoberto,
  mercadoRecebido;

size_t 
  tamanho = 0;

uint8_t 
  *conteudo;

char 
  msg [100],
  codigoSupermercado [100]; // O tamanho maximo dos dados e de 64 bytes!!!

static coap_packet_t
  request [1],
  requisicao [1];

struct etimer 
  tempo;

static uip_ipaddr_t 
  addr;

// Manipula as respostas do servidor de melhor preco para testar o vocabulario
void discovery_vocabulario_handler (void *response)
{
  printf ("[GELADEIRA] (discovery_vocabulario) Recebeu resposta de servidor de melhor preco.\n");
  
  uint16_t 
      payload_len = 0;
    
    uint8_t* 
      payload = NULL;
    
    char
      temp [100];
      
    payload_len = coap_get_payload(response, &payload);

      if (payload) 
      {
        memcpy(temp, payload, payload_len);
        
        temp[payload_len] = 0;
        
        printf("[MERCADO] (discovery_vocabulario) payload: %s\n", temp);
      }
  
  // Se receber resposta do servidor de melhor preco, pode modificar o flag
  servicoDescoberto = 1;
  
}

void broadcast_handler (void *response)
{
  uint16_t 
    payload_len = 0;
  
  uint8_t* 
    payload = NULL;
  
  char
    temp [100];
    
  payload_len = coap_get_payload(response, &payload);

    if (payload) 
    {
      memcpy(temp, payload, payload_len);
      
      temp[payload_len] = 0;
      
      printf("[MERCADO] (broadcast_handler) payload: %s\n", temp);
    }
  
  
  // Nao importa o conteudo da resposta!!!
  // Se chegar uma resposta, o mercado ja foi inserido em melhor preco.
  // Essa resposta faz com que o mercado pare de enviar anuncio de servico 
  // ao servidor de menor preco
  mercadoRecebido = 1;
}

RESOURCE (consulta, METHOD_GET, "mercado/consulta", "title=\"consulta?item=...\";rt=\"Text\"");

void consulta_handler (void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const char 
    *codigo_produto = NULL;
  
  const char 
    *message = "<prd><cod>";
  
  if (REST.get_query_variable (request, "item", &codigo_produto)) 
  {
  	strcat((char *)message, codigo_produto);
  
  	strcat((char *)message, "</cod><pr>37</pr></prd>");
    
  	memcpy(buffer, message, strlen(message));
  } 

  REST.set_header_content_type (response, APPLICATION_XML); 
  
  REST.set_response_payload (response, buffer, strlen(message));
}

PROCESS (mercado, "Mercado");

AUTOSTART_PROCESSES (&mercado);

PROCESS_THREAD (mercado, ev, data)
{

  PROCESS_BEGIN ();
  
  // Flag de mercado recebido
  mercadoRecebido = 0;
  
  // Flag de servico descoberto (teste do vocabulario)
  servicoDescoberto = 0;
  
  // A cada 30 segundos realiza uma compra
  etimer_set (&tempo, 30 * CLOCK_SECOND);
  
  printf ("[MERCADO] (principal) Supermercado na rede.\n");
  
  // Define o codigo do supermercado como sendo o numero do no atribuido pelo contiki
  sprintf (codigoSupermercado, "?cod=%d", node_id);
  
  //printf ("[MERCADO] (principal) node-id: %s.\n", codigoSupermercado);
  
  // Inicia a engine de REST
  rest_init_engine ();
  
  // Ativa o recurso de consulta de preco via REST
  rest_activate_resource (&resource_consulta);
  
  // Atribui o endereco IPv6 do servidor de melhor preco
  SERVER_NODE (addr);
    
  while (1) 
  {
    PROCESS_WAIT_EVENT (); 
  
    if (etimer_expired (&tempo)) 
    {
      // Se o servico nao estiver descoberto, envia GET para discovery do servidor de melhor preco
            if (!servicoDescoberto)
            {
              // Exibe mensagem de consulta de vocabulario
              printf ("[MERCADO] (principal) Solicitando servicos - teste de vocabulario.\n");
              
              // Faz GET no servidor de menor preco para testar o vocabulario
              REST_GET(&addr, "discovery", discovery_vocabulario_handler, "SERVICES", " ");
              
            }
      
      
        if (!mercadoRecebido && servicoDescoberto) // So envia ao menor preco se ele nao enviar a resposta com reconhecimento
        {
          printf ("[MERCADO] (principal) envia IP e codigo a menor preco.\n");
          
          REST_PUT_BROADCAST (&addr, "server/mercado/discovery", broadcast_handler, MY_IP, codigoSupermercado);
        }
        
    	  etimer_reset (&tempo);
    }
  }
  
  PROCESS_END ();
}
