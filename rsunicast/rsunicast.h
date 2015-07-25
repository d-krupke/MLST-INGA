/**
 * MODULE OF mlst_network.h
 *
 * This file implements a reliable sleepenabled unicast. 
 * It builds upon unicast and is no extension of runicast!
 * 
 * For the energy saving, the networking of some nodes can be temporarily switched off if they do not act as hops.
 * If someone tells it that it can sleep the rsunicast will automatically switch off as soon as the message queue is empty.
 * The messaging will automatically switch on again as soon new messages are to be sent.
 * 
 * Only one instance of rsunicast can be open. This is for the simple reason that the sleeping becomes hard to manage with
 * multiple open connections. Further there is probably no use of multiple connections as there is only one MLST.
 *
 * End User Functions:
 * --------------------------
 * void rsunicast_init(); //initializes the rsunicast (i.e. opens the communication channels)
 * void rsunicast_send(void* msg, uint16_t size); //for sending a message to the sink of the MLST
 * void rsunicast_allowSleeping(); //Allows the rsunicast to sleep if it is idle (no messages can be received)
 * void rsunicast_disallowSleeping(); //Wakes the rsunicast up
 * void rsunicast_setparent(uint16_t id); //Sets the parent in the Sink-Tree
 * void rsunicast_setFailureCallback(void (*onLostMessageCB)(uint16_t id, uint8_t times)); //Sets the function that is called
 * 																if the last messages times out without an ACK
 * rsunicast_print_state(); //Prints some informations (messages in queue, ...)
 * ROOT ONLY: void rsunicast_setNewMessageCallback_root(void (*cb)(void* msg, uint16_t size)); //This callback is called on
 * 																			new incoming messages
 *
 * @author Dominik Krupke, d.krupke@tu-bs.de
 * @year 2015
 * @licence MIT
 */


#ifndef RSUNICAST_H
#define RSUNICAST_H

#include "contiki.h"
#include "net/rime/rime.h"
#include "rsunicast_history.h"

#ifndef CHECK_ALLOCATION
#define CHECK_ALLOCATION(x)	if( (x) == 0 ){ printf("MEMORY ALLOCATION FAILED. EXPECT THE UNEXPECTED!\n"); }
#endif

//Defines the communication port for sending the user data
#define MESSAGING_PORT 181
//Defines the communication port for sending and receiving acknowledgements for the use data messages
#define ACKNOWLEDGEMENT_PORT 182
//If after this time no acknowledgement has been received, the message times out and is either resent after some delay or discarded
#define TIMEOUT_IN_SEC 0.2
//The number of resends that are made before a message is discarded
#define MAX_TRIES 5
//The delay before a message is sent (randomized to prevent all nodes to send always at the same time)
#define NEXT_MSG_DELAY 0.01
//Extra delay for failed messages depending on number of retries. Is multiplied by tries^2 * rnd(0,1)
#define DELAY_ON_FAIL_IN_SEC 0.1

void rsunicast_send(void* msg, uint16_t size); //preliminary definition


//The message queue saves all the messages that have to be sent. They are sent serially to avoid collisions and 
// better acknowledge management
struct RSUnicastQueueElement;
struct RSUnicastQueueElement {
	void *msg;
	uint16_t size;
	uint8_t tries;
	struct RSUnicastQueueElement* next;
};

//**VARIABLES**
struct unicast_conn rsu_data_channel; //channel on which the actual messages are sent
struct unicast_conn rsu_ack_channel; //Channel on which the ACKs are sent
struct ctimer rsu_timer; //Timer used for all the tasks
struct RSUnicastQueueElement* rsu_queue = 0; //The messaging queue (messages to be sent)
void (*rsu_onLostMessageCB)(uint16_t, uint8_t) = 0; //Called if a message times out without ACK
uint8_t rsu_seqno = 0; //The increasing(+mod) seqno to prevent duplicates
uint8_t rsu_is_online = 0; //1 iff the communication channels are open
uint8_t rsu_is_allowed_to_sleep = 0; //1 iff is allowed to switch off networking if idle
uint16_t rsu_parent = 0; //the parent in the sink tree to whom the message are sent/forwarded
uint16_t rsu_messages_in_queue = 0;
//--VARIABLES--




//**CTIMER CALLBACKS**

//forward declaration because needed here
static void rsu_send_next_message(void* ctimer_data);

