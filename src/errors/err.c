#include "err.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
	#include <execinfo.h>
	#include <signal.h>
#elif _WIN32
	#include <windows.h>
#endif

static void _print_bt(void)
{
	#ifdef __linux__
		void* bt[BT_MAX_FRAMES];
		
		size_t size = backtrace(bt, BT_MAX_FRAMES);		/*	Получение адрессов	*/
		char **strings = backtrace_symbols(bt, size);	/*	Получение символов	*/
		fprintf(stderr, "----- BACKTRACE START -----\nFRAMES COUNT - %zu\n", size);		/*	zu для size_t	*/
		for (size_t i = 0; i < size; i++)
		{
			if (strings != NULL)
			{
				fprintf(stderr, "(%zu) %s \n", i, strings[i]);
			} 
			else
			{
				fprintf(stderr, "(%zu) ??? \n", i);
			}
			
		}
		fprintf(stderr, "----- BACKTRACE END -----\n");
		free(strings);
	#elif _WIN32
		void* bt[MAX_FRAMES];
		
		HANDLE cur_proc = GetCurrentProcess();			/*	Получаем handler. Нужен для SymInitialize	*/
		SymInitialize(cur_proc, NULL, TRUE);			/*	Инициализация обработчика символов	*/

		USHORT frames = CaptureStackBackTrace(0, MAX_FRAMES, stack, NULL);		/*	Сбор стэка	*/	
		SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);		

		symbol->MaxNameLen = 255;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

		fprintf(stderr, "----- BACKTRACE START -----\nFRAMES COUNT - %hu\n", frames);

		for (USHORT i = 0; i < frames; i++)
		{
			SymFromAddr(cur_proc, (DWORD64)(bt[i]), 0, symbol);

			if (symbol->Name[0])
			{
				fprintf(stderr, "(%hu) 0x%0llX %s/n", i, (unsigned long long)bt[i], symbol->Name);	
			}
			else
			{
				fprintf(stderr, "(%hu) 0x%0llX ???\n", i, (unsigned long long)bt[i]);
			}
			
		}
		fprintf(stderr, "----- BACKTRACE END -----\n");
		free(symbol);
		SymCleanup(cur_proc);	
	#endif
}

#ifdef __linux__
	static void crit_handler(int sig, __attribute__((unused)) siginfo_t* info, __attribute__((unused)) void* ctx)
	{
		char* signame = "UNKNOWN";
		switch (sig)
		{
			case SIGSEGV:
				signame = "SIGSEGV";		/*	Ошибка сегментации	*/
				break;
			case SIGABRT:
				signame = "SIGABRT";		/*	Вызван abort() (аварийная остановка)	*/
				break;
			case SIGFPE:
				signame = "SIGFPE";			/*	Фатальная арифметическая ошибка	*/
				break;
			case SIGILL:
				signame = "SIGILL";			/*	Попытка выполнения некорректной инструкции	*/
				break;
			case SIGBUS:
				signame = "SIGBUS";			/*	Вызыв адресса не существующего в компоненте управления доступа к памяти (MMU)	*/
				break;
		}

		fprintf(stderr, "\n --- ПОЛУЧЕН СИГНАЛ %d (%s) --- \n", sig, signame);
		_print_bt();

		exit(128 + sig);		/*	Сдвиг до сигнала	*/
	}
#elif _WIN32
	static LONG WINAPI crit_handler(EXCEPTION_POINTERS* info)
	{
		fprintf(stderr, "\n --- ВЫЗВАННО ИСКЛЮЧЕНИЕ 0x%08X --- \n", info->ExceptionRecord->ExceptionCode);
		_print_bt();
		return EXCEPTION_EXECUTE_HANDLER;
	}
#endif

void init_crit_handlers(void)
{
	#ifdef __linux__
		struct sigaction sa = {0};

		sa.sa_sigaction = crit_handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;

		sigaction(SIGSEGV, &sa, NULL);
		sigaction(SIGABRT, &sa, NULL);
		sigaction(SIGFPE, &sa, NULL);
		sigaction(SIGILL, &sa, NULL);
		sigaction(SIGBUS, &sa, NULL);
		
	#elif _WIN32
		SetUnhandledExceptionFilter(crit_handler);
	#endif
}
