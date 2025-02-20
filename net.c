#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
bool nread(int fd, int len, uint8_t *buf)
{
  int bytes_read = 0;

  while (bytes_read < len) {
      int val = read(fd, &buf[bytes_read], len - bytes_read);
      
      if (val == -1) {
          return false;
      }
      bytes_read += val;
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
bool nwrite(int fd, int len, uint8_t *buf)
{
  int bytes_written = 0;

  while (bytes_written < len) {
      int val = write(fd, &buf[bytes_written], len - bytes_written);
      
      if (val == -1) {
          return false;
      }
      bytes_written += val;
  }
  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
bool recv_packet(int fd, uint32_t *op, uint8_t *ret, uint8_t *block)
{
  uint8_t header_byte[HEADER_LEN];

  if (!nread(fd, HEADER_LEN, header_byte)){
    return false;
  }

  memcpy(op, header_byte, sizeof(*op));
  *op = ntohl(*op);
  memcpy(ret, &header_byte[sizeof(*op)], 1);

  if ((*ret >> 1) == 0){
    return true;
  }

  else if ((*ret >> 1) == 1){
    
    if (!nread(fd, JBOD_BLOCK_SIZE, block)){
        return false;
    }
    return true;
  }

  return false;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
bool send_packet(int sd, uint32_t op, uint8_t *block)
{
  uint8_t packet_byte[JBOD_BLOCK_SIZE + HEADER_LEN];
  op = htonl(op);

  memcpy(packet_byte, &op, sizeof(op));
  uint8_t temp_byte = 0;
  bool is_a_block = false;

  if (block == NULL){
    is_a_block = false;
    temp_byte = 0;
  }
  
  else{
      is_a_block = true;
      temp_byte = 2;
  }

  memcpy(&packet_byte[sizeof(op)], &temp_byte, 1);
  bool val;

  if (is_a_block){
    memcpy(&packet_byte[sizeof(op) + 1], block, JBOD_BLOCK_SIZE);
    val = nwrite(sd, JBOD_BLOCK_SIZE + HEADER_LEN, packet_byte);
  } 
  
  else{
      val = nwrite(sd, HEADER_LEN, packet_byte);
  }
  return val;
}

/* connect to server and set the global client variable to the socket */
bool jbod_connect(const char *ip, uint16_t port)
{
  struct sockaddr_in caddr;
  caddr.sin_family = AF_INET;
  caddr.sin_port = htons(port);

  if(inet_aton(ip, &caddr.sin_addr) == 0){
    return false;
  }
  
  cli_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (cli_sd == -1){
    return false;
  }

  if (connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1){
    return false;
  }
  return true;
}

void jbod_disconnect(void)
{
  close(cli_sd);
  cli_sd = -1;
}

int jbod_client_operation(uint32_t op, uint8_t *block)
{
  uint8_t temp_ret;
  uint32_t temp_op;
  bool temp = send_packet(cli_sd, op, block);
  if (!temp){
    return -1;
  }

  temp = recv_packet(cli_sd, &temp_op, &temp_ret, block);
  if (!temp){
    return -1;
  }

  if(((temp_ret >> 7) & 0x01) == 1){
      return -1;
  }

  else if(((temp_ret >> 7) & 0x01) == 0){
      return 0;
  }
  return -1;
}