//Is called if a sent messages times out without the acknowledgement being received.
static void rsu_on_ack_timeout(void* ctimer_data)
{
#ifdef DEBUG
	printf("TIME OUT\n");
#endif
	//TODO rsu_parent
	if(rsu_onLostMessageCB!=0) (*rsu_onLostMessageCB)(rsu_parent, rsu_queue->tries);

	//If there has been to many failed transmission attempts
	if(rsu_queue->tries > MAX_TRIES){
		//Remove first element in queue
		free(rsu_queue->msg);
		struct RSUnicastQueueElement* tmp = rsu_queue;
		rsu_queue = rsu_queue->next;
		free(tmp);

		//if is now idle and allowed to sleep, go to sleep
		if(rsu_queue == 0 && rsu_is_allowed_to_sleep == 1){
			unicast_close(&rsu_data_channel);
			unicast_close(&rsu_ack_channel);
			rsu_is_online = 0;
		}
	}

	if(rsu_queue!=0){
		//Start timer for next message
		ctimer_set(&rsu_timer, CLOCK_SECOND*DELAY_ON_FAIL_IN_SEC*((float)random_rand()/RANDOM_RAND_MAX)*(rsu_queue->tries*rsu_queue->tries),
				rsu_send_next_message, 0);
	}
}


//Is called if the first element of the queue should be sent
static void rsu_send_next_message(void* ctimer_data)
{
	if(rsu_parent!=0){
#ifdef DEBUG
		printf("TRY TO SEND\n");
#endif
		packetbuf_copyfrom(rsu_queue->msg, rsu_queue->size);
		static linkaddr_t recv;
		recv.u8[0] = rsu_parent>>8;
		recv.u8[1] = rsu_parent&0xFF;
		unicast_send(&rsu_data_channel, &recv);
		rsu_queue->tries++;
	}

	//set timeout
	ctimer_stop(&rsu_timer);
	//TODO: Append rsu_parent
	ctimer_set(&rsu_timer, CLOCK_SECOND*TIMEOUT_IN_SEC, rsu_on_ack_timeout, 0);
}
//--CTIMER CALLBACKS--

//**UNICAST CALLBACKS**

/**
 * Called on new incoming message on the acknowledgment channel.
 * There cannot be any duplicate acknowledgments. It will always correspond to the top most message in the queue
 */
void rsu_on_recieve_ack(struct unicast_conn* c, const linkaddr_t *from)
{
#ifdef DEBUG
	printf("SUCCESS\n");
#endif
	if(rsu_queue == 0){ printf("Received unexpected ACK\n"); return;}
	//Remove first element in queue
	free(rsu_queue->msg);
	struct RSUnicastQueueElement* tmp = rsu_queue;
	rsu_queue = rsu_queue->next;
	free(tmp);
	rsu_messages_in_queue--;

	//Stop timeout
	ctimer_stop(&rsu_timer);
	if(rsu_queue != 0){
		//Start timer for next message
		ctimer_set(&rsu_timer, CLOCK_SECOND*NEXT_MSG_DELAY*(0.5+(float)random_rand()/(2*RANDOM_RAND_MAX)), rsu_send_next_message, 0);
	}

	//if is idle and allowed to sleep, go to sleep
	if(rsu_queue == 0 && rsu_is_allowed_to_sleep == 1){
		unicast_close(&rsu_data_channel);
		unicast_close(&rsu_ack_channel);
		rsu_is_online = 0;
	}
}
static const struct unicast_callbacks rsu_ack_callbacks = {rsu_on_recieve_ack};



#ifdef ROOT
void (*rsu_on_new_message_for_root_cb)(void* msg, uint16_t size) = 0; //Pointer to the callback for arriving user data messages.

/**
 * Sets the callback for the root that is called when user data messages from the other nodes arrive.
 * Thus, this is the other ending of 'void rsunicast_send(void* msg, uint16_t size)'
 * @param *cb 	Pointer to the function that is called with *msg containing the user data and size the byte-length of it
 */
void rsunicast_setNewMessageCallback_root(void (*cb)(void* msg, uint16_t size)){
	rsu_on_new_message_for_root_cb = cb;
}
#endif


//Called on new incoming message on the data channel
void rsu_on_new_message(struct unicast_conn* c, const linkaddr_t *from)
{
	uint16_t id = ((uint16_t)from->u8[0])<<8 | from->u8[1]; //decode id
	void* msg = packetbuf_dataptr();
	uint16_t size = packetbuf_datalen();
	uint8_t seqno = *(uint8_t*)msg;	

	//send ACK
	char ack = 'A';
	packetbuf_copyfrom(&ack, 1);
	static linkaddr_t recv;
	recv.u8[0] = id>>8;
	recv.u8[1] = id&0xFF;
	unicast_send(&rsu_ack_channel, &recv);
#ifdef ROOT
	//Inform root about new message for it
	if(rsu_on_new_message_for_root_cb!=0){
		if(rsu_check_history(id, seqno)==0){ //no duplicate
			(*rsu_on_new_message_for_root_cb)(msg+1, size-sizeof(uint8_t));
		}
	}
#else
	//Check for duplicate
	if(rsu_check_history(id, seqno)!=0){
#ifdef DEBUG
		printf("Received duplicate message from %d\n",id);
#endif
		//Duplicate
		return;
	} else {
#ifdef DEBUG
		printf("Received message from %d\n",id);
#endif
		//Add to history
		rsu_add_history(id, seqno);
		//Add to queue
		rsunicast_send(msg+1, size-sizeof(uint8_t));
	}
#endif
}
static const struct unicast_callbacks rsu_msg_callbacks = {rsu_on_new_message};
//--UNICAST CALLBACKS--


