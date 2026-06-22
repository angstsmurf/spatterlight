/* findroute.c --- Finds a route from one location to another
 * (C) 2008 David Disher, distribute and use
 * according to GNU GPL, see file COPYING for details.
 */

#include "jacl.h"
#include "language.h"
#include "types.h"
#include "prototypes.h"

/**************************************/
/* Queue functions                    */
/**************************************/

typedef struct QueueNode
{
	int val;
	int val2;
	struct QueueNode *next;
} QueueNode;

typedef struct
{
	QueueNode *head;
	QueueNode *tail;
} Queue;

static void
qInit(Queue *q)
{
	q->head = q->tail = NULL;
}

static void
qDelete(Queue *q)
{
	QueueNode *node, *next;

	for (node = q->head;node != NULL;node = next)
	{
		next = node->next;
		free(node);
	}

	q->head = q->tail = NULL;
}

static int
qIsEmpty(Queue *q)
{
	return (q->head == NULL);
}

/* Returns 0 on success, -1 on OOM. The pathfinder treats an OOM as
 * "no route" and falls through. Without the NULL check the prior
 * code dereferenced a NULL `node` -> SIGSEGV on memory-exhausted
 * CGI workers. */
static int
qAppend(Queue *q, int val, int val2)
{
	QueueNode *node = (QueueNode*) malloc(sizeof(QueueNode));
	if (node == NULL) return -1;
	node->val = val;
	node->val2 = val2;
	node->next = NULL;

	if (q->head == NULL)
	{
		q->head = q->tail = node;
	}
	else
	{
		q->tail->next = node;
		q->tail = node;
	}
	return 0;
}

static void
qPop(Queue *q, int *val, int *val2)
{
	//assert(q->head != NULL);

	*val = q->head->val;
	*val2 = q->head->val2;

	if (q->head == q->tail)
	{
		q->head = q->tail = NULL;
	}
	else
	{
		q->head = q->head->next;
	}
}

/**************************************/
/* Set functions                      */
/**************************************/

/* linked list for hash table */
typedef struct SetNode
{
	int val;
	struct SetNode *next;
} SetNode;

#define SET_HASHSIZE 101

typedef struct
{
	SetNode *node[SET_HASHSIZE];
} Set;

static void
setInit(Set *set)
{
	int n;

	for (n = 0;n < SET_HASHSIZE;n++)
	{
		set->node[n] = NULL;
	}
}

static void
setDelete(Set *set)
{
	int n;

	for (n = 0;n < SET_HASHSIZE;n++)
	{
		SetNode *node, *next;

		for (node = set->node[n];node != NULL;node = next)
		{
			next = node->next;
			free(node);
		}

		set->node[n] = NULL;
	}
}

static int
setHash(int val)
{
	return abs(val) % SET_HASHSIZE;
}

/* Returns 0 on success / already-present, -1 on OOM. */
static int
setAdd(Set *set, int val)
{
	SetNode *node;
	int n = setHash(val);

	/* check if val is already in the set */

	for (node = set->node[n];node != NULL;node = node->next)
	{
		if (node->val == val) { return 0; }
	}

	node = (SetNode*) malloc(sizeof(SetNode));
	if (node == NULL) return -1;
	node->val = val;
	node->next = set->node[n];
	set->node[n] = node;
	return 0;
}

/* returns 1 if the set contains val, otherwise returns 0 */

static int
setContains(Set *set, int val)
{
	SetNode *node;
	int n = setHash(val);

	for (node = set->node[n];node != NULL;node = node->next)
	{
		if (node->val == val) { return 1; }
	}

	return 0;
}

/**************************************/

#define DIR_NONE   -1

int
find_route(int fromRoom, int toRoom, int known)
{
	// KNOWN INDICATES WHETHER THE LOCATION MUST HAVE BEEN
	// VISITED PREVIOUSLY. THIS IS NOT REQUIRED FOR NPCS.

	/* Refuse impossible inputs up-front. Without this the BFS reads
	 * `object[fromRoom]` on the first iteration with no validity
	 * check, which OOBs for fromRoom outside [1, objects]. The same
	 * could happen for toRoom via the n==toRoom equality check. */
	if (fromRoom < 1 || fromRoom > objects || object[fromRoom] == NULL)
		return DIR_NONE;
	if (toRoom < 1 || toRoom > objects || object[toRoom] == NULL)
		return DIR_NONE;

	// CREATE AND INITIALISE THE QUEUE OF LOCATION TO PROCESS
	Queue q;
	qInit(&q);

	// CREATE AND INITIALISE THE SET OF VISITED LOCATIONS
	Set visited;
	setInit(&visited);

	int firstTime = TRUE;

	int result = DIR_NONE;

	// ADD THE STARTING LOCATION TO THE QUEUE FOR PROCESSING
	if (qAppend(&q, fromRoom, DIR_NONE) != 0) {
		setDelete(&visited);
		return DIR_NONE;
	}

	// ADD THE STARTING LOCATION TO THE SET OF VISITED LOCATIONS
	setAdd(&visited, fromRoom);

	// KEEP PROCESSING WHILE THERE ARE LOCATIONS LEFT IN THE QUEUE
	while (!qIsEmpty(&q))
	{
		int n, dir, firstDir;

		// POP THE LOCATION AT THE HEAD OF THE QUEUE (FIRST IN FIRST OUT)
		// n IS THE LOCATION, firstDir IS THE EXIT THAT WAS PUSHED IN
		// THE QUEUE.
		qPop(&q, &n, &firstDir);

		if (n == toRoom)
		{
			// HAVE ARRIVED AT THE DESTINATION, RETURN THE FIRST
			// DIRECTION WALKED IN TO GET TO THE DESTINATION.
			result = firstDir;
			break;
		}

		// LOOP THROUGH ALL THE EXITS OF THE CURRENT LOCATION
		for (dir = 0;dir < 12 ;dir++)
		{
			// SET THE DESTINATION TO THE LOCATION THIS DIRECTION
			// LEADS TO.
			int dest = object[n]->integer[dir];

			if (dest < 1 || dest > objects) continue;

			if (object[dest] == NULL) continue;

			if (dest != NOWHERE && !setContains(&visited, dest))
			{
				if (!known || (object[dest]->attributes & KNOWN)) {
					// PUT THE DESTINATION LOCATION INTO THE QUEUE
					// (IF THIS IS THE STARTING LOCATION) 
					// AND STORE THE DIRECTION THAT LEAD TO IT
					// firstTime IS CHECKED AS THE LOCATIONS REACHED
					// FROM THE STARTING LOCATION ARE THE ONLY
					// ONES THAT REALLY MATTER AS THIS FUNCTION
					// ONLY RETURNS THE FIRST STEP, NOT THE FULL
					// PATH. firstDir IS THE ORIGINAL DIRECTION
					// SET OUT IN FROM THE STARTING LOCATION
					qAppend(&q, dest, (firstTime ? dir : firstDir));

					// ADD THE LOCATION TO THE SET OF LOCATIONS VISITED
					setAdd(&visited, dest);
				}
			}
		}

		firstTime = FALSE;
	}

	setDelete(&visited);
	qDelete(&q);

	return result;
}
