/**
 * MODULE OF runicast.h
 *
 * Here the logic to check if a received message of the rsunicast is a duplicate (lost ACK) is implemented.
 * It has only internal use and you (enduser) do not have to read it, except you want to manipulate the history size.
 *
 * @author Dominik Krupke, d.krupke@tu-bs.de
 */


#ifndef RSUNICAST_HISTORY_H
#define RSUNICAST_HISTORY_H

//For checking if the allocation has failed
#ifndef CHECK_ALLOCATION
#define CHECK_ALLOCATION(x)	if( (x) == 0 ){ printf("MEMORY ALLOCATION FAILED. EXPECT THE UNEXPECTED!\n"); }
#endif

/**
 * The amount of neighbors that are kept in the history. It is always the oldest entry that is removed if it is full.
 */
#define MAX_HISTORY_SIZE 30

//**HISTORY DATABASE**
struct rsu_history_element;
struct rsu_history_element{
	uint16_t id;
	uint8_t seqno;
	struct rsu_history_element* next;
};
struct rsu_history_element* rsu_history_list = 0;
uint8_t rsu_history_size = 0;

/**
 * Returns 1 iff the last received message of this neighbor has the same seqno.
 * Else 0
 */
uint8_t rsu_check_history(uint16_t from, uint8_t seqno){
	struct rsu_history_element* tmp = rsu_history_list;
	for(;tmp!=0; tmp = tmp->next){
		if(tmp->id == from && tmp->seqno == seqno) return 1;
	}
	return 0;
}

/**
 * Adds an entry to the history. 
 * For each robot only the last seqno is saved
 */
void rsu_add_history(uint16_t from, uint8_t seqno){
	struct rsu_history_element* tmp = rsu_history_list;

	//Remove old entries of this neighbor at the beginning of the list
	while(rsu_history_list != 0 && rsu_history_list->id == from){
		rsu_history_list = rsu_history_list->next;
		free(tmp);
		rsu_history_size--;
		tmp = rsu_history_list;
	}

	if(rsu_history_list==0){//if list is empty, set this element the first
		rsu_history_list = (struct rsu_history_element*) calloc(1, sizeof(struct rsu_history_element));
		CHECK_ALLOCATION( rsu_history_list );
		rsu_history_list->id = from;
		rsu_history_list->seqno = seqno;
		rsu_history_list->next = 0;
		rsu_history_size++;
	} else {//Append to end of list

		for(;tmp!=0; tmp = tmp->next){
			//Reached end, insert
			if(tmp->next == 0){
				tmp->next = (struct rsu_history_element*) calloc(1, sizeof(struct rsu_history_element));
				CHECK_ALLOCATION( tmp->next );
				tmp->next->id = from;
				tmp->next->seqno = seqno;
				tmp->next->next = 0;
				rsu_history_size++;
				break;
			} else if(tmp->next->id == from) { //Old entry of this neighbor found -> delete it
				struct rsu_history_element* tmp2 = tmp->next;
				tmp->next = tmp2->next;
				free(tmp2);
			}
		}
	}
	//If history has to many elements, clean the oldest
	while(rsu_history_size>MAX_HISTORY_SIZE){
		tmp = rsu_history_list;
		rsu_history_list = rsu_history_list->next;
		free(tmp);
		rsu_history_size--;
	}
}

//--HISTORY DATABASE--
#endif
