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

#ifndef _TTFTPS_H_
#define _TTFTPS_H_

typedef unsigned char u8;
typedef unsigned short u16;

typedef enum {FALSE = 0, TRUE = 1} BOOL;

#define DEFAULT_BLOCK_SIZE 512
#define LARGEST_BLOCK_SIZE 65464

#define RECV_ERROR_COUNT_MAX 10

typedef enum {
  WAITING_FOR_REQUEST,
  WAITING_FOR_ACK,
} server_mode_t;



typedef struct {
  uint  server_port;
  uint  client_port;
  char* interface;
  char* path_root;
} ttftps_params_t;

#define DEFAULT_TTFTPS_PARAMS {                 \
    .server_port = 69,                          \
      .client_port = 8080,                      \
      .interface = NULL,                        \
      .path_root = NULL,                        \
      }

enum {
  TFTP_OP_RRQ=1,
  TFTP_OP_WRQ,
  TFTP_OP_DATA,
  TFTP_OP_ACK,
  TFTP_OP_ERR,
  TFTP_OP_OACK,
};

typedef enum {
  TFTP_MODE_NETASCII,
  TFTP_MODE_OCTET,
  TFTP_MODE_MAIL,
} tftp_mode_t;

typedef struct {
  server_mode_t server_mode;
  size_t ack_num;
  size_t block_size;
  tftp_mode_t transfer_mode;
  int client_socket;
  char* path_root;
  struct sockaddr_in client;
  struct sockaddr_in client_me;
  FILE* file;
  uint server_port;
  uint client_port;
} server_state_t;

#define DEFAULT_SERVER_STATE {                  \
    .server_mode = WAITING_FOR_REQUEST,         \
      .ack_num = 0,                             \
      .block_size = DEFAULT_BLOCK_SIZE,         \
      .transfer_mode = TFTP_MODE_NETASCII,      \
      .client_socket = -1,                      \
      .file = NULL,                             \
      .path_root = NULL,                        \
      }

typedef enum {
  TFTP_OPTION_BLKSIZE,
} tftp_option_t;

typedef struct {
  u16 opcode;
  u16 ack_num;
  char* filename;
  uint block_size;
  tftp_mode_t mode;
  uint option_bitmap;
#define BLKSIZE_OPTION (1 << 0)
} tftp_packet_t;

#define DEFAULT_TFTP_PACKET {                   \
    .opcode = 0,                                \
      .filename = NULL,                         \
      .block_size = DEFAULT_BLOCK_SIZE,         \
      .mode = TFTP_MODE_NETASCII,               \
      .option_bitmap = 0,                       \
      }


#endif // _TTFTPS_H_

/* 
 * Local Variables:
 * indent-tabs-mode: nil
 * mode: C
 * c-file-style: "gnu"
 * c-basic-offset: 2
 * End: 
 */

/* vi: set et sw=2 sts=2: */
