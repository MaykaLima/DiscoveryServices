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

#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET) && !defined (CONTIKI_TARGET_NATIVE)
#warning "Compiling with static routing!"
#include "static-routing.h"
#endif


#include "erbium.h"

#if WITH_COAP == 3
#include "er-coap-03.h"
#elif WITH_COAP == 7
#include "er-coap-07.h"
#else
#warning "Erbium example without CoAP-specifc functionality"
#endif /* CoAP-specific example */

#if WITH_COAP == 3
#include "er-coap-03-engine.h"
#elif WITH_COAP == 6
#include "er-coap-06-engine.h"
#elif WITH_COAP == 7
#include "er-coap-07-engine.h"
#else
#error "CoAP version defined by WITH_COAP not implemented"
#endif

#define LOCAL_PORT      UIP_HTONS(COAP_DEFAULT_PORT+1)
#define REMOTE_PORT     UIP_HTONS(COAP_DEFAULT_PORT)

#define MAX_MERCADOS 5

#define MY_IP "65152,0,0,0,530,29698,2,514,"


#define REST_PUT_BROADCAST(ip, url, handler, corpo, atr) coap_init_message (requisicao, COAP_TYPE_CON, COAP_PUT, 0); \
                                                          coap_set_header_uri_path (requisicao, url); \
                                                          coap_set_header_uri_query (requisicao, atr); \
                                                          coap_set_header_content_type (requisicao, APPLICATION_XML); \
                                                          coap_set_payload(requisicao, (uint8_t *) corpo, sizeof (corpo)-1); \
                                                          COAP_BLOCKING_REQUEST (ip, REMOTE_PORT, requisicao, handler)

#define REST_GET(ip, url, handler, corpo, atr) coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0); \
                                               coap_set_header_uri_path(request, url); \
                                               coap_set_header_uri_query(request, atr); \
                                               coap_set_header_content_type(request, APPLICATION_XML); \
                                               coap_set_payload(request, (uint8_t *)corpo, sizeof(corpo)-1); \
                                               COAP_BLOCKING_REQUEST(ip, REMOTE_PORT, request, handler)

#define LER_IP_PARTE(str) i_l = 0; \
                           while (str [comeco] != ',') \
                           { \
                             ip_parte [i_l++] = str[comeco++]; \
                           } \ 
                           ip_parte [i_l] = 0; \
                           comeco++

#define LER_IP(str, inic) int comeco = inic, j; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_a[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_b[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_c[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_d[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_e[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_f[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_g[j] = ip_parte[j]; \
                          LER_IP_PARTE(str); for(j=0; j<=i_l; j++) ip_h[j] = ip_parte[j]; \
                          i_l = comeco

struct TYPE {
    int cod;
    uip_ipaddr_t ip;
};

size_t tamanho = 0;

struct TYPE 
  mercados [MAX_MERCADOS];

int  
  aux, 
  i, 
  j, 
  codigo;

static int 
  quantidadeMercados;

uint8_t 
  *conteudo;

static coap_packet_t 
  requisicao [1];

struct etimer 
  tempo;

static char 
  msg [100]; // O tamanho maximo dos dados e de 64 bytes!!!

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

static uip_ipaddr_t 
  addr;



