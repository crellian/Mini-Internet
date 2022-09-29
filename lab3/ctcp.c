/******************************************************************************
 * ctcp.c
 * ------
 * Implementation of cTCP done here. This is the only file you need to change.
 * Look at the following files for references and useful functions:
 *   - ctcp.h: Headers for this file.
 *   - ctcp_iinked_list.h: Linked list functions for managing a linked list.
 *   - ctcp_sys.h: Connection-related structs and functions, cTCP segment
 *                 definition.
 *   - ctcp_utils.h: Checksum computation, getting the current time.
 *
 *****************************************************************************/

#include "ctcp.h"
#include "ctcp_linked_list.h"
#include "ctcp_sys.h"
#include "ctcp_utils.h"
int n = 1;
/**
 * Connection state.
 *
 * Stores per-connection information such as the current sequence number,
 * unacknowledged packets, etc.
 *
 * You should add to this to store other fields you might need.
 */
struct ctcp_state {
  struct ctcp_state *next;  /* Next in linked list */
  struct ctcp_state **prev; /* Prev in linked list */

  conn_t *conn;             /* Connection object -- needed in order to figure
                               out destination when sending */
  linked_list_t *segments;  /* Linked list of segments sent to this connection.
                               It may be useful to have multiple linked lists
                               for unacknowledged segments, segments that
                               haven't been sent, etc. Lab 1 uses the
                               stop-and-wait protocol and therefore does not
                               necessarily need a linked list. You may remove
                               this if this is the case for you */

  /* FIXME: Add other needed fields. */
  ctcp_config_t * cfg;
  int cur_seq_no;
  int ack_no;
  bool fin_wait;
  bool buffer_congested;
  linked_list_t *unsent_segments;
  linked_list_t *receiving_data;
};

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
/**
 * Linked list of connection states. Go through this in ctcp_timer() to
 * resubmit segments and tear down connections.
 */
static ctcp_state_t *state_list;

/* FIXME: Feel free to add as many helper functions as needed. Don't repeat
          code! Helper functions make the code clearer and cleaner. */
typedef struct segment_wrap{
  long last_sent;
  int retrans;
  ctcp_segment_t * segment;
} segment_wrap_t;
void make_segment(ctcp_segment_t *segm, uint32_t seqno, uint32_t ackno, uint16_t len, uint32_t flags, uint16_t window){
    segm->seqno = htonl(seqno);
    segm->ackno = htonl(ackno);
    segm->window = htons(window);
    segm->len = htons(len);
    segm->flags = htonl(flags);
    segm->cksum = 0;
}
int send_segment(ctcp_state_t *state, ctcp_segment_t *segm, size_t len, int in_queue){
  fprintf(stderr, "%d\n",state->segments->length);
   fprintf(stderr, "%d\n",(int)len);
   fprintf(stderr, "%d\n",state->cfg->send_window);
   fprintf(stderr, "%d\n",!state->unsent_segments->head);
    fprintf(stderr, "%d\n",!state->unsent_segments->length);
   fprintf(stderr, "\n");
  if(!state->unsent_segments->head && state->segments->length+len < state->cfg->send_window){/*send window is available*/
	ll_add_front(state->segments,segm);
  state->segments->length -= 1;
	state->segments->length += len;/*keep track of the total length of segment queue*/
  conn_send(state->conn, segm, len);
  fprintf(stderr, "Debug message here");
  return 0;
    }else if(in_queue<1){
        ll_add_front(state->unsent_segments,segm);
      return 0;
    }
    return -1;
}



ctcp_state_t *ctcp_init(conn_t *conn, ctcp_config_t *cfg) {
  /* Connection could not be established. */
  if (conn == NULL) {
    return NULL;
  }

  /* Established a connection. Create a new state and update the linked list
     of connection states. */
  ctcp_state_t *state = calloc(sizeof(ctcp_state_t), 1);
  state->next = state_list;
  state->prev = &state_list;
  if (state_list)
    state_list->prev = &state->next;
  state_list = state;

  /* Set fields. */
  state->conn = conn;
  /* FIXME: Do any other initialization here. */
  state->cfg = (ctcp_config_t *) malloc(sizeof(ctcp_config_t));
  state->cfg->recv_window = cfg->recv_window;
  state->cfg->send_window = cfg->send_window;
  state->cfg->timer = cfg->timer;
  state->cfg->rt_timeout = cfg->rt_timeout;
  free(cfg);
  state->cur_seq_no = 1;
  state->ack_no = 1;

  state->segments = (linked_list_t *) malloc(sizeof(linked_list_t));
  state->unsent_segments = (linked_list_t *) malloc(sizeof(linked_list_t));
  state->receiving_data = (linked_list_t *) malloc(sizeof(linked_list_t));
  return state;
}



void ctcp_destroy(ctcp_state_t *state) {
  /* Update linked list. */
  if (state->next)
    state->next->prev = state->prev;

  *state->prev = state->next;
  conn_remove(state->conn);

  /* FIXME: Do any other cleanup here. */
  if(!state->segments){
    ll_node_t * walker = state->segments->head;
    while(walker){
      ll_node_t * tmp = walker;
      walker = walker->next;
      free(tmp);
    }
  }
  free(state->cfg);
  free(state);
  end_client();
}



