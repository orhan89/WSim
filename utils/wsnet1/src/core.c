/*
 *  core.c
 *  
 *
 *  Created by Guillaume Chelius on 20/11/05.
 *  Copyright 2005 __WorldSens__. All rights reserved.
 *  Modified Loic Lemaitre 2009
 *
 */

#include "private/simulation_private.h"
#include "private/nodes_private.h"
#include "private/packets_private.h"
#include "private/core_private.h"

#include "public/log.h"
#include "worldsens.h"

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

struct _event *g_events = NULL;
int g_events_card = 0;

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

void
core_runtime_end (void)
{
  struct _packet *packet;
  struct _event *event;

  while (g_events)
    {
      event = g_events;
      g_events = g_events->next;
      free (event);
    }

  while (g_packets)
    {
      packet = g_packets;
      g_packets = g_packets->next;
      packet_destroy (packet);
    }

  free (g_nodes);
  return;
}


/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

#if (BIG_ENDIAN == BYTE_ORDER)
static uint64_t
htonll (uint64_t v)
{
  return v;
}

/* static */ double
htondbl (double v)
{
  return v;
}

#else
static uint64_t
htonll (uint64_t v)
{
  uint64_t r;
  uint8_t *pv, *pr;

  pv = (uint8_t *) & v;
  pr = (uint8_t *) & r;

  pr[0] = pv[7];
  pr[1] = pv[6];
  pr[2] = pv[5];
  pr[3] = pv[4];
  pr[4] = pv[3];
  pr[5] = pv[2];
  pr[6] = pv[1];
  pr[7] = pv[0];

  return r;
}

/* static */ double
htondbl (double v)
{
  double r;
  uint8_t *pv, *pr;

  pv = (uint8_t *) & v;
  pr = (uint8_t *) & r;

  pr[0] = pv[7];
  pr[1] = pv[6];
  pr[2] = pv[5];
  pr[3] = pv[4];
  pr[4] = pv[3];
  pr[5] = pv[2];
  pr[6] = pv[1];
  pr[7] = pv[0];
  return r;
}
#endif

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

int
core_start (struct _worldsens_s *worldsens)
{
  static int evt_nb = 0;

  simulation_keeps_going = 1;

  while (simulation_keeps_going == 1)
    {

      /* If no event or if event in the future, wait at rp point */
      if ((g_events == NULL) || (g_events->time > get_global_time()))
	{
	  if (worldsens_s_listen_to_next_rp (worldsens) < 0)
	    {
	      return -1;
	    }
	}

      if (g_events == NULL)
	{
	  /* If no event, program rp point */
	  if (worldsens_s_save_release_request
	      (worldsens, WORLDSENS_SYNCH_PERIOD))
	    return -1;
	}
      else if (get_global_time() < g_events->time)
	{
	  /* If event, program event */
	  if (worldsens_s_save_release_request
	      (worldsens, g_events->time - get_global_time()))
	    return -1;
	}
      else
	{
	  struct _event *event = g_events;

	  /* And time goes on... */
	  if (get_global_time() != event->time)
	    {
	      fprintf (stderr,
		       "\nEXCEPTION: CORE DESYNCHRONIZATION time=%" PRId64
		       ", evt_time=%" PRId64 "\n", get_global_time(), event->time);
	      return -1;
	    }
	  evt_nb++;

	  if (event->type == TX_EVENT_TYPE)
	    {
	      struct _packet *packet = event->packet;
	      struct _worldsens_data worldsens_data[g_c_nodes];
	      int i = g_m_nodes;
	      int c_node = 0;

	      /* Remove event */
	      g_events = event->next;
	      g_events_card--;
	      free (event);

	      while (i--)
		{
		  if (g_nodes[i].active)
		    {
		      struct _packet *rx_pkt = packet_duplicate (packet);

		      /* Update receiving node mobility */
		      //mobility_update (&g_nodes[i]);

		      /* Compute packet BER/SiNR */
		      //propagation_compute_BER (&g_nodes[i], rx_pkt);

		      /* Record reception and destroy packet */
		      uint64_t *tx_mW = (uint64_t *) &(rx_pkt->tx_mW);  /* put double into uint64_t variable for swap */
		      worldsens_data[c_node].node  = htonl (i);
		      worldsens_data[c_node].data  = packet->data[0];
		      worldsens_data[c_node].SiNR  = htonll (0.0);     /* perfect radio layer: no noise */
		      worldsens_data[c_node].rx_mW = htonll (*tx_mW);  /* perfect radio layer: no energy dissipation... */
		      packet_destroy (rx_pkt);	//TODO: optimize.... no need to creat and destroy packet...

		      /* Update considered node */
		      c_node++;
		    }
		}

	      if (g_events == NULL)
		{
		  /* If no more event, backup and program rp point */
#define MAGIC_VALUE 31250
		  if (worldsens_s_save_release_request_rx (worldsens, packet->node->addr, 
							   packet->freq, packet->modulation, 
							   worldsens_data, MAGIC_VALUE))	// ToCheck: optimization to avoir backtracks
		    return -1;
		}
	      else
		{
		  /* If future event, backup and program event */
		  if (g_events->time > get_global_time())
		    {
		      if (worldsens_s_save_release_request_rx
			  (worldsens, packet->node->addr, packet->freq,
			   packet->modulation, worldsens_data,
			   g_events->time - get_global_time()))
			return -1;
		    }
		  else
		    {
		      /* If simultaneous events, do nothing */
		      if (worldsens_s_rx (worldsens, packet->node->addr,
					  packet->freq, packet->modulation,
					  worldsens_data))
			return -1;
		    }
		}

	      /* Update "in the air tonight" packet list */
	      if (core_update_packet_list ())
		{
		  return -1;
		}
	    }
	}
    }				/* while */

  return 0;
}


