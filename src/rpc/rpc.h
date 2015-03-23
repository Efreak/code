#ifndef INEXOR_RPC_RPC_HEADER
#define INEXOR_RPC_RPC_HEADER

// Convenience Header

#include "inexor/net/MCServer.h"

#include "inexor/rpc/inexor_service.pb.h"
#include "inexor/rpc/InexorServiceImpl.h"
#include "inexor/rpc/MCRpcServer.h"

#include "inexor/rpc/rpc_sb_compat.h"

namespace inexor {
namespace rpc {

  /** The message connect server that is bound to a socket */
  extern inexor::net::MCServer *ipc_socket;
  /** The InexorService instance */
  extern InexorService *ipc_inexor_service;
  /** The RPC Server handling calls */
  extern MCRpcServer   *ipc_server;

}
}

#endif
