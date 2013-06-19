/*          The Trivial Trivial File Transfer Protocol Server
 *  Copyright (C) 2013 Eric Mismier
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ttftps.h"

static u8* get_next_value(u8* msg, int length)
{
  while(!*msg) {
    if(!(--length)) return NULL;
    ++msg;
  }
  return msg;
}

static BOOL get_tftp_mode(tftp_packet_t* packet, char* mode_string)
{
  if(strcasecmp(mode_string, "octet") == 0) {
    packet->mode = TFTP_MODE_OCTET;
    return TRUE;
  }

  if(strcasecmp(mode_string, "netascii" ) == 0) {
    packet->mode = TFTP_MODE_NETASCII;
    return TRUE;
  }

  if(strcasecmp(mode_string, "mail") == 0) {
    packet->mode = TFTP_MODE_MAIL;
    return TRUE;
  }
  return FALSE;
}

static BOOL process_option(tftp_packet_t* packet, char* opt, char* opt_val)
{
  if(strcasecmp(opt, "blksize") == 0) {
    packet->block_size = atoi(opt_val);
    packet->option_bitmap |= BLKSIZE_OPTION;
    return TRUE;
  }
  
  return FALSE;
}

static BOOL parse_recvd_packet(tftp_packet_t* packet, u8* msg, size_t length)
{
  char* opt;
  char* opt_val;
  int opt_len;
  int opt_val_len;
  packet->opcode = ntohs(*(short*)msg);
  
  msg += 2;
  length -= 2;
  switch(packet->opcode) {
  case TFTP_OP_RRQ:
  case TFTP_OP_WRQ:
    
    if(!(packet->filename = (char*)get_next_value(msg, length))) return FALSE;
    opt_len = strlen(packet->filename);
    msg += (opt_len+1);
    length -= (opt_len+1);
    if(!(opt = (char*)get_next_value(msg, length))) return FALSE;
    opt_len = strlen(opt);
    msg += (opt_len+1);
    length -= (opt_len+1);
    if(!get_tftp_mode(packet, opt)) return FALSE;

    packet->option_bitmap = 0;
    
    while(length > 2) {
      if(!(opt = (char*)get_next_value(msg, length))) return FALSE;
      opt_len = strlen(opt);
      msg += (opt_len+1);
      length -= (opt_len+1);

      if(!(opt_val = (char*)get_next_value(msg, length))) return FALSE;
      opt_val_len = strlen(opt_val);
      msg += (opt_val_len+1);
      length -= (opt_val_len+1);
      
      if(!process_option(packet, opt, opt_val)) break;
    }
    return TRUE;
  case TFTP_OP_DATA:
    return FALSE;
  case TFTP_OP_ACK:
    packet->ack_num = ntohs(*(short*)msg);
    return TRUE;
  case TFTP_OP_ERR:
    return TRUE;
  case TFTP_OP_OACK:
    return FALSE;
  }
  return FALSE;
}

static int prepare_oack_packet(tftp_packet_t* packet, server_state_t* server, u8* buf, int buf_size)
{
  int msg_size = 2;
  char temp_buf[1024];
  int num_bytes;
  *(u16*)buf = htons(TFTP_OP_OACK);
  if(packet->option_bitmap & BLKSIZE_OPTION) {
    server->block_size = packet->block_size;
    if(server->block_size > LARGEST_BLOCK_SIZE) server->block_size = LARGEST_BLOCK_SIZE;
    num_bytes = sprintf(temp_buf, "blksize %lu", server->block_size);
    temp_buf[7] = 0;
    temp_buf[num_bytes++] = 0;
    memcpy(&buf[msg_size], temp_buf, num_bytes);
    msg_size += num_bytes;
  }

  return msg_size;
}

BOOL send_buffer(u8* buf, int buf_size, server_state_t* server)
{
  return sendto(server->client_socket, buf, buf_size,
                0, (const struct sockaddr *)&server->client,
                sizeof(server->client)) == buf_size;
}

static BOOL send_oack(tftp_packet_t* packet, server_state_t* server)
{
  u8 buf[LARGEST_BLOCK_SIZE];
  int msg_size = prepare_oack_packet(packet, server, buf, LARGEST_BLOCK_SIZE);

  if (!send_buffer(buf, msg_size, server)) {
    return FALSE;
  }
  
  server->server_mode = WAITING_FOR_ACK;
  server->ack_num = 0;
  return TRUE;
}

/*
 *           TFTP Data Block
 *   2 bytes     2 bytes     n bytes
 *   ----------------------------------
 *  | Opcode |   Block #  |   Data     |
 *   ----------------------------------
 */

