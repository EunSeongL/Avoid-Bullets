#include "device_driver.h"
#include <stdlib.h>

#define LCDW			(320)
#define LCDH			(240)
#define X_MIN	 		(0)
#define X_MAX	 		(LCDW - 1)
#define Y_MIN	 		(20)
#define Y_MAX	 		(LCDH - 1)

#define TIMER_PERIOD	(10)

#define BULLET_STEP_1	(1)
#define BULLET_STEP_2	(2)
#define BULLET_STEP_3	(3)

#define BULLET_SIZE_X	(5)
#define BULLET_SIZE_Y	(5)

#define MAX_BULLET      (20)
#define MAX_BIG_BULLET  (5)

#define USER_STEP		(5)
#define USER_SIZE_X		(5)
#define USER_SIZE_Y		(5)

#define ENEMY_STEP      (1)
#define ENEMY_SIZE_X	(5)
#define ENEMY_SIZE_Y	(5)

#define BIG_STEP        (1)
#define BIG_SIZE_X	    (7)
#define BIG_SIZE_Y	    (7)

#define ITEM_SIZE_X     (5)
#define ITEM_SIZE_Y     (5)

#define BACK_COLOR		(5)
#define BULLET_COLOR	(0)
#define USER_COLOR		(1)
#define LIFE_COLOR      (2)
#define ENEMY_COLOR     (4)

#define GAME_OVER		(1)

extern volatile int TIM1_expired;
extern volatile int TIM2_expired;
extern volatile int TIM4_expired;
extern volatile int USART1_rx_ready;
extern volatile int USART1_rx_data;
extern volatile int Jog_key_in;
extern volatile int Jog_key;

typedef struct
{
    int x, y;
    int w, h;
    int dir_x, dir_y;
    int alive;
    int ci;
    int type;
} QUERY_DRAW;


static QUERY_DRAW bullets[MAX_BULLET];
static QUERY_DRAW user;
static QUERY_DRAW big[MAX_BIG_BULLET];
static QUERY_DRAW enemy;
static QUERY_DRAW life;

static int enemy_respawn_timer = 0;
static int life_spawn_timer = 0;
static int life_exist_timer = 0;   
static int life_active = 0;         

static int score = 0;
static int sec_counter = 0;
static int stage = 1;
static int live;
int bgm_index = 0;
static unsigned short color[] = {RED, YELLOW, GREEN, BLUE, WHITE, BLACK};

static void Draw_Bullet(QUERY_DRAW * b)
{
    if(b->alive){
        Lcd_Draw_Box(b->x, b->y, b->w, b->h, color[b->ci]);
    }
}

static void Draw_Object(QUERY_DRAW * obj)
{
	Lcd_Draw_Box(obj->x, obj->y, obj->w, obj->h, color[obj->ci]);
}

//--------------------------------------------------------------------------------
static void Reset_Life_Spawn(void)
{
    life_spawn_timer = (rand() % 70 + 30);
}

static void Spawn_Life_Item(void)
{
    life.x = rand() % (X_MAX - ITEM_SIZE_X);
    life.y = Y_MIN + rand() % ((Y_MAX - Y_MIN) - ITEM_SIZE_Y);
    life.w = ITEM_SIZE_X;
    life.h = ITEM_SIZE_Y;
    life.ci = LIFE_COLOR;
    life.alive = 1;
    life_exist_timer = 100;
    life_active = 1;
}

//--------------------------------------------------------------------------------
static void Bullet_Init(QUERY_DRAW *b);

