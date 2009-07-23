/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 * DESCRIPTION:
 *  JACK server monitor and control
 *
 *****************************************************************************/

#include "jack.h"
#include "jack_proxy.h"

bool
jack_init(
  void)
{
  if (!jack_proxy_init())
  {
    return false;
  }

  return true;
}

void
jack_uninit(
  void)
{
  jack_proxy_uninit();
}

void
on_jack_client_appeared(
  uint64_t client_id,
  const char * client_name)
{
  lash_info("JACK client appeared.");
}

void
on_jack_client_disappeared(
  uint64_t client_id)
{
  lash_info("JACK client disappeared.");
}

void
on_jack_port_appeared(
  uint64_t client_id,
  uint64_t port_id,
  const char * port_name)
{
  lash_info("JACK port appeared.");
}

void
on_jack_port_disappeared(
  uint64_t client_id,
  uint64_t port_id,
  const char * port_name)
{
  lash_info("JACK port disappeared.");
}

void
on_jack_ports_connected(
  uint64_t client1_id,
  uint64_t port1_id,
  uint64_t client2_id,
  uint64_t port2_id)
{
  lash_info("JACK ports connected.");
}

void
on_jack_ports_disconnected(
  uint64_t client1_id,
  uint64_t port1_id,
  uint64_t client2_id,
  uint64_t port2_id)
{
  lash_info("JACK ports disconnected.");
}

bool
conf_callback(
  void * context,
  bool leaf,
  const char * address,
  char * child)
{
  char path[1024];
  const char * component;
  char * dst;
  size_t len;

  dst = path;
  component = address;
  while (*component != 0)
  {
    len = strlen(component);
    memcpy(dst, component, len);
    dst[len] = ':';
    component += len + 1;
    dst += len + 1;
  }

  strcpy(dst, child);

  if (!leaf)
  {
    dst = context;
    len = component - address;
    memcpy(dst, address, len);
    dst += len;
    len = strlen(child) + 1;
    memcpy(dst, child, len);
    dst[len] = 0;
  }

  if (leaf)
  {
    lash_info("%s (leaf)", path);
    return true;
  }

  lash_info("%s (container)", path);

  if (!jack_proxy_read_conf_container(context, context, 1024, context, conf_callback))
  {
    return false;
  }

  *(char *)component = 0;

  return true;
}

void
on_jack_server_started(
  void)
{
  char buffer[1024] = "";
  lash_info("JACK server start detected.");

  if (!jack_proxy_read_conf_container(buffer, buffer, 1024, buffer, conf_callback))
  {
  }
}

void
on_jack_server_stopped(
  void)
{
  lash_info("JACK server stop detected.");
}

void
on_jack_server_appeared(
  void)
{
  lash_info("JACK controller appeared.");
}

void
on_jack_server_disappeared(
  void)
{
  lash_info("JACK controller disappeared.");
}
