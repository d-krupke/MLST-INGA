/**
 * This example is for a simple node of the MLST (./mlst_network.h). This node only sends and forwards messages.
 *
 * @see ./mlst_network.h << The library this example is for
 * @see ./mlst_network_example_root.c << The example code for the corresponding root node
 *
 * @author Dominik Krupke, d.krupke@tu-bs.de
 * @year 2015
 */

#define ENERGY_STATE ((linkaddr_node_addr.u8[0]<<8) | linkaddr_node_addr.u8[1])%3+1
#define EA3

#include "contiki.h"
#include "net/rime/rime.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef EA1
#include "mlst_network-ea1.h"
#endif
#ifdef EA2
#include "mlst_network-ea2.h"
#endif
#ifdef EA3
#include "mlst_network-ea3.h"
#endif

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
	eamlst_set_energy_state(ENERGY_STATE);

	while(1) {
		mlst_print_state();
		rsunicast_print_state();
		etimer_set(&et, CLOCK_SECOND * 4 * getRandomFloat(0.5,1.0));
		//uint8_t data[7];
		//mlst_send(&data, sizeof(data));
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/