static int Check_Collision(void)
{
    int i;
    for (i = 0; i < MAX_BULLET; i++) {
        if (bullets[i].alive)
        {
            if ((bullets[i].x < user.x + user.w) && (bullets[i].x + bullets[i].w > user.x) &&
                (bullets[i].y < user.y + user.h) && (bullets[i].y + bullets[i].h > user.y))
            {
                bullets[i].ci = BACK_COLOR;
                Draw_Bullet(&bullets[i]);
                Bullet_Init(&bullets[i]);
                live--;         
                Uart1_Printf("Crush bullet(-1)\nLife : %d\n", live);        
            }
        }
    }

    for (i = 0; i < MAX_BIG_BULLET; i++) {
        if (big[i].alive)
        {
            if ((big[i].x < user.x + user.w) && (big[i].x + big[i].w > user.x) &&
                (big[i].y < user.y + user.h) && (big[i].y + big[i].h > user.y))
            {
                big[i].ci = BACK_COLOR;
                Draw_Bullet(&big[i]);
                Bullet_Init(&big[i]);
                live-=2;         
                Uart1_Printf("Crush big bullet(-2)\nLife : %d\n", live);        
            }
        }
    }

    if(enemy.alive){
        if ((enemy.x < user.x + user.w) && (enemy.x + enemy.w > user.x) &&
            (enemy.y < user.y + user.h) && (enemy.y + enemy.h > user.y))
        {   
            enemy.alive = 0;
            enemy_respawn_timer = (rand() % 50) + 70;
            live--; 
            Uart1_Printf("Crush enemy(-1)\nLife : %d\n", live);
        }
    }
    
    if(life.alive){
        if ((life.x < user.x + user.w) && (life.x + life.w > user.x) &&
            (life.y < user.y + user.h) && (life.y + life.h > user.y))
        {   
            life.ci = BACK_COLOR;
            Draw_Bullet(&life);
            life.alive = 0;
            life_active = 0;  
            Reset_Life_Spawn();
            live++;
            if(live > 5)
            {
                live = 5;
                Uart1_Printf("Alreagy Get MAX LIFE 5\n");
            }
            else{
                Uart1_Printf("Get heal Item(+1)\nLife : %d\n", live);
            }
        }
    }

    if (live <= 0) {
        return GAME_OVER;
    }
    
    return 0;
}

static void Bullet_Init(QUERY_DRAW *b)
{
    b->w = (b->type == 1) ? BIG_SIZE_X : BULLET_SIZE_X;
    b->h = (b->type == 1) ? BIG_SIZE_Y : BULLET_SIZE_Y;
    b->ci = BULLET_COLOR;
    b->alive = 1;

    int edge = rand() % 4;

    switch(edge)
    {
        case 0: // 위쪽
            b->x = rand() % (X_MAX - b->w);
            b->y = Y_MIN;
            break;
        case 1: // 아래쪽
            b->x = rand() % (X_MAX - b->w);
            b->y = Y_MAX;
            break;
        case 2: // 왼쪽
            b->x = X_MIN;
            b->y = rand() % (Y_MAX - b->h);
            break;
        case 3: // 오른쪽
            b->x = X_MAX;
            b->y = rand() % (Y_MAX - b->h);
            break;
    }

    int dir = rand() % 3;

    switch(edge)
    {
        case 0: // 위
            if(dir == 0) { b->dir_x = 1; b->dir_y = 1; }
            else if(dir == 1) { b->dir_x = 0; b->dir_y = 1; }
            else { b->dir_x = -1; b->dir_y = 1; }
            break;
        case 1: // 아래
            if(dir == 0) { b->dir_x = -1; b->dir_y = -1; }
            else if(dir == 1) { b->dir_x = 0; b->dir_y = -1; }
            else { b->dir_x = 1; b->dir_y = -1; }
            break;
        case 2: // 왼쪽
            if(dir == 0) { b->dir_x = 1; b->dir_y = -1; }
            else if(dir == 1) { b->dir_x = 1; b->dir_y = 0; }
            else { b->dir_x = 1; b->dir_y = 1; }
            break;
        case 3: // 오른쪽
            if(dir == 0) { b->dir_x = -1; b->dir_y = -1; }
            else if(dir == 1) { b->dir_x = -1; b->dir_y = 0; }
            else { b->dir_x = -1; b->dir_y = 1; }
            break;
    }
}

static void Bullets_Init_All(void)
{
    int i;
    for (i = 0; i < MAX_BULLET - 1; i++) {
        bullets[i].type = 0;
        Bullet_Init(&bullets[i]);
    }
    for (i = 0; i < MAX_BIG_BULLET - 1; i++) {
        big[i].type = 1;
        Bullet_Init(&big[i]);
    }
}

