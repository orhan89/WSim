/*
 *  simulation.c
 *  
 *
 *  Created by Guillaume Chelius on 01/08/05.
 *  Copyright 2005 __WorldSens__. All rights reserved.
 *
 */
#include <private/core_private.h>
#include <private/parse.h>
#include <private/command_line.h>
#include <private/log_private.h>

#include "libtracer/tracer.h"

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

int g_m_nodes = -1;
int g_c_nodes = 0;
	
double g_x    = -1.0;
double g_y    = -1.0;
double g_z    = -1.0;

uint64_t g_time = 0;

struct _worldsens_s worldsens;


/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

int simulation_init(int argc, char *argv[]) 
{
  if (command_line (argc, argv)) 
    {
      return -1;
    }
  
  if (parse_config ()) 
    {
      return -1;
    }
  
  if (worldsens_s_initialize(&worldsens))
    {
      return -1;
    }
	
  return 0;
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

uint64_t get_wsnet_time()
{
  return g_time;
}

int simulation_start(int argc, char* argv[]) 
{
  if (simulation_init(argc, argv) < 0) 
    {
      fprintf(stderr, "failed in simulation_init\n");
      return -1;
    }

  /* TRACER */
  tracer_init("wsnet1.trc", get_wsnet_time);
  core_start(&worldsens);
  tracer_close(); 

  if (g_log_to_file) 
    {
      fclose(g_log_file);
    }

  return 0;
}

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/