/**
 * Sends data of this node to the parent. If there are still outstanding message, it is appended to the end of the message 
 * queue.
 * @param msg The data to be sent. Will be copied, so you can free the memory afterwards
 * @param size The size of msg
 */
void rsunicast_send(void* msg, uint16_t size)
{
	//if is sleeping, wake up
	if(rsu_is_online == 0) {
		unicast_open(&rsu_data_channel, MESSAGING_PORT, &rsu_msg_callbacks);
		unicast_open(&rsu_ack_channel, ACKNOWLEDGEMENT_PORT, &rsu_ack_callbacks);
		rsu_is_online = 1;
	}

	//Create Queue Entry
	struct RSUnicastQueueElement* queue_element = (struct RSUnicastQueueElement*) calloc(1, sizeof(struct RSUnicastQueueElement));
	CHECK_ALLOCATION( queue_element );
	queue_element->size = size + sizeof(uint8_t);
	queue_element->msg = calloc(1, queue_element->size);
	CHECK_ALLOCATION( queue_element->msg );
	//set seqno
	*((uint8_t*)(queue_element->msg)) = rsu_seqno;
	memcpy(queue_element->msg+1, msg, size);
	queue_element->tries = 0;

	//increment sequence no
	if(rsu_seqno == 0xff) rsu_seqno = 0;
	else rsu_seqno++;


	//Add to queue
	if(rsu_queue==0) {
		rsu_queue = queue_element;
		ctimer_set(&rsu_timer, CLOCK_SECOND*NEXT_MSG_DELAY*(0.5+(float)random_rand()/(2*RANDOM_RAND_MAX)), rsu_send_next_message, 0); //bump sending if idle
	} else {
		//append at end
		struct RSUnicastQueueElement* tmp = rsu_queue;
		while(tmp->next!=0) tmp = tmp->next;
		tmp->next = queue_element;
	}	
	rsu_messages_in_queue++;
}




/**
 * Sets up the messaging and opens the channels. 
 * No messages can be received or sent until this function has been called!
 */
void rsunicast_init()
{
	static uint8_t is_initialized = 0;
	if(is_initialized==0) {
		unicast_open(&rsu_data_channel, MESSAGING_PORT, &rsu_msg_callbacks);
		unicast_open(&rsu_ack_channel, ACKNOWLEDGEMENT_PORT, &rsu_ack_callbacks);
		rsu_is_online = 1;
		is_initialized = 1;
	}
}

/**
 * Will allow the rsunicast to close the connections if nothing has to be sent
 */
void rsunicast_allowSleeping()
{
	rsu_is_allowed_to_sleep = 1;
	//if is idle, set to sleep
	if(rsu_queue == 0) {
		unicast_close(&rsu_data_channel);
		unicast_close(&rsu_ack_channel);
		rsu_is_online = 0;
	}
}

/**
 * Disallows the rsuincast to close the connections if nothing has to be sent.
 * Wakes the rsunicast up if it is sleeping.
 */
void rsunicast_disallowSleeping()
{
	rsu_is_allowed_to_sleep = 0;
	//if is sleeping, wake up
	if(rsu_is_online == 0) {
		unicast_open(&rsu_data_channel, MESSAGING_PORT, &rsu_msg_callbacks);
		unicast_open(&rsu_ack_channel, ACKNOWLEDGEMENT_PORT, &rsu_ack_callbacks);
		rsu_is_online = 1;
	}
}

/**
 * Sets the parent to whom all messages are sent/forwarded
 * If 0, the parent is undefined
 */
void rsunicast_setparent(uint16_t id)
{
	rsu_parent = id;
}	

/**
 * Sets the function that is called if the acknowledgement is not received
 */
void rsunicast_setFailureCallback(void (*onLostMessageCB)(uint16_t id, uint8_t times))
{
	rsu_onLostMessageCB = onLostMessageCB;
}

/**
 * Prints some information about the state of rsunicast.
 */
void rsunicast_print_state()
{
	printf("RSUNICAST: Port=(%u/%u), Parent=%u, Messages in queue=%u", MESSAGING_PORT, ACKNOWLEDGEMENT_PORT, rsu_parent, rsu_messages_in_queue);
	if(rsu_is_online == 0){
		printf(", offline\n");
	} else {
		printf(", online\n");
	}
}
#endif
