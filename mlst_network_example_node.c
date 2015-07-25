/**
 * This example is for a simple node of the MLST (./mlst_network.h). This node only sends and forwards messages.
 *
 * @see ./mlst_network.h << The library this example is for
 * @see ./mlst_network_example_root.c << The example code for the corresponding root node
 *
 * @author Dominik Krupke, d.krupke@tu-bs.de
 * @year 2015
 */

#include "contiki.h"
#include "net/rime/rime.h"
#include <stdio.h>
#include <stdlib.h>
#include "mlst_network.h"
#include "./auxiliary.h"


/*---------------------------------------------------------------------------*/
PROCESS(example_mlst_node_process, "MLST Node Example");
AUTOSTART_PROCESSES(&example_mlst_node_process);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_mlst_node_process, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();

	//Initialize the mlst-network. Has to be done to open ports, etc.
	mlst_init();	

	while(1) {
		mlst_print_state();
		etimer_set(&et, CLOCK_SECOND * 4 * getRandomFloat(0.5,1.0));
		uint8_t data[7];
		mlst_send(&data, sizeof(data));
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/

