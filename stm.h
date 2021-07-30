/*
 *
 *	Copyright (C) 2015 Yizhou Shan
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _SYZ_STM_H_
#define _SYZ_STM_H_

#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "atomic.h"

//#define STM_DEBUG
//#define STM_STATISTICS

#ifdef STM_DEBUG
#define PRINT_DEBUG(format, ...)	printf(format, ##__VA_ARGS__)
#else
#define PRINT_DEBUG(format, ...)
#endif

/*
 *	STM API
 *
 *	Note that with this STM implementation, you do not
 *	need to specify any memory region as Transactional Memory.
 *	When you access the shared object using STM API in transaction,
 *	for example, TM_READ() or TM_WRITE(), the system will mark the
 *	memory region that the shared object cover as TM automatically.
 *	And, yes, the granularity is one single byte, each byte in TM
 *	has a ownership record depict itself.
 */

#define __TM_START__			\
	{				\
		stm_start();		\
		setjmp(thread_tx->jb);	\
	}

#define __TM_END__				\
	{					\
		stm_tx_t *tx = tls_get_tx();	\
		if (!tx_committed()) {		\
			stm_restart();		\
			longjmp(tx->jb, 1);	\
		}				\
	}

#define TM_THREAD_INIT()	stm_thread_init()
#define TM_INIT()		stm_init()
#define TM_ABORT()		stm_abort()
#define TM_COMMIT()		stm_commit()
#define TM_VALIDATE()		stm_validate()
#define TM_BARRIER()		stm_barrier()

#define TM_READ_CHAR(a)		stm_read_char(&(a))
#define TM_WRITE_CHAR(a,v)	stm_write_char(&(a),v)

#define TM_READ(TMOBJECT)					\
	({							\
		int __i; char __c;				\
		typeof(TMOBJECT)  __ret;			\
		char *__retp = (char *)&(__ret);		\
		char *__addr = (char *)&(TMOBJECT);		\
		for (__i = 0; __i < sizeof(TMOBJECT); ) {	\
			__c = stm_read_char(__addr);		\
			*__retp = __c;				\
			__addr++; __retp++; __i++;		\
		}						\
		__ret;						\
	})

#define TM_WRITE(TMOBJECT, VALUE)				\
	{							\
		int __i; char __c;				\
		typeof(TMOBJECT) __val = (VALUE);		\
		char *__valp = (char *)&(__val);		\
		char *__addr = (char *)&(TMOBJECT);		\
		for (__i = 0; __i < sizeof(TMOBJECT); ) {	\
			__c = *((char *)__valp);		\
			stm_write_char(__addr, __c);		\
			__addr++; __valp++; __i++;		\
		}						\
	}

/*
 * Contention Policy
 * 
 * AGGRESSIVE is an obstruction-free implementation.
 * The transaction just abort all other transactions
 * that holds the orec of the data that this transaction
 * want to access.
 *
 * POLITE is an more gentle implementation.
 * The transaction will wait a moment when encounter a
 * conflict. The waiting time has a exponential growth.
 */
enum {
	CM_AGGRESSIVE,
	CM_POLITE
};

#define DEFAULT_CM_POLICY	CM_AGGRESSIVE

/*
 * Transaction Status
 * A transaction is ACTIVE once it starts, until ABORT or COMMIT
 * A transaction is COMMITING when TM_COMMIT() is flushing dirty data.
 * A transaction is COMMITED when TM_COMMIT() has finished and succeed.
 * A transaction is ABORT when two actions happened:
 *  i)  call TM_ABORT() itself
 *  ii) ABORT by the conflicting transaction
 */
enum STM_STATUS {
	STM_NULL,
	STM_ACTIVE,
	STM_COMMITING,
	STM_COMMITED,
	STM_ABORT
};

enum STM_ABORT_REASON {
	STM_SELF_ABORT	= 1,
	STM_ENEMY_ABORT	= 2
};

/*
 * Read Data Entry
 * A single byte that have been read during a
 * transaction has read data entry.
 */
struct r_entry {
	volatile char *addr;
	struct orec *rec;
	struct r_entry *next;
};

/*
 * Write Data Entry
 * A single byte that have been written to during a
 * transaction has a write data entry.
 */
struct w_entry {
	volatile void *addr;
	struct orec *rec;
	struct w_entry *next;
};