static BOOL send_next_data_block(tftp_packet_t* packet, server_state_t* server)
{
  u8* buf;
  int bytes_read;
  BOOL res;
  buf = malloc(server->block_size + 4);
  //socklen_t slen=sizeof(struct sockaddr_in);

  if(!buf) return FALSE;

  *((u16*)buf) = htons(TFTP_OP_DATA);
  *((u16*)&buf[2]) = htons(++server->ack_num);

  bytes_read = fread(&buf[4], 1, server->block_size, server->file);

  if(bytes_read < server->block_size) server->server_mode = WAITING_FOR_REQUEST;
  else server->server_mode = WAITING_FOR_ACK;
  res =  send_buffer(buf, bytes_read + 4, server);


  //fprintf(stderr, "Waiting for recvFrom for ack\n");
  //recvfrom(s, buf, 512, 0,
  //         (struct sockaddr * __restrict__)&server->client, &slen);


  //fprintf(stderr, "Got ack\n");
  //while(1);
  free(buf);
  return res;
}


static BOOL process_recvd_packet(tftp_packet_t* packet, server_state_t* server)
{
  char path[512];
  switch(packet->opcode) {
  case TFTP_OP_RRQ:

    if(server->file) {
      fclose(server->file);
    }
    
    if(packet->filename[0] == '/') packet->filename++;
    if(server->root_path) {
      strcpy(path, server->root_path);
      strcat(path, packet->filename);
    }
    else {
      strcpy(path, packet->filename);
    }
    server->file = fopen(path, server->transfer_mode == TFTP_MODE_OCTET ? "rb" : "r");
    if(!server->file) return FALSE;
    server->ack_num = 0;
    if(packet->option_bitmap) {
      return send_oack(packet, server);
    }
    else {
      return send_next_data_block(packet, server);
    }
  case TFTP_OP_WRQ:
    return FALSE;
  case TFTP_OP_DATA:
    return FALSE;
  case TFTP_OP_ACK:
    if(packet->ack_num == server->ack_num) {
      send_next_data_block(packet, server);
    }
    return TRUE;
  case TFTP_OP_ERR:
    return FALSE;
  case TFTP_OP_OACK:
    return FALSE;
  }
  return FALSE;
}

static BOOL open_socket_to_client(struct sockaddr_in* client, tftp_packet_t* packet,
                                  server_state_t* server)
{
  int s;
  if(server->client_socket >= 0) {
    close(server->client_socket);
    server->client_socket = -1;
  }

  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
    return FALSE;

  memset((char *) &server->client_me, 0, sizeof(server->client_me));
  server->client_me.sin_family = AF_INET;
  server->client_me.sin_port = htons(server->client_port);
  server->client_me.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(s, (const struct sockaddr *)&server->client_me, sizeof(server->client_me))==-1) {
    perror("bind");
    close(s);
    return FALSE;
  }

  memset((char *) &server->client, 0, sizeof(server->client));
  server->client.sin_family = AF_INET;
  server->client.sin_port = client->sin_port;
  server->client.sin_addr = client->sin_addr;

  memcpy(&server->client, client, sizeof(*client));

  server->client_socket = s;
  
  
  return TRUE;
}