// Monta o conteudo da resposta: o vocabulario com os servicos contidos em menor preco
// Comprimento da mensagem de resposta: 1305 bytes
void montaResposta (char *resposta)
{
  resposta [0] = '\0';
  
  strcat (resposta, "Servico:servicosdisponiveis{\"@context\":{\"ServicoIoT\":\"Discovery\",");
  
  strcat (resposta, "\"dc\":\"http://purl.org/dc/terms/\",\"cc\":\"http://creativecommons.org/ns#\",");
  
  strcat (resposta, "\"title\": \"Discovery\",\"description\":\"listas os servicos disponiveis\",");
  
  strcat (resposta, "\"entrypoint\":{\"@id\":\"ServicoIoT:Entrypoint\",\"@type\": \"@id\"},");
  
  strcat (resposta, "\"CadastraMercado\":\"ServicoIoT:cadastraMercado\",\"CompraProduto\":\"ServicoIoT:compraProduto\",");

  strcat (resposta, "\"@id\":\"Vocabulario para descoberta de servicos de menor preco\",\"@type\":\"Json:vocabulario semantico\",");
  
  strcat (resposta, "\"label\":\"Adaptacao de vocabulario generico de descoberta de servico\",\"preferredprefix\":\"ServicoIoT\",");

  strcat (resposta, "\"dc:description\":\"Vocabulario de teste para o servidor de menor preco\",");
  
  strcat (resposta, "\"defines\":{{\"@id\":\"ServicoIoT:Entrypoint\",\"label\":\"Discovery\",");
  
  strcat (resposta, "\"comment\":\"link para a descoberta de servico\",\"method\":\"GET\"\"return\":\"servicoIoT:Vocabulario\"},");
  
  strcat (resposta, "{\"@id\":\"ServicoIoT:cadastraMercado\",\"label\":\"cadastraMercado\",\"comment\":\"Cadastra mercado e produto\",");
  
  strcat (resposta, "\"method\":\"PUT\",\"parameters\":{\"codigomercado\":\"ServicoIoT:propriedadecodigo\",");
  
  strcat (resposta, "\"codigoProduto\":\"ServicoIoT:propriedadecodigo\",\"quantidade\":\"ServicoIoT:quantidade\",\"valor\":\"servicoIoT:valor\"}},");
  
  strcat (resposta, "{\"@id\":\"ServicoIoT:compraProduto\",\"label\":\"compraProduto\",\"comment\":\"Compra produtos\",");
  
  strcat (resposta, "\"method\":\"GET\",\"parameters\":{\"codigoProduto\":\"ServicoIoT:propriedadecodigo\",\"quantidade\":\"ServicoIoT:quantidade\",}");
  
  strcat (resposta, "\"return\":\"servicoIoT:retorno\",}}}");
}


// Recurso para a descoberta dos servicos contidos no servidor
RESOURCE (discovery, METHOD_GET, "discovery", "");

// Funcao para gerir as requisicoes de GET de descoberta de servico vindas de geladeiras ou supermercados
// A responsabilidade desta funcao e enviar o vocabulario com as definicoes de servico como resposta
void discovery_handler (void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  char 
    resposta [2048]; // String contendo o vocabulario com os servicos contidos em menor preco
  
  printf("[MELHOR PRECO] (discovery_handler) Descoberta de servico recebida.\n");
  
  printf("[MELHOR PRECO] (discovery_handler) Tamanho maximo em bytes da resposta: %d.\n", preferred_size);
  
  // Monta a resposta na respectiva string
  montaResposta (resposta);

  printf("[MELHOR PRECO] (discovery_handler) Resposta enviada: %s.\n", resposta);
  
  printf ("[MELHOR PRECO] (discovery_handler) Tamanho da resposta em bytes: %d\n", strlen (resposta));
  
  // A resposta e copiada para o buffer com o tamanho maximo
  // Caso tente-se usar o tamanho real do vocabulario (1305 bytes), ocorre um erro
  // em tempo de simulacao, por isso a linha com strlen (resposta) esta comentada
  //memcpy(buffer, resposta, strlen (resposta));
  memcpy(buffer, resposta, 64);
  
  printf("[MELHOR PRECO] (discovery_handler) Resposta enviada - buffer: %s.\n", buffer);
  
  // Informa que o conteudo e JSON
  REST.set_header_content_type (response, APPLICATION_JSON); 
    
  // Envia o vocabulario como resposta ao cliente que solicitou a descoberta de servico
  // Tentar enviar os 1305 bytes da resposta gera um erro em tempo de simulacao
  // O maximo de dados que pode ser enviado sao 64 bytes
  //REST.set_response_payload (response, buffer, strlen (resposta));
  REST.set_response_payload (response, buffer, 64);

}


