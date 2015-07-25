/**
 * This example is for the root/sink of the MLST (./mlst_network.h). This node is the node that receives all the messages but does not send 
 * any messages.
 *
 * @see ./mlst_network.h << The library this example is for
 * @see ./mlst_network_example_node.c << The example code for the corresponding simple nodes
 *
 * @author Dominik Krupke, d.krupke@tu-bs.de
 * @year 2015
 */


//this will switch the headers to add root-node features
#define ROOT
#include "contiki.h"
#include "net/rime/rime.h"
#include <stdio.h>
#include <stdlib.h>
#include "mlst_network.h"

void onIncomingMessage(void* msg, uint16_t msg_size){
	printf("Received Message\n");
}

/*---------------------------------------------------------------------------*/
PROCESS(example_mlst_root_process, "MLST Root Example");
AUTOSTART_PROCESSES(&example_mlst_root_process);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_mlst_root_process, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();

	//Initialize the mlst-network. Has to be done to open ports, etc.
	mlst_init();	
	//Sets the callback, that is called if a new message arrives
	rsunicast_setNewMessageCallback_root(onIncomingMessage);

	while(1) {
		etimer_set(&et, CLOCK_SECOND * 4);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/

