#include "bench.h"
#include "../snake/Source.h"
#include "../errors/err.h"
#include <GLFW/glfw3.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

static void prepare_buffer(struct bench_ctx* ctx);
static void next_line(struct bench_ctx* ctx);
static void buf_put_level_info(struct bench_ctx* ctx);
static unsigned char choose_next_lvl(struct bench_ctx* ctx);
static void prepare_file(struct bench_ctx* ctx);
static void dump_buf(struct bench_ctx* ctx);
 
size_t lsize = 0;

void start_bench(struct bench_ctx* ctx)
{
	assert(ctx != NULL && ctx->try_per_level > 0 && !IS_FLAG(BCH_MX, ctx->flags));
	prepare_buffer(ctx);

	GLFWwindow* window = prepare_window();
	assert(window != NULL);

	while (ctx->lvl != 0)
	{
		for (unsigned char try = 0; try < ctx->try_per_level; try++)
		{
			ctx->try = try + 1;
			for (unsigned char depth = 3; depth <= 5; depth++)
			{
				ctx->depth = SIMULATION_DEPTH = depth;
				bench_main(window, ctx->lvl);
				buf_put_level_info(ctx);
			}
		}
		ctx->lvl = choose_next_lvl(ctx);
	}
	
	dump_buf(ctx);
	
	glfwTerminate();
}

int main(int argc, char* argv[])
{
	init_crit_handlers();
	unsigned char flags = 0;
	SET_FLAG(BCH_L1, flags);
	struct bench_ctx ctx = {0};
	ctx.try_per_level = 1;
	ctx.flags = flags;

	start_bench(&ctx);
	
	return 0;
}

static void prepare_buffer(struct bench_ctx* ctx)
{
	char lvl_count = 0;
	char lvl_set = 0;
	
	if (IS_FLAG(BCH_AL, ctx->flags))
	{
		lvl_set = 1;
		ctx->lvl = 1;
		lvl_count = 3;
	}
	else
	{
		for (unsigned char off = 0; off < 3; off++)
		{
			if (IS_FLAG(BCH_L1 << off, ctx->flags))
			{
				if (!lvl_set) { lvl_set = 1; ctx->lvl = off + 1; }
				lvl_count++;
			}
		}	
	}

	assert(lvl_count > 0);
	
	if (IS_FLAG(BCH_MX, ctx->flags))
	{
		lsize = 9 * sizeof(double);
		void* buf = calloc(3 * lvl_count * lsize, 1);
		assert(buf != NULL);
		ctx->_buf = ctx->_cur_ptr = buf;
	}
	else
	{
		lsize = 10 * sizeof(double);
		void* buf = calloc(3 * lvl_count * ctx->try_per_level * lsize, 1);
		assert(buf != NULL);
		ctx->_buf = ctx->_cur_ptr = buf;
	}

	ctx->lines = 3 * lvl_count * ctx->try_per_level;
}

static void next_line(struct bench_ctx* ctx)
{
	if (IS_FLAG(BCH_MX, ctx->flags))
	{
		ctx->_cur_ptr = (unsigned char*)ctx->_cur_ptr + 9 * sizeof(double);
	}
	else
	{
		ctx->_cur_ptr = (unsigned char*)ctx->_cur_ptr + 10 * sizeof(double);
	}
}

static void buf_put_level_info(struct bench_ctx* ctx)
{
	double* ptr = (double*)ctx->_cur_ptr;
	ptr[0] = (double)ctx->lvl;
	ptr[1] = (double)ctx->depth;
	ptr[2] = (double)ctx->try;
	ptr[3] = (double)snakeLength;
	ptr[4] = (double)score;
	ptr[5] = (double)foodEaten;
	ptr[6] = start_time;
	ptr[7] = gameover_time;
	ptr[8] = update_count;
	ptr[9] = avg_fps;
	next_line(ctx);
}

static unsigned char choose_next_lvl(struct bench_ctx* ctx)
{
	unsigned char lvl = ctx->lvl;
	if (lvl == 3) return 0;
	if (IS_FLAG(BCH_AL, ctx->flags))
	{
		return lvl + 1;
		
	}
	for(;lvl != 3; lvl++)
	{
		if (IS_FLAG(BCH_L1 << lvl, ctx->flags))
		{
			return lvl + 1;
		}
	}
	
	return 0;
}

static void prepare_file(struct bench_ctx* ctx)
{
	if (ctx->file == NULL)
	{
		FILE* out = fopen("out.txt", "a");
		assert(out != NULL);
		ctx->file = out;
	}
}

static void dump_buf(struct bench_ctx* ctx)
{ 
	prepare_file(ctx);
	ctx->_cur_ptr = ctx->_buf;

	unsigned char level = 0;
	unsigned char depth = 0;
	unsigned char try = 0;

	int score = 0;
	int foode = 0;
	int length = 0;

	double st_time;
	double end_time;
	double update_c;
	double avg;
	
	fprintf(ctx->file, "----- BENCHMARK START -----\n");
	
	for (long long line = 0; line < ctx->lines; line++)
	{
		fputc('\n', ctx->file);

		double* ptr = ctx->_cur_ptr;
		
		level = (unsigned char)ptr[0];
		depth = (unsigned char)ptr[1];
		try = (unsigned char)ptr[2];
		
		fprintf(ctx->file, "> LVL: %hhu\n> DEPTH: %hhu\n> TRY: %hhu\n", level, depth, try);
		
		fputs("> RESULTS:\n", ctx->file);

		length = (int)ptr[3];
		score = (int)ptr[4];
		foode = (int)ptr[5];
		st_time = ptr[6];
		end_time = ptr[7];
		update_c = ptr[8];
		avg = ptr[9];
		
		fprintf(ctx->file, "\tLength: %d\n\tScore: %d\n\tFood eaten: %d\n\tStart time: %f\n\tGame over time: %f\n\tUpdate count: %f\n\tAVG fps:%f\n", 
		length, score, foode, st_time, end_time, update_c, avg);
		next_line(ctx);
	}
	fclose(ctx->file);
	free(ctx->_buf);
	ctx->file = NULL;
	ctx->_buf = 0;
}