static int start_server(ttftps_params_t* params)
{
  int error_count;
  struct sockaddr_in si_me, si_other;
  int s;
  server_state_t server = DEFAULT_SERVER_STATE;
  socklen_t slen=sizeof(struct sockaddr_in);
  size_t bytes_recv;
  u8 buf[LARGEST_BLOCK_SIZE + 4];  /* Add 4 for opcode (2 bytes) and block num (2 bytes) */
  
  
  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
    perror("socket");
    return EXIT_FAILURE;
  }

  server.server_port = params->server_port;
  server.client_port = params->client_port;
  
  memset((char *) &si_me, 0, sizeof(si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(params->server_port);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(s, (const struct sockaddr *)&si_me, sizeof(si_me))==-1) {
    perror("bind");
    close(s);
    return EXIT_FAILURE;
  }
  
  while(1) {
    tftp_packet_t packet = DEFAULT_TFTP_PACKET;
    if(server.server_mode == WAITING_FOR_REQUEST) {
      //printf("Calling recvfrom at line %d\n", __LINE__);
      if ((bytes_recv = recvfrom(s, buf, sizeof(buf), 0,
                                 (struct sockaddr * __restrict__)&si_other, &slen))==-1) {
        error_count++;
        if(error_count >= RECV_ERROR_COUNT_MAX) break;
        continue;
      }
    }
    else {
      //printf("Calling recvfrom at line %d\n", __LINE__);
      if ((bytes_recv = recvfrom(server.client_socket, buf, sizeof(buf), 0,
                                 (struct sockaddr * __restrict__)&server.client, &slen))==-1) {
        error_count++;
        if(error_count >= RECV_ERROR_COUNT_MAX) break;
        continue;
      }
    }
    //printf("Received packet from %s:%d, %lu\n", 
    //       inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), bytes_recv);
    if(!parse_recvd_packet(&packet, buf, bytes_recv)) {
      error_count++;
      if(error_count >= RECV_ERROR_COUNT_MAX) break;
      continue;
    }
    if(server.server_mode == WAITING_FOR_REQUEST) {
      if(!open_socket_to_client(&si_other, &packet, &server)) {
        error_count++;
        if(error_count >= RECV_ERROR_COUNT_MAX) break;
        continue;
      }
    }
    if(!process_recvd_packet(&packet, &server)) {
      error_count++;
      if(error_count >= RECV_ERROR_COUNT_MAX) break;
      continue;
    }
    error_count = 0;
  }
  
  close(s);
  return EXIT_SUCCESS;
}

BOOL process_program_args(int argc, char* argv[], ttftps_params_t* params)
{
  char* endptr;
  int opt;
  while ((opt = getopt(argc, argv, "p:r:")) != -1) {
    switch(opt) {
    case 'p':
      params->server_port = strtol(optarg, &endptr, 10);
      if(*endptr != '\0') return FALSE;
      break;
    case 'r':
      params->path_root = strdup(optarg);
      if(!params->path_root) return FALSE;
      if(params->path_root[strlen(params->path_root)-1] != '/') {
        char* temp = malloc(strlen(params->path_root + 2));
        if(!temp) {
          free(params->path_root);
          return FALSE;
        }
        memcpy(temp, params->path_root, strlen(params->path_root)+1);
        strcat(temp, "/");
        free(params->path_root);
        params->path_root = temp;
        
      }
      break;
    default:
      return FALSE;
    }
  }
  return TRUE;
}

void print_usage()
{
  fprintf(stderr, "Invalid arguments\n");
}

int main(int argc, char* argv[])
{
  ttftps_params_t params = DEFAULT_TTFTPS_PARAMS;
  if(!process_program_args(argc, argv, &params)) {
    print_usage();
    return EXIT_FAILURE;    
  }
  //printf("port = %d\n", params.server_port);
  //printf("root path = %s\n", params.path_root);
  return start_server(&params);
}




/* 
 * Local Variables:
 * indent-tabs-mode: nil
 * mode: C
 * c-file-style: "gnu"
 * c-basic-offset: 2
 * End: 
 */

/* vi: set et sw=2 sts=2: */
