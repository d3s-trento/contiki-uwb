/*
 * Copyright (c) 2025, University of Trento, Italy
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * \file SAMU Glossy API
 *
 * Authors:
 *   Enrico Soprana <enrico.soprana@unitn.it>
 */
#ifndef GLOSSY_SAMU_H
#define GLOSSY_SAMU_H

#include <stdbool.h>
#include <stdint.h>

#include "contiki.h"

#include PROJECT_CONF_H

struct glossy_context_t {
  uint8_t n_tx;
  uint8_t n_actions;

  int forward_packet;
};

struct glossy_cfg_t {
  uint8_t N;  // number of transmissions in the Glossy round
  uint32_t W; // maximum number of actions in the Glossy round
};

struct glossy_next_action_t {
  int role;
  struct glossy_cfg_t cfg;
  
  union {
	uint8_t data_len;
	uint8_t tx_len;
  };
  uint8_t* buf;
};

extern struct glossy_next_action_t glossy_next_action;
extern struct glossy_context_t glossy_context;
extern struct pt glossy_pt;

char glossy_trx();
void glossy_init();

#define GLOSSY_FWD (0)
#define GLOSSY_INIT (1)

#define SAMU_GLOSSY(_pt, _role, _cfg, _buf, _txlen)                            \
  do {                                                                         \
	glossy_next_action.role = _role;                                           \
	glossy_next_action.cfg = _cfg;                                             \
	glossy_next_action.tx_len = _txlen;                                        \
	glossy_next_action.buf = _buf;                                             \
	PT_SPAWN(_pt, &glossy_pt, glossy_trx());                                   \
  } while (0);

#endif
