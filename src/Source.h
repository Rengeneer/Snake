#ifndef SOURCE_H
#define SOURCE_H

	/* ---------------- *
	|					|
	|		Consts		|
	|					|
	* ----------------- */

	#define WIDTH 800
	#define HEIGHT 600
	#define CELL_SIZE 20 
	#define UPDATE_INTERVAL 0.00000000000015
	#define WALL_THICKNESS 1
	#define MAX_SNAKE_LENGTH 500
	#define MAX_WALLS 200
	#define SIMULATION_DEPTH 3
	#define BONUS_LIFETIME 100

	/* ---------------- *
	|					|
	|	   STRUCTS		|
	|					|
	* ----------------- */
	
	typedef struct			/*  Структура для хранения координат сегмента змейки  */
	{
	    int x;
	    int y;
	} Segment;
	
	
	typedef struct 			/*  Структура для хранения данных бонуса  */
	{
	    int x;
	    int y;
	    int type;
	    int lifetime;
	} Bonus;
	
	typedef struct 			/*  Структура для хранения узла для поиска пути A*  */
	{
	    int x, y;
	    int f, g, h;
	    int parentX, parentY;
	} Node;
	
	typedef struct 			/*  Структура для хранения состояния симуляции бота  */
	{
	    Segment snake[MAX_SNAKE_LENGTH];
	    int snakeLength;
	    int foodX, foodY;
	    Bonus bonus;
	    int score;
	    int multiplierCount;
	    int walls[MAX_WALLS][2];
	    int wallsCount;
	} SimulationState;

	/* ---------------- *
	|					|
	|	    ENUMS		|
	|					|
	* ----------------- */

	enum Direction 
	{ 
		UP, 
		DOWN, 
		LEFT, 
		RIGHT, 
		NONE
	 };

	enum GameState 
	{ 
		MENU, 
		PLAYING, 
		INFO 
	};

#endif