static int Bullet_Move(QUERY_DRAW *b)
{
    if(!b->alive) return 0;

    if(stage == 1){
        b->x += b->dir_x * BULLET_STEP_1;
        b->y += b->dir_y * BULLET_STEP_1;
    }
    if(stage == 2){
        b->x += b->dir_x * BULLET_STEP_2;
        b->y += b->dir_y * BULLET_STEP_2;
    }
    if(stage == 3){
        b->x += b->dir_x * BULLET_STEP_3;
        b->y += b->dir_y * BULLET_STEP_3;
    }

    if(b->x < X_MIN || b->x > X_MAX || b->y < Y_MIN || b->y > Y_MAX)
    {
        Bullet_Init(b);
    }
    Draw_Bullet(b);

    return Check_Collision();
}

static int Big_Move(QUERY_DRAW *b)
{
    b->x += b->dir_x * BULLET_STEP_1;
    b->y += b->dir_y * BULLET_STEP_1;

    if(b->x < X_MIN || b->x > X_MAX || b->y < Y_MIN || b->y > Y_MAX)
    {
        Bullet_Init(b);
    }
    Draw_Bullet(b);

    return Check_Collision();
}


static int Enemy_Move(void)
{
    if(enemy.x < user.x)
        enemy.x += ENEMY_STEP;
    else if(enemy.x > user.x)
        enemy.x -= ENEMY_STEP;

    if(enemy.y < user.y)
        enemy.y += ENEMY_STEP;
    else if(enemy.y > user.y)
        enemy.y -= ENEMY_STEP;

    if(enemy.x < X_MIN) enemy.x = X_MIN;
    if(enemy.x + enemy.w > X_MAX) enemy.x = X_MAX - enemy.w;
    if(enemy.y < Y_MIN) enemy.y = Y_MIN;
    if(enemy.y + enemy.h > Y_MAX) enemy.y = Y_MAX - enemy.h;

    Draw_Bullet(&enemy);

    return Check_Collision();
}

static void k0(void)
{
	if(user.y > Y_MIN) user.y -= USER_STEP;
}

static void k1(void)
{
	if(user.y + user.h < Y_MAX) user.y += USER_STEP;
}

static void k2(void)
{
	if(user.x > X_MIN) user.x -= USER_STEP;
}

static void k3(void)
{
	if(user.x + user.w < X_MAX) user.x += USER_STEP;
}

static int USER_Move(int k)
{
	// UP(0), DOWN(1), LEFT(2), RIGHT(3)
	static void (*key_func[])(void) = {k0, k1, k2, k3};
	if(k <= 3) key_func[k]();
	return Check_Collision();
}

static void Game_Init(void)
{
	Lcd_Clr_Screen();
    Reset_Life_Spawn();
    life_active = 0; life.alive = 0;
	user.x = LCDW/2; user.y = LCDH/2; user.w = USER_SIZE_X; user.h = USER_SIZE_Y; user.ci = USER_COLOR;
    score = 0; stage = 1; live = 5;
    life_spawn_timer = 0; life_exist_timer = 0; sec_counter = 0;
    TIM1_expired = 0; TIM2_expired = 0; TIM4_expired = 0;
    bgm_index = 0;

    Bullets_Init_All();
    Bullet_Init(&enemy);
 
    Lcd_Draw_Box(0, 0, X_MAX, 16, color[4]);
	Lcd_Draw_Box(user.x, user.y, user.w, user.h, color[user.ci]);
    //Lcd_Draw_Box(enemy.x, enemy.y, enemy.w, enemy.h, color[enemy.ci]);
    TIM4_Repeat_Interrupt_Enable(1, TIMER_PERIOD*10);
    TIM2_Repeat_Interrupt_Enable(1, TIMER_PERIOD*10);
    Uart1_Printf("\nStage %d\nLife %d\n", stage, live);
    
}

#define BASE  (500) // msec 단위


enum key {
    C1, C1_, D1, D1_, E1, F1, F1_, G1,
    G1_, A1, A1_, B1, C2, C2_, D2, D2_,
    E2, F2, F2_, G2, G2_, A2, A2_, B2
};

enum note {
    N16 = BASE/4, N8 = BASE/2, N4 = BASE,
    N2 = BASE*2, N1 = BASE*4
};

static const unsigned short tone_value[] = {
    523, 554, 587, 622, 659, 698, 739, 783,
    830, 880, 932, 987, 1046, 1108, 1174, 1244,
    1318, 1396, 1479, 1567, 1661, 1760, 1864, 1975
};