/*
 * Read Data Set
 * All the data have been read during a transaction
 * form the read data set of that transaction.
 */
struct read_set {
	struct r_entry *head;
	int nr_entries;
};

/*
 * Write Data Set
 * All the data have been written to during a transaction
 * form the write data set of that transaction.
 */
struct write_set {
	struct w_entry *head;
	int nr_entries;
};

/*
 * Transaction Descriptor
 * Each transaction have a descriptor depicts
 * its status, data sets and so on.
 */
typedef struct stm_tx {
	atomic_t	status;
	pthread_t	tid;
	jmp_buf jb;
	int version;
	struct read_set  rs;
	struct write_set ws;
	int start_tsp;	/* Start TimeStamp */
	int end_tsp;	/* End TimeStamp */
	int abort_reason;
#ifdef STM_STATISTICS
	int nr_aborts;
#endif
}stm_tx_t;

/*
 * Ownership Record
 * Every byte in TM has its own record. The size of
 * the record is 8+4+4=16 bytes in 64-bit system.
 * Therefore, if you have 1M transactional memory,
 * the total records will consume 16M.
 */
struct orec {
	struct stm_tx *owner;
	int version;
	char old;
	char new;
	char pad[2];
};

#define DEFINE_PER_THREAD(TYPE, NAME)	__thread TYPE NAME
#define DECLARE_PER_THREAD(TYPE, NAME) extern __thread TYPE NAME
#define GET_TX(tx)	struct stm_tx *tx = tls_get_tx()

DECLARE_PER_THREAD(struct stm_tx *, thread_tx);

void stm_restart(void);
void stm_start(void);
void stm_abort(void);
int stm_commit(void);
int stm_validate(void);
char stm_read_char(volatile void *addr);
void stm_write_char(volatile void *addr, char new);
int stm_contention_manager(struct stm_tx *enemy);

struct stm_tx *tls_get_tx(void);
void tls_set_tx(struct stm_tx *nex_tx);

/*
 * Unlike SigSTM, we can NOT guarantee strong isolation
 * between transactional and non-transactional code.
 * tm_barrier() will saddle non-transactional code with
 * additional overheads. Because processor can NOT classify
 * where the memory access come from, TM or non-TM. So, all
 * memory access can NOT across barrier no matter who you are.
 */
static inline void stm_barrier(void)
{
	asm volatile ("mfence":::"memory");
}

#define DELAY_LOOPS	50
static inline void stm_wait(void)
{
	int i;
	for (i = 0; i < DELAY_LOOPS; i++) {
		asm ("nop":::"memory");//Compiler Barrier
	}
}


static inline void
OREC_SET_OWNER(struct orec *r, struct stm_tx *t)
{
	r->owner = t;
}

static inline stm_tx_t *
OREC_GET_OWNER(struct orec *r)
{
	return r->owner;
}

static inline void
OREC_SET_OLD(struct orec *r, char old)
{
	r->old = old;
}

static inline void
OREC_SET_NEW(struct orec *r, char new)
{
	r->new = new;
}

static inline char
OREC_GET_OLD(struct orec *r)
{
	return r->old;
}

static inline char
OREC_GET_NEW(struct orec *r)
{
	return r->new;
}

static inline void
stm_set_status(int status)
{
	GET_TX(tx);
	atomic_write(&tx->status, status);
}

static inline int
stm_get_status(void)
{
	GET_TX(tx);
	return atomic_read(&tx->status);
}

static inline void
stm_set_abort_reason(int reason)
{
	GET_TX(tx);
	atomic_write(&tx->abort_reason, reason);
}

static inline int
stm_get_abort_reason(void)
{
	GET_TX(tx);
	return atomic_read(&tx->abort_reason);
}

static inline void
stm_set_status_tx(struct stm_tx *tx, int status)
{
	atomic_write(&tx->status, status);
}

static inline int
stm_get_status_tx(struct stm_tx *tx)
{
	return atomic_read(&tx->status);
}

static inline void
stm_set_abort_reason_tx(struct stm_tx *tx, int reason)
{
	atomic_write(&tx->abort_reason, reason);
}

static inline int
stm_get_abort_reason_tx(struct stm_tx *tx)
{
	return atomic_read(&tx->abort_reason);
}

static inline int
tx_committed(void)
{
	return stm_get_status() == STM_COMMITED;
}

#endif /* _SYZ_STM_H_ */
