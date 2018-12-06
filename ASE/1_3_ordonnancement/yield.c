#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "include/yield.h"
#include "include/tmr.h"
#include "include/hardware.h"
#include "include/hwconfig.h"

struct ctx_s* current_ctx = NULL;

int main(int argc, char** argv) {

	create_ctx(65536,doit,"ABCDEF");
	create_ctx(65536,doit,"1234567890");
	create_ctx(65536,doit,"hello");
	create_ctx(65536,doit,"Bye Bye");

	start_sched(yield);

	return(1);
}

void doit(void *arg) {
	char* str = arg;
	int i, j;
	for (i=0; str[i] != '\0'; i++) {

		for(j=0;j<100000000;j++);
		putchar(str[i]);
		printf("\n");
	}
}

struct ctx_s* create_ctx(int stack_size, func_t f, void* args) {

	struct ctx_s* new_ctx = malloc(sizeof(struct ctx_s));

	/* fail */
	if ( ! init_ctx(new_ctx, stack_size, f, args) ) {
		free(new_ctx);
		return NULL;
	} else {

		irq_disable();

		if (current_ctx == NULL) {

			current_ctx = new_ctx;
			current_ctx->next = new_ctx;	/* cycle */
		} else {

			new_ctx->next = current_ctx->next;
			current_ctx->next = new_ctx;
		}

		irq_enable();
	}

	return new_ctx;
}

int init_ctx(struct ctx_s* ctx, int stack_size, func_t f, void* args) {

	ctx->stack = malloc(sizeof(char) * stack_size);
	ctx->args = args;
	ctx->magic = MAGIC;
	ctx->state = ctx_RDY;
	ctx->entrypoint = f;
	ctx->rsp = ctx->stack + stack_size - sizeof(void*);
	ctx->rbp = ctx->rsp;

	return (ctx->stack != NULL);
}

void yield() {
	_out(TIMER_ALARM,0xFFFFFFFE);  /* alarm at next tick (at 0xFFFFFFFF) */
	irq_disable();
	switch_to_ctx(current_ctx->next);
}

void switch_to_ctx(struct ctx_s* ctx) {

	assert(ctx->magic == MAGIC);

	if (current_ctx != NULL) {
		asm("mov %%rsp, %0;" : "=r"(current_ctx->rsp)::);
    asm("mov %%rbp, %0;" : "=r"(current_ctx->rbp)::);
	}

	current_ctx = ctx;

	asm("mov %0, %%rsp;" : :"r"(current_ctx->rsp):);
  asm("mov %0, %%rbp;" : :"r"(current_ctx->rbp):);

	irq_enable();

  if (current_ctx->state == ctx_RDY) {
		start_current_ctx();
  }

    return;
}

void start_current_ctx() {

	struct ctx_s * ctx = current_ctx;

	ctx->state = ctx_RUN;
 	ctx->entrypoint(ctx->args);
 	ctx->state = ctx_TERM;

	irq_disable();

	if(ctx != ctx->next) {

		struct ctx_s* last_ctx = ctx;

		while(last_ctx->next != ctx) {
			last_ctx = last_ctx->next;
		}

		last_ctx->next = ctx->next;
		free(ctx);

		irq_enable();

		yield();

 	} else {
		 free(ctx);
		 exit(EXIT_SUCCESS);
 	}
}