const int song1[][2] = {
    {E1, N8}, {E1, N8}, {B1, N8}, {C2, N8}, {D2, N4},
    {C2, N8}, {B1, N8}, {A1, N4},
    {A1, N8}, {C2, N8}, {E2, N4}, {D2, N8}, {C2, N8}, {B1, N4},
    {C2, N8}, {D2, N8}, {E2, N4}, {C2, N8}, {A1, N4},
    {A1, N8}, {D2, N8}, {F2, N4}, {A2, N8}, {G2, N8}, {F2, N4},
    {E2, N8}, {C2, N8}, {E2, N8}, {D2, N8}, {C2, N4}, {B1, N4},
    {B1, N8}, {C2, N8}, {D2, N4}, {E2, N8}, {C2, N8}, {A1, N4}, {A1, N4}
};
const int song_length = sizeof(song1) / sizeof(song1[0]);

void BGM(void)
{
    if (bgm_index >= song_length) bgm_index = 0;

    unsigned char tone = song1[bgm_index][0];
    int duration = song1[bgm_index][1];

	TIM3_Out_Freq_Generation(tone_value[tone]);
    TIM1_Delay(1, duration); 
    bgm_index++;
}


void System_Init(void)
{
	Clock_Init();
	LED_Init();
	Key_Poll_Init();
	Uart1_Init(115200);

	SCB->VTOR = 0x08003000;
	SCB->SHCSR = 7<<16;
}

#define DIPLAY_MODE		3

