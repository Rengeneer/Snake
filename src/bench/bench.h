#ifndef BENCH_H
#define BENCH_H

	#include <stdio.h>

   /* ----------------- *
	|					|
	|	    FLAGS		|
	|					|
	* ----------------- */

	#define BCH_L1 (1 << 0)
	#define BCH_L2 (1 << 1)
	#define BCH_L3 (1 << 2)
	#define BCH_AL (1 << 3)
	#define BCH_MX (1 << 4)
	#define BCH_AVR (1 << 5)

   /* ----------------- *
	|					|
	|	   STRUCTS		|
	|					|
	* ----------------- */

	struct bench_ctx
	{
		FILE* file;
		
		void* _cur_ptr;
		void* _end_ptr;
		void* _buf;

		long long lines;

		int try;
		
		unsigned char try_per_level;
		unsigned char flags;
		
		unsigned char lvl;
		unsigned char depth;
	};

   /* ----------------- *
	|					|
	|	  FLAGS_OPS		|
	|					|
	* ----------------- */

	#define SET_FLAG(FLAG, BUF) ( BUF |= FLAG )
	#define IS_FLAG(FLAG, BUF) ( BUF & FLAG )
	#define SWITCH_FLAG(FLAG, BUF) { BUF ^= FLAG }
	#define CLEAR_FLAG(FLAG, BUF) { BUF &= ~FLAG }

   /* ----------------- *
	|					|
	|	   BENCH		|
	|					|
	* ----------------- */

	void	start_bench	(	struct bench_ctx*	ctx	);

#endif