/**************************************************************************/
/**************************************************************************/
/**************************************************************************/
int
core_add_schedule (void (*callback) (void *), void *arg, uint64_t time)
{
  struct _event *event;
  struct _event *loop = NULL;
  struct _event *tmp = NULL;

  if ((event = (struct _event *) malloc (sizeof (struct _event))) == NULL)
    {
      fprintf (stderr, "\nEXCEPTION: MALLOC ERROC\n");
      return -1;
    }

  event->type = SCHED_EVENT_TYPE;
  event->time = time;
  event->callback = callback;
  event->arg = arg;

  if (g_events == NULL)
    {
      g_events = event;
      event->next = NULL;
    }
  else
    {
      loop = g_events;

      /* We want callback events to be called after packet events (for protocol timeouts) */
      while ((loop != NULL) && (loop->time <= event->time))
	{
	  tmp = loop;
	  loop = loop->next;
	}

      if (loop == NULL)
	{
	  tmp->next = event;
	  event->next = NULL;
	}
      else if (tmp == NULL)
	{
	  g_events = event;
	  event->next = loop;
	}
      else
	{
	  tmp->next = event;
	  event->next = loop;
	}
    }

  g_events_card++;
  return 0;
}


/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

int
core_add_packet (struct _packet *packet)
{
  struct _event *event;
  struct _event *loop = NULL;
  struct _event *tmp = NULL;
  struct _packet *p_loop = NULL;
  struct _packet *p_tmp = NULL;

  if ((event = (struct _event *) malloc (sizeof (struct _event))) == NULL)
    {
      fprintf (stderr, "\nEXCEPTION: MALLOC ERROC\n");
      return -1;
    }

  event->type = TX_EVENT_TYPE;
  event->node = packet->node->addr;
  event->time = packet->tx_end;
  event->packet = packet;

  if (g_events == NULL)
    {
      g_events = event;
      event->next = NULL;
    }
  else
    {
      loop = g_events;

      /* We want callback events to be called after packet events (for protocol timeouts) */
      while ((loop != NULL) && (loop->time < event->time))
	{
	  tmp = loop;
	  loop = loop->next;
	}

      if (loop == NULL)
	{
	  tmp->next = event;
	  event->next = NULL;
	}
      else if (tmp == NULL)
	{
	  g_events = event;
	  event->next = loop;
	}
      else
	{
	  tmp->next = event;
	  event->next = loop;
	}
    }

  /* Update "in the air tonight" packet list */
  if (g_packets == NULL)
    {
      g_packets = packet;
      packet->next = NULL;
    }
  else
    {
      p_loop = g_packets;

      while ((p_loop != NULL) && (p_loop->tx_end < packet->tx_end))
	{
	  p_tmp = p_loop;
	  p_loop = p_loop->next;
	}

      if (p_loop == NULL)
	{
	  p_tmp->next = packet;
	  packet->next = NULL;
	}
      else if (p_tmp == NULL)
	{
	  g_packets = packet;
	  packet->next = p_loop;
	}
      else
	{
	  p_tmp->next = packet;
	  packet->next = p_loop;
	}
    }

  return 0;
}


/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

int
core_update_packet_list (void)
{
  struct _packet *p_loop = g_packets;
  struct _packet *p_tmp = NULL;
  struct _event *event = g_events;
  uint64_t tx_start;

  /* Get date of first "in the air tonight" packet */
  tx_start = get_global_time();
  while (event)
    {
      if ((event->type == TX_EVENT_TYPE)
	  && (event->packet->tx_start < tx_start))
	{
	  tx_start = event->packet->tx_start;
	}
      event = event->next;
    }

  /* Remove anterior packets */
  while (p_loop != NULL)
    {
      if (p_loop->tx_end <= tx_start)
	{
	  if (p_tmp != NULL)
	    {
	      p_tmp->next = p_loop->next;
	      packet_destroy (p_loop);
	      p_loop = p_tmp->next;
	    }
	  else
	    {
	      g_packets = p_loop->next;
	      packet_destroy (p_loop);
	      p_loop = g_packets;
	    }
	}
      else
	{
	  p_tmp = p_loop;
	  p_loop = p_loop->next;
	}
    }

  return 0;
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

int
core_backtrack (uint64_t time)
{
  struct _packet *p_loop = g_packets;
  struct _packet *p_tmp = NULL;
  struct _event *e_loop = g_events;
  struct _event *e_tmp = NULL;

  while (e_loop != NULL)
    {
      if (e_loop->packet->tx_start > time)
	{
	  if (e_tmp != NULL)
	    {
	      e_tmp->next = e_loop->next;
	      free (e_loop);
	      e_loop = e_tmp->next;
	    }
	  else
	    {
	      g_events = e_loop->next;
	      free (e_loop);
	      e_loop = g_events;
	    }
	}
      else
	{
	  e_tmp = e_loop;
	  e_loop = e_loop->next;
	}
    }

  while (p_loop != NULL)
    {
      if (p_loop->tx_start > time)
	{
	  if (p_tmp != NULL)
	    {
	      p_tmp->next = p_loop->next;
	      packet_destroy (p_loop);
	      p_loop = p_tmp->next;
	    }
	  else
	    {
	      g_packets = p_loop->next;
	      packet_destroy (p_loop);
	      p_loop = g_packets;
	    }
	}
      else
	{
	  p_tmp = p_loop;
	  p_loop = p_loop->next;
	}
    }

  return 0;
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/