void ctcp_read(ctcp_state_t *state) {
  /* FIXME */
  //n = state->cfg->send_window;
  int n =1;
  /*fin wait -- stop reading from STDIN*/
  if(state->fin_wait)
    return;
  /*create a segment for input and add it to queue*/
  ctcp_segment_t * segm = (ctcp_segment_t *) malloc(sizeof(ctcp_segment_t));

  char * data = (char *)calloc(MAX_SEG_DATA_SIZE, 1);
  int len = conn_input(state->conn, data, MAX_SEG_DATA_SIZE);
  if(len<0){/*read EOF*/
    make_segment(segm,state->cur_seq_no,state->ack_no,sizeof(ctcp_segment_t),segm->flags | FIN,state->cfg->recv_window);
    
    segm->cksum = cksum(segm, sizeof(ctcp_segment_t));
    len = 0; /*set to 0 for calculation convenience*/
    state->fin_wait = 1;
  } else if(len>0) {/*read input*/
    make_segment(segm,state->cur_seq_no,state->ack_no,len+sizeof(ctcp_segment_t),segm->flags | ACK,state->cfg->recv_window);
      fprintf(stderr, "-----------");
     fprintf(stderr, "%d\n",!state->unsent_segments->head);
    fprintf(stderr, "%d\n",!state->unsent_segments->length);
    fprintf(stderr, "-----------");
    state->cur_seq_no += len; /*max seq no. been sent*/
    strcpy(segm->data, data); //data malloc?
    
    segm->cksum = cksum(segm, len+sizeof(ctcp_segment_t));
  }
  send_segment(state, segm, len+sizeof(ctcp_segment_t),0);
  fprintf(stderr, "here\n");
  //free(data);
  //free(segm);
}



void ctcp_receive(ctcp_state_t *state, ctcp_segment_t *segment, size_t len) {
  /* FIXME */    
  fprintf(stderr, "there\n");
	int n=1;
    uint16_t seg_len = ntohs(segment->len);
    uint32_t no = ntohl(segment->seqno);
    uint32_t rev_ackno = ntohl(segment->ackno);
    
  if(seg_len>sizeof(ctcp_segment_t)){
    /*receiving data*/
	/*check seqno*/
	if(no != state->ack_no)
		return;
	/*output data*/
//	ll_node_t* data = (ll_node_t*)malloc(sizeof(ll_node_t));
	ctcp_segment_t * seg = (ctcp_segment_t *)malloc(seg_len);


	memcpy(seg, segment, seg_len);
	ll_add(state->receiving_data, seg);
	ctcp_output(state);
	if(state->buffer_congested){
		free(segment);  //flow control the sender by not acknowledging segments if there is no buffer space available for conn_output().
		return;
	}
	/*send ACK*/
	ctcp_segment_t * reply = (ctcp_segment_t *)malloc(sizeof(ctcp_segment_t));
	state->ack_no = no+seg_len-sizeof(ctcp_segment_t);
	make_segment(reply, rev_ackno, state->ack_no, sizeof(ctcp_segment_t), reply->flags | ACK, state->cfg->recv_window);
	reply->cksum = cksum(reply, sizeof(ctcp_segment_t));
	conn_send(state->conn,reply,sizeof(ctcp_segment_t)); //returned value?
	free(reply);
    } 
  else if(ntohl(segment->flags) & ACK){
    /*receiving ACK*/
	/*move window & send data*/
  
	ll_node_t * walker = state->segments->head;
	while(walker){
		ctcp_segment_t * seg = (ctcp_segment_t *) walker->object;
		ll_node_t * tmp = walker->next;
		if(rev_ackno > ntohl(seg->seqno)){
			state->segments->length -= ntohs(seg->len);
      free(seg);
			ll_remove(state->segments, walker);
      state->segments->length += 1;
		}
		walker = tmp;
    	}
	ll_node_t * walker_ = state->unsent_segments->head;
  
	while(walker_){
		ctcp_segment_t * seg = (ctcp_segment_t *) walker_->object;
		ll_node_t * tmp = walker_->next;
		if(send_segment(state, seg, ntohs(seg->len),1)>-1){
      free(seg);
			ll_remove(state->unsent_segments, walker_); //segment being sent
		} else {
			break;					  //remaining segments cannot be sent
		}
    
		walker_ = tmp;
	}
  
    } 
    /*receiving FIN*/
    free(segment);
}


void ctcp_output(ctcp_state_t *state) {
  /* FIXME */
  ll_node_t * d =  ll_front(state->receiving_data);
  while(d){
    ctcp_segment_t* seg = d->object;
    size_t len = conn_bufspace(state->conn);
    if(conn_output(state->conn, seg->data, MIN(ntohs(seg->len),len))<0)
	ctcp_destroy(state);
    if(len < ntohs(seg->len)){
        state->buffer_congested = 1;
    	break;
    } else {
    	state->buffer_congested = 0;
    }
    free(seg);
    ll_remove(state->receiving_data, d);
    d =  ll_front(state->receiving_data);
  }
  /*output EOF by conn_output 0*/
}

void ctcp_timer() {
  /* FIXME */
  long time = current_time();
  while(state_list){
    linked_list_t * segments= state_list->segments;
    if(segments == NULL){
      state_list = state_list->next;
      continue;
    }
      
    ll_node_t* segment_node = segments->head;
    while(segment_node != NULL){
      segment_wrap_t * segment = (segment_wrap_t *) segment_node->object;
      if(time - segment->last_sent > state_list->cfg->rt_timeout){
         if(segment->retrans>4){
           ctcp_destroy(state_list);
           break;
         } else {
           conn_send(state_list->conn, segment->segment, ntohs(segment->segment->len));
           segment->last_sent=time;
           segment->retrans += 1;
         }
      }
      segment_node = segment_node->next;
    }
    state_list = state_list->next;
  }
}

