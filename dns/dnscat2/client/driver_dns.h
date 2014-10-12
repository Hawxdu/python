#ifndef __DRIVER_DNS_H__
#define __DRIVER_DNS_H__

#include "select_group.h"
#include "session.h"

typedef struct
{
  int        s;

  char      *domain;
  char      *dns_host;
  int        dns_port;

  NBBOOL     is_closed;

} driver_dns_t;

driver_dns_t *driver_dns_create(select_group_t *group, char *domain);
void          driver_dns_destroy();

#endif