// Recebe anuncio de servico de mercado
// Cada mercado envia endereco IPv6 e codigo
// Isso e armazenado em um vetor de mercados
RESOURCE (discovery_mercado, METHOD_PUT, "server/mercado/discovery", "title=\"cod=...\";rt=\"Discovery\"");

void discovery_mercado_handler (void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  printf("[MELHOR PRECO] (discovery_mercado_handler) Recebeu anuncio de servico de mercado.\n");
  
  if (quantidadeMercados < MAX_MERCADOS)
  {
      int 
        tamanho = coap_get_payload (request, &conteudo);
      
      //printf ("[MELHOR PRECO] (discovery_mercado_handler) Conteudo da requisicao: %s\n", conteudo);
      
      if (conteudo) 
      {
       memcpy (msg, conteudo, tamanho);
      
       msg [tamanho] = 0;
       
       //printf ("[MELHOR PRECO] (discovery_mercado_handler) Conteudo da requisicao: %s\n", conteudo);
      }
      
      const char 
        *queryString = NULL;
      
      codigo = -1;
      
      if (coap_get_query_variable(request, "cod", &queryString))
      {
        char 
          tempCodigo [2];
        
       tempCodigo [0] = queryString[0];
       
       tempCodigo [2] = '\0';
       
       codigo = atoi (tempCodigo);
       
       //printf ("[MELHOR PRECO] (discovery_mercado) Conteudo de queryString: %s\n", queryString);
      }
      
      int
        found,
        temp,
        i_l;
      
      LER_IP(msg,0);
      
      found = 0;
      for (temp = 0; temp < quantidadeMercados; ++ temp)
        if (codigo == mercados [temp].cod) 
          found = 1;
      
      // So insere um mercado se ele ainda nao existir
      if (found == 0)
      { 
        // Insere um mercado no vetor
        uip_ip6addr (&mercados [quantidadeMercados].ip, atoi (ip_a), atoi (ip_b), atoi (ip_c), atoi (ip_d), atoi (ip_e), atoi (ip_f), atoi (ip_g), atoi (ip_h));
      
        mercados [quantidadeMercados++].cod = codigo;
        
        printf("[MELHOR PRECO] (discovery_mercado) Inseriu o mercado de codigo %d\n", codigo);
        
        coap_set_payload (response, "1", 1); // Envia a resposta 0 para informar que a inclusao do mercado ocorreu
      }
      else
      {
        coap_set_payload (response, "0", 1); // Envia a resposta 0 para informar que a inclusao do mercado nao ocorreu
      }       
  }
}

// Recurso para receber requisicoes de compra das geladeiras
RESOURCE (compra, METHOD_GET, "server/compra", "title=\"Compra\";rt=\"Text\"");

void compra_handler (void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  printf("[MELHOR PRECO] (compra_handler) Solicitacao de compra recebida.\n");
    
  int tamanho = coap_get_payload (request, &conteudo);
  
  if (conteudo) 
  {
    memcpy (msg, conteudo, tamanho);
    
    msg [tamanho] = 0;
    
    //printf ("[MELHOR PRECO] (compra_handler) Conteudo da compra: %s",msg); 
  }
  
  char 
    *item = strstr (msg, "item");
  
  if (item != NULL)
  {
    printf ("[MELHOR PRECO] (compra_handler) Conteudo: %s\n", msg);
    
    memcpy (buffer, "01", 2);
  }
  else
  {
   memcpy (buffer, "-1", 2);
  }
  
  REST.set_response_payload (response, buffer, 2);
}


PROCESS (preco, "Menor Preco");

AUTOSTART_PROCESSES (&preco);

PROCESS_THREAD (preco, ev, data)
{
  PROCESS_BEGIN ();

  printf("[MELHOR PRECO] (principal) Melhor preco iniciado.\n");
  
  rest_init_engine();
  
  rest_activate_resource(&resource_discovery);
  
  rest_activate_resource(&resource_compra);
    
  rest_activate_resource(&resource_discovery_mercado);
    
  quantidadeMercados = 0; 
    
  while (1)
  {
    PROCESS_WAIT_EVENT (); 
 
  }
  
  PROCESS_END ();
}