void Main(void)
{
	System_Init();
	Uart_Printf("BULLET HELL: Survival Arena\n");
    Uart_Printf("===========================");
	Lcd_Init(DIPLAY_MODE);

	Jog_Poll_Init();
	Jog_ISR_Enable(1);
	Uart1_RX_Interrupt_Enable(1);
    TIM3_Out_Init();  
    int music = 0;
    int stage_flag = 0;
	for(;;)
	{
        Lcd_Clr_Screen();
		Lcd_Printf(50,50, color[0],color[BACK_COLOR],2,2,"BULLET HELL");
        Lcd_Printf(50,100, color[4],color[BACK_COLOR],2,2,"Survival Arena");
		Lcd_Printf(50, LCDH/2+40, color[4],color[BACK_COLOR],1,1,"Move Jog");
        
        
		while (!Jog_key_in)
		{
			if (TIM1_expired || music == 0)
			{
				music = 1;
				BGM();
				TIM1_expired = 0;
			}
		}	
		srand(TIM2->CNT ^ TIM3->CNT ^ SysTick->VAL);
        Jog_Wait_Key_Released();
        Game_Init();
        
		for(;;)
		{
			int game_over = 0;
            int i;
            if (TIM1_expired || music == 0)
			{
				music = 1;
				BGM();
				TIM1_expired = 0;
			}

			if(Jog_key_in) 
			{
				user.ci = BACK_COLOR;
				Draw_Object(&user);
				game_over = USER_Move(Jog_key);
				user.ci = USER_COLOR;
				Draw_Object(&user);
				Jog_key_in = 0;
			}

            if (TIM2_expired)
            {
                sec_counter++;
                if (sec_counter % 10 == 0)
                {
                    score += 1;
                }

                if(sec_counter % 600 == 0)
                {
                    stage += 1;
                    if(stage > 3){
                        stage = 3;
                    }
                    if (stage < 3)
                    {
                        Uart1_Printf("Stage %d\n", stage);
                    }
                    else if (stage == 3 && stage_flag == 0)
                    {
                        Uart1_Printf("Final Stage %d\n", stage);
                        stage_flag = 1;
                    }    
                }
                
                TIM2_expired = 0;
            }

			if(TIM4_expired)
            {
                for (i = 0; i < MAX_BULLET; i++)
                {
                    if (bullets[i].alive)
                    {
                        bullets[i].ci = BACK_COLOR;
                        Draw_Bullet(&bullets[i]);
                        if(Bullet_Move(&bullets[i])) 
                        {
                            game_over = 1;
                        }
                        bullets[i].ci = BULLET_COLOR;
                        Draw_Bullet(&bullets[i]);
                    }

                }

                if(stage > 1){
                    for (i = 0; i < MAX_BIG_BULLET; i++)
                    {
                        if (big[i].alive)
                        {
                            big[i].ci = BACK_COLOR;
                            Draw_Bullet(&big[i]);
                            if(Big_Move(&big[i])) 
                            {
                                game_over = 1;
                            }
                            big[i].ci = BULLET_COLOR;
                            Draw_Bullet(&big[i]);
                        }
                    }
                }

                if(stage == 3)
                {
                    if (enemy.alive)
                    {
                        enemy.ci = BACK_COLOR;
                        Draw_Bullet(&enemy);
                        if(Enemy_Move())
                        {
                            enemy.ci = BACK_COLOR;
                            Draw_Bullet(&enemy);
                            enemy.alive = 0;
                        }
                        else{
                            enemy.ci = ENEMY_COLOR;
                            Draw_Bullet(&enemy);
                        }
                    }
                    else if (enemy_respawn_timer > 0){
                        enemy_respawn_timer--;
                        if (enemy_respawn_timer == 0)
                        {
                            Bullet_Init(&enemy); 
                            enemy.ci = ENEMY_COLOR;
                            enemy.alive = 1;
                        }
                    }
                }
                
                if (life_active)
                {
                    if (life_exist_timer > 0)
                    {
                        life_exist_timer--;
                        if (life_exist_timer % 10 == 0)
                        {
                            life.ci = LIFE_COLOR;
                            Draw_Bullet(&life);
                        }
                    }
                    else
                    {
                        life.ci = BACK_COLOR;
                        Draw_Bullet(&life);
                        life.alive = 0;
                        life_active = 0;
                        Reset_Life_Spawn();
                    }
                    
                }
                else
                {
                    if (life_spawn_timer > 0)
                    {
                        life_spawn_timer--;
                    }
                    else
                    {
                        Spawn_Life_Item();
                    }
                }
                Lcd_Printf(0, 0, BLACK, WHITE, 1, 1, "Stage %d", stage);
                Lcd_Printf(X_MAX/2, 0, BLUE, WHITE, 1, 1, "Time:%d", score);
                Lcd_Printf(X_MAX/2 + 100, 0, RED, WHITE, 1, 1, "Lives:%d", live);
                
                TIM4_expired = 0;
            }

			if(game_over)
			{
				TIM4_Repeat_Interrupt_Enable(0, 0);
                TIM2_Repeat_Interrupt_Enable(0, 0);
                Lcd_Clr_Screen();
                Uart_Printf("\n===================");
                Uart_Printf("\nGame Over\nSurvive Time:%d\nReset:Move Jog\n", score);
                Lcd_Printf(50, LCDH/4, color[4],color[BACK_COLOR],2,2,"Game Over");
                Lcd_Printf(50 ,LCDH/4+40, color[4],color[BACK_COLOR],1,1,"Survive Time:%d", score);
                Lcd_Printf(50, LCDH/4+60, color[4],color[BACK_COLOR],1,1,"Please press any key to continue");
                int wait_counter = 100;
                TIM2_Repeat_Interrupt_Enable(1, TIMER_PERIOD*10);
                while (!Jog_key_in)
                {
                    if (TIM1_expired || music == 0)
                    {
                        music = 1;
                        BGM();
                        TIM1_expired = 0;
                    }
                    if (TIM2_expired)
                    {
                        TIM2_expired = 0;
                        if(wait_counter % 10 == 0){
                            Lcd_Printf(50, LCDH/4+80, color[4],color[BACK_COLOR],2,2,"Continue? %2d", wait_counter/10);
                        }
                        wait_counter--;
                        if (wait_counter < 0)
                        {
                            Uart_Printf("\nBULLET HELL: Survival Arena\n");
                            Uart_Printf("===========================");
                            Lcd_Clr_Screen();
                            Lcd_Printf(50, 50, color[0], color[BACK_COLOR], 2, 2, "BULLET HELL");
                            Lcd_Printf(50, 100, color[4], color[BACK_COLOR], 2, 2, "Survival Arena");
                            Lcd_Printf(50, LCDH / 2 + 40, color[4], color[BACK_COLOR], 1, 1, "Move Jog");
                            break;
                        }
                    }    
                }
                break;
			}
		}
	}
}