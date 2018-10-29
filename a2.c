#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include <macros.h>
#include <ascii_font.h>
#include <graphics.h>
#include <lcd.h>
#include <sprite.h>
#include <usb_serial.h>
#include "lcd_model.h"
#include "cab202_adc.h"	


#define HERO_WIDTH (8)
#define HERO_HEIGHT (7)
#define MAX_BLOCKS (16)
#define NUM_ROWS (3)
#define NUM_COLUMNS (5)
#define MAX_BLOCK_WIDTH (10)
#define MAX_FOOD (5)
#define MAX_ZOMBIES (5)
#define FREQ     (8000000.0)
#define PRESCALE (256.0)
#define BIT(x) (1 << (x))
#define OVERFLOW_TOP (1023)
#define ADC_MAX (1023)

uint8_t x, y;
Sprite hero;
Sprite blocks[MAX_BLOCKS];
Sprite treasure;
Sprite food[MAX_FOOD];
Sprite zombies[MAX_ZOMBIES];
bool startCheck = false; // only for start game
bool state = false; // treasure pause state
bool gamePaused = false; // false when playing
bool treasure_collide = false; // collision of player and treasure
bool direction = false;
bool check = true;
bool * checkaddr = &check; 
bool end_game = false; // show end game screen
bool movement = true;
bool zombieSpawn = false; // triggers at the start to draw
bool light = false; // prevent looping for backlight because of show_screen();

int counter = 0; // for LED
int start_life = 10; // life tracker
int score = 0; // score tracker
int seconds = 0; // time in seconds
int minutes = 0; // time in minutes
int num_food = 5;
int num_zombies = 5;
int seed = 0;
int fedzombies = 0;
int pwm = 0; // set_duty_cycle argument
int Lcd_Contrast = LCD_DEFAULT_CONTRAST; // for LCD contrast. not backlight.

double BLOCK_LEFT_SPEED = -0.5;
double BLOCK_RIGHT_SPEED = 0.5;





uint8_t hero_bitmap[7] =
{
	0b11100000,
	0b10100000,
	0b11100000,
	0b01000000,
	0b11111111,
	0b01000000,
	0b10100000,
};

uint8_t treasure_bitmap[5] =
{
	0b00010000,
	0b01010100,
	0b01111100,
	0b01000100,
	0b01111100,
};

uint8_t badblocks_bitmap[4] =
{
	0b10101010,
	0b10010101,
	0b01010101,
	0b00101010,
};

uint8_t platform_bitmap[4] =
{
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111100,
};

uint8_t empty_bitmap[4] =
{
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
};

uint8_t food_bitmap[3] =
{
	0b01110000,
	0b11011000,
	0b01110000,	
};

uint8_t zombie_bitmap[6] =
{
	0b11111000,
	0b11011000,
	0b00100000,
	0b00100000,
	0b11111000,
	0b10001000,	
};

volatile uint8_t bit_counter = 0;
volatile uint8_t bit_counterleft = 0;
volatile uint8_t bit_counterright = 0;
volatile uint8_t bit_counterdown = 0;
volatile uint8_t bit_counterRB = 0;
volatile uint8_t bit_counterLB = 0;
volatile uint8_t bit_counterMID = 0;
volatile uint8_t bit_counterstart = 0;
volatile uint8_t bit_counterup = 0;

volatile uint32_t overflow_count = 0;

volatile uint8_t switch_state = 0;
volatile uint8_t switch_stateleft = 0;
volatile uint8_t switch_stateright = 0;
volatile uint8_t switch_statedown = 0;
volatile uint8_t switch_stateRB = 0;
volatile uint8_t switch_stateLB = 0;
volatile uint8_t switch_stateMID = 0;
volatile uint8_t switch_statestart = 0;
volatile uint8_t switch_stateup = 0;

void set_duty_cycle(int duty_cycle);
void fade_in();
void fade_out();
double get_current_time( void );

void sprite_draw_better(sprite_id sprite) {
	// Do nothing if not visible
	if (!sprite->is_visible) {
		return;
	}

	int x = round(sprite->x);
	int y = round(sprite->y);

	if ( x >= LCD_X || x + sprite->width <= 0 ) return;
	if ( y >= LCD_Y || y + sprite->height <= 0 ) return;

	// Loop through the bit-packed bitmap data, drawing each individual bit
	// (assume that the bitmap size is h * ceil(w/8))
	int byte_width = (sprite->width + 7) / 8;

	for (int dy = 0; dy < sprite->height; dy++) {
		for (int dx = 0; dx < sprite->width; dx++) {
			if ( ((sprite->bitmap[(int) (dy*byte_width + dx / 8)] >> (7 - dx % 8) & 1) == 1))
			{
			draw_pixel( x+dx, y+dy,
				(sprite->bitmap[(int) (dy*byte_width + dx / 8)] >> (7 - dx % 8) & 1)
			);
			}
		}
	}
} // improved sprite draw such that it does not draw when the bitmap is 0

bool sprite_move_to( sprite_id sprite, double x, double y ) {
	assert( sprite != NULL );
	int x0 = round( sprite->x );
	int y0 = round( sprite->y );
	sprite->x = x;
	sprite->y = y;
	int x1 = round( sprite->x );
	int y1 = round( sprite->y );
	return ( x1 != x0 ) || ( y1 != y0 );
} // taken from ZDK

void draw_double(uint8_t x, uint8_t y, double value, colour_t colour) {
	char buffer[100];
	snprintf(buffer, sizeof(buffer), "%f", value);
	draw_string(x, y, buffer, colour);
} // taken from AMS

void draw_int(int x, int y, int value) {
	char buffer[100];
	sprintf(buffer, "%d", value);
	draw_string(x, y, buffer, FG_COLOUR);
} // taken from AMS

bool sprite_step( sprite_id sprite ) {
	assert( sprite != NULL );
	int x0 = round( sprite->x );
	int y0 = round( sprite->y );
	sprite->x += sprite->dx;
	sprite->y += sprite->dy;
	int x1 = round( sprite->x );
	int y1 = round( sprite->y );
	return ( x1 != x0 ) || ( y1 != y0 );
} // taken from ZDK

void sprite_turn_to( sprite_id sprite, double dx, double dy ) {
	assert( sprite != NULL );
	sprite->dx = dx;
	sprite->dy = dy;
} // taken from ZDK

bool sprites_collide(sprite_id s1, sprite_id s2) // check for collision
{
    int top1 = round(s1->y);
    int bottom1 = top1 + s1->height - 1;
    int left1 = round(s1->x);
    int right1 = left1 + s1->width - 1;

    int top2 = round(s2->y);
    int bottom2 = top2 + s2->height - 1;
    int left2 = round(s2->x);
    int right2 = left2 + s2->height - 1;

    if (top1 > bottom2) return false;
    else if (top2 > bottom1) return false;
    else if (right1 < left2) return false;
    else if (right2 < left1) return false;
    else return true;
} // taken from ZDK

sprite_id sprites_collide_any(Sprite s, Sprite sprites[], int n) // check for collision in array
{
	sprite_id result = NULL;

    for ( int i = 0; i < n; i++ )
    {
        if ( sprites_collide( &s, &sprites[i] ) )
        {
            result = &sprites[i];
            break;
        }
    }

    return result;
} // taken from lecture examples on arrays

// HELPER FUNCTIONS ABOVE


int countRows(int blockNo) // returns row number that the block is in
{
	double rowNum = blockNo/NUM_COLUMNS;
	int rowNumber;
	if (blockNo % NUM_COLUMNS == 0)	rowNumber = rowNum ;
	else if (blockNo == 0) rowNumber = 1;
	else rowNumber = (trunc(rowNum) + 1);	
	return rowNumber;
}

double countColumns (int blockNo) // returns column number that the block is in
{
	double columnNumber = (blockNo % NUM_COLUMNS);
	return columnNumber;
}

void setup_food()
{
	for (int i = 0; i < MAX_FOOD; i++)
	{
		sprite_init(&food[i], -5, 0, 5, 3, food_bitmap);
		food[i].is_visible = false;
	}
}

void setup_zombies()
{
	int x = 10;
	for (int i = 0; i < MAX_ZOMBIES; i++)
	{
		sprite_init(&zombies[i], x, 0, 5, 6, zombie_bitmap);
		x+= 6;
	}
}

void setup_bunchofblocks()
{
	srand(seed);
	int luck, row1 = HERO_HEIGHT + 1; // y of row 1
	int by, bx;
	
	for (int i = 0; i< MAX_BLOCKS; i++) // each block
	{	
		if (countRows(i) > NUM_ROWS)
		{
			break;
		}
		luck = rand() % 30;	
		by = ((countRows(i)-1)*12) + row1; // gives y coords corresponding to the row
		if (countRows(i) == 0) by = row1; // solve block[0] issue
		bx =  (countColumns(i)) * (MAX_BLOCK_WIDTH + 8) ;	// gives x coords cooresponding to column
		
		if ((x+10 >= (hero.x+2) && x <= (hero.x+2) && y == row1) || i == 0)
		{
			sprite_init(&blocks[i], bx, by, 10, 2, platform_bitmap);	
		}
		else if (luck % 3 == 0)	sprite_init(&blocks[i], bx, by, 10, 2, badblocks_bitmap);
		else if ( luck % 6 == 0) sprite_init(&blocks[i], bx, by, 10, 2, empty_bitmap);
		else sprite_init(&blocks[i], bx, by, 10, 2, platform_bitmap);	
		
	}
}

int find_safe_block(void)
{
	int x = 5;
	for (int i = 0; i < MAX_BLOCKS; i++)
	{
		if (blocks[i].x > 0 && blocks[i].x + 10 < LCD_X && blocks[i].bitmap != badblocks_bitmap)
		{
			x = blocks[i].x;
			break;
		}
	}
	
	return x;
}

void setup_all()
{
	sprite_init(&hero, 0, 1, HERO_WIDTH, HERO_HEIGHT, hero_bitmap);
	sprite_init(&treasure, -1, LCD_Y - 5, 6, 5, treasure_bitmap);
	treasure.dx = 0.4;
	setup_bunchofblocks();
	setup_food();
}

bool sprites_above(Sprite above, Sprite below) // logic for hero standing on platform
{
    int top1 = round(above.y);
    int bottom1 = top1 + above.height - 1;
    int left1 = round(above.x);
	int right1;
    if (above.width == 8) right1 = round(left1 + 2);
	else right1 = round(left1 + above.width - 1);

    int top2 = round(below.y);
    int left2 = round(below.x);
    int right2 = round(left2 + below.width - 1);

    if (bottom1 == top2 - 1 && right1 >= left2 && left1 <= right2)
    {
        return true;
    }
    else 
	{
        return false;
    }
}

sprite_id sprites_above_any( Sprite s, Sprite sprites[], int n) // if standing on any blocks[i]
{
	sprite_id result = NULL;;
	
    for ( int i = 0; i < n; i++ )
    {
        if ( sprites_above( s, sprites[i] ) )
        {
            result = &sprites[i];
            break;
        }
    }

    return result;
}

bool zombieLand()
{
	int numZombie[MAX_ZOMBIES];
	for (int i = 0; i < MAX_ZOMBIES; i++)
	{
		if ( zombies[i].y > LCD_Y && zombieSpawn == true) 
		{
			numZombie[i] = 1;
		}
		
		else if (sprites_above_any(zombies[i], blocks, MAX_BLOCKS) && zombieSpawn == true) numZombie[i] = 1;
		
		else numZombie[i] = 0;
	}
		
	int total = numZombie[1] + numZombie[2] + numZombie[3] + numZombie[4] + numZombie[0];
	
	if (total == 5) return true;
	
	else return false;	
}

int numberOfZombies()
{
	int numZombie[MAX_ZOMBIES];
	for (int i = 0; i < MAX_ZOMBIES; i++)
	{
		if ((zombies[i].x >= 0 && (zombies[i].x + zombies[i].width <= LCD_X)) && zombies[i].y <= LCD_Y && zombieSpawn == true) 
		{
			numZombie[i] = 1;
		}
		else if ((zombies[i].y > LCD_Y ||  zombies[i].x + zombies[i].width > LCD_X || zombies[i].x < 0 )&& zombieSpawn == true && zombies[i].is_visible == false ) 
		{
			numZombie[i] = 0;
		}
		else return 5;
	}
	int total = numZombie[1] + numZombie[2] + numZombie[3] + numZombie[4] + numZombie[0];
	return total;
}


bool pixel_level_collision(Sprite s1, Sprite s2)
{
	int byte_width = (s1.width + 7) / 8;
	int byte_width2 = (s2.width + 7) / 8;
	int xof1 = 200, xof2, yof1 = 200, yof2;	
	for (int y = 0; y < s1.height; y++)
    {
        for (int x = 0; x < s1.width; x++)
        {
			if (((s1.bitmap[(int)(y*byte_width + x / 8)] >> (7 - x % 8)) & 1))
			{
				xof1 = round(x + s1.x);
				yof1 = round(y + s1.y);
			}
			
			for (int y1 = 0; y1 < s2.height; y1++)
			{
				
				for (int x1 = 0; x1 < s2.width; x1++)
				{
						if (((s2.bitmap[(int) (y1*byte_width2 + x1 / 8)] >> (7 - x1 % 8)) & 1))
						{
							xof2 = round(x1 + s2.x);
							yof2 = round(y1 + s2.y);
							if (xof1 == xof2 && yof1 == yof2) return true;	
						}	
				}
			}
			
        }
		
	}
	return false;
}

sprite_id pixel_level_collision_any( Sprite s, Sprite sprites[], int n) // if standing on any blocks[i]
{
	sprite_id result = NULL;;
	
    for ( int i = 0; i < n; i++ )
    {
        if ( pixel_level_collision( s, sprites[i] ) )
        {
            result = &sprites[i];
            break;
        }
    }

    return result;
}

void hero_bump()
{
	if ( sprites_collide_any(hero, blocks, MAX_BLOCKS) ) // if hit
	{
		sprite_id corner = sprites_collide_any(hero, blocks, MAX_BLOCKS);
		if (pixel_level_collision(hero, *corner))
		{
			if (corner->bitmap == platform_bitmap)
			{
				sprite_turn_to(&hero, 0 , hero.dy); //rebound
			}
		}
	}
}

void check_respawn(void) // check if player out of screen left and right.
{
	if (hero.x < 0)
	{
		if (start_life > 1)
		{	
			fade_out();
			fade_in();
		}
		start_life -= 1;		
		sprite_turn_to(&hero, BLOCK_RIGHT_SPEED, 0);
		sprite_move_to(&hero, find_safe_block(), blocks[0].y - HERO_HEIGHT);
		movement = false;
		
		char  s[100];
		sprintf(s, "Death by off screen! Score: %d\tLife: %d\tTime: %d:%d\r\n",\
		score, start_life, minutes, seconds);
		usb_serial_write((uint8_t *) s, strlen(s));
		
		char  str[100];
		sprintf(str, "Player respawns at (%0f, %0f)\r\n", roundf(hero.x), roundf(hero.y));
		usb_serial_write((uint8_t *) str, strlen(str));
	}
	else if (hero.x >= LCD_X -2)
	{
		if (start_life > 1)
		{	
			fade_out();
			fade_in();
		}
		start_life -= 1;
		sprite_turn_to(&hero, BLOCK_RIGHT_SPEED, 0);
		sprite_move_to(&hero, find_safe_block(), blocks[0].y - HERO_HEIGHT);
		movement = false;
		
		char s[100];
		sprintf(s, "Death by off screen! Score: %d\tLife: %d\tTime: %d:%d\r\n",\
		score, start_life, minutes, seconds);
		usb_serial_write((uint8_t *) s, strlen(s));
		
		char  str[100];
		sprintf(str, "Player respawns at (%0f, %0f)\r\n", roundf(hero.x), roundf(hero.y));
		usb_serial_write((uint8_t *) str, strlen(str));
	}
}


void move_platform()
{
	int left_adc = adc_read(0);
	BLOCK_LEFT_SPEED = left_adc * (-0.5)/1024;
	BLOCK_RIGHT_SPEED = left_adc * (0.5)/1024;
	for (int i = 0; i < MAX_BLOCKS; i++)
	{
		if (countRows(i) % 2 != 0  || i == 0) // 1st and last
		{
			blocks[i].dx = BLOCK_RIGHT_SPEED; // speed is 0.5
			if (blocks[i].x > LCD_X + 2) blocks[i].x = 0;
			
			if (sprites_above(hero, blocks[i])) // if hero is on block
			{
				check_respawn(); //check if out of screen
				if (movement == true) hero.dx = blocks[i].dx;
			}
		}
		
		else if (countRows(i) % 2 == 0) // middle
		{
			blocks[i].dx = BLOCK_LEFT_SPEED; // speed is -0.5
			if (round(blocks[i].x + 10) == 0) blocks[i].x = LCD_X;
			
			if (sprites_above(hero, blocks[i])) // if hero is on block
			{
				check_respawn(); //check if out of screen
				if (movement == true) hero.dx = blocks[i].dx;
			}
		
		}
		
		for (int x = 0; x < MAX_FOOD; x++)
		{
			if (sprites_above(food[x], blocks[i]) && i != 0) 
			{
				food[x].dx = blocks[i].dx;	
				sprite_step(&food[x]);
			}			
		}
		
		sprite_step(&blocks[i]);
	}
}

void add_life(int * lives) // add lives if collide and updates display screen
{
	if (pixel_level_collision(treasure, hero) && treasure_collide == false) 
	{	
		(* lives) += 2;
		treasure.is_visible = false;
		hero.x = find_safe_block();
		hero.dx = BLOCK_RIGHT_SPEED;
		hero.dy = 0;
		hero.y = 1;
		treasure_collide = true;
		
		char  s[150];
		sprintf(s, "Treasure collided. Score: %d\tLife: %d\tTime: %d:%d Position(%d,%d)\r\n",\
		score, start_life, minutes, seconds, (int)hero.x, (int)hero.y);
		usb_serial_write((uint8_t *) s, strlen(s));
	}
}

void forbidden_touch()
{
	if (start_life > 1)
	{	
		fade_out();
		fade_in();
	}
	start_life-= 1;
	*checkaddr = true; // change check to true once land again
	
	sprite_move_to(&hero, find_safe_block(), 1);
	sprite_turn_to(&hero, BLOCK_RIGHT_SPEED, 0);
	char  s[100];
	sprintf(s, "Death by bad block! Score: %d\tLife: %d\tTime: %d:%d\r\n",\
	score, start_life, minutes, seconds);
	usb_serial_write((uint8_t *) s, strlen(s));
	
	char  str[100];
	sprintf(str, "Player respawns at (%0f, %0f)\r\n", roundf(hero.x), roundf(hero.y));
	usb_serial_write((uint8_t *) str, strlen(str)); 
}

void score_tracker() // tracks score and life if lands on block
{
	sprite_id block_check = sprites_above_any(hero, blocks, MAX_BLOCKS);
	sprite_id collide_check = sprites_collide_any(hero, blocks, MAX_BLOCKS);
	if ((block_check && check == false) || collide_check)  
	{
		if (block_check->bitmap == badblocks_bitmap) // deducts life if lands on bad block
		{
			forbidden_touch();
		}
		
		if (collide_check->bitmap == badblocks_bitmap)
		{
			if (pixel_level_collision(hero, *collide_check))
			{
				forbidden_touch();
			}
		}
		
		else if (block_check->bitmap == platform_bitmap) // adds score if lands on good block
		{
			// step on good blocks
			score+= 1;
			*checkaddr = true; // change check to true once land again
		}	
		
	}
	
	for (int i = 0; i < MAX_FOOD; i++)
	{		
		if (sprites_collide_any(food[i], zombies, MAX_ZOMBIES))
		{
			sprite_id dedzombie = sprites_collide_any(food[i], zombies, MAX_ZOMBIES);
			if (pixel_level_collision(hero, *dedzombie))
			{
				dedzombie->is_visible = false;
				dedzombie->x = 100; // goodbye zombie
				food[i].x = 200; // goodbye food
				food[i].is_visible = false;
				num_food++;
				score+= 10;
				num_zombies--;
				fedzombies++;
				
				char  str[100];
				sprintf(str, "Fed zombie! No. Zombies: %d, no. food: %d, Time:%d : %d\r\n", num_zombies, num_food, minutes, seconds);
				usb_serial_write((uint8_t *) str, strlen(str));
			}
		}
		
	}
	
	if (sprites_collide_any(hero, zombies, MAX_ZOMBIES))
	{
		if (block_check == &blocks[0] || block_check == &blocks[1])
		{
			
		}
		
		else
		{
			if (start_life > 1)
			{	
				fade_out();
				fade_in();
			}
			start_life-= 1;
			*checkaddr = true; // change check to true once land again
			sprite_move_to(&hero, find_safe_block(), 1);
			sprite_turn_to(&hero, BLOCK_RIGHT_SPEED, 0);
			char  zombiedie[100];
			sprintf(zombiedie, "Death by zombie! Score: %d\tLife: %d\tTime: %d:%d\r\n",\
			score, start_life, minutes, seconds);
			usb_serial_write((uint8_t *) zombiedie, strlen(zombiedie));		

			char  str[100];
			sprintf(str, "Player respawns at (%0f, %0f)\r\n", roundf(hero.x), roundf(hero.y));
			usb_serial_write((uint8_t *) str, strlen(str));			
		}
	}
}

bool gravity(void) // free fall at 0.015 units per update
{
	for (int i = 0; i < MAX_ZOMBIES; i++)
	{
		if ((!sprites_above_any(zombies[i], blocks, MAX_BLOCKS)))
		{
			if (zombies[i].dy < 0.505) zombies[i].dy+= 0.05;
			zombies[i].y += zombies[i].dy; // equivalent of sprite step
		}
		else
		{
			zombies[i].dy = 0;
		} //reset acceleration	
		
	}
	
	if ((!sprites_above_any(hero, blocks, MAX_BLOCKS)))
	{
		if (hero.dy < 0.505) hero.dy+= 0.05;
		hero.x += 0.65 * hero.dx;
		hero.y += hero.dy;
		movement = true;
		check = false;  // change check to false once away from block
		return check;
	}
	else
	{
		hero.dy = 0;
	} //reset acceleration


	return check;
}

void draw_sprites( Sprite s[], int n) //draw sprite array
{
	for (int i = 0; i < n; i++)
	{
		sprite_draw_better(&s[i]);
	}
}

void draw_all()
{
	sprite_draw_better(&hero);
	sprite_draw_better(&treasure);
	draw_sprites(blocks, MAX_BLOCKS);
	draw_sprites(food, MAX_FOOD);
}



void jump()
{
	hero.dy -= 0.3;
	check = false;
}

void summon_food()
{	
	if (sprites_above_any(hero, blocks, MAX_BLOCKS))
	{
		food[5-num_food].x = hero.x;
		food[5-num_food].y = hero.y + HERO_HEIGHT - 3;
		food[5-num_food].is_visible = true;
		num_food -= 1;
		
	}	
}

void move_hero(int c)
{
	static uint8_t prevStateleft = 0;
	static uint8_t prevStateright = 0;
	static uint8_t prevStatedown = 0;
	static uint8_t prevStateup = 0;
	sprite_id onblock = sprites_above_any(hero, blocks, MAX_BLOCKS);
	
	if (gamePaused == false)
	{
		if ( switch_stateleft != prevStateleft ) {
			prevStateleft = switch_stateleft;
			if (prevStateleft == 1) // left
			{
				movement = false;
				if (onblock->dx == BLOCK_LEFT_SPEED)  // if block move left i.e. middle platform ALL WORKS
				{
					if (onblock->dx == hero.dx) hero.dx = -1; // and hero not moving
				
					if (hero.dx == 1) movement = true;// and hero moving right					
				}

				if (onblock->dx == BLOCK_RIGHT_SPEED && hero.dx == 1) movement = true;
					
				else if (onblock->dx == BLOCK_RIGHT_SPEED && hero.dx == BLOCK_RIGHT_SPEED) hero.dx = -1;
					
				
			}
		}
		
		else if (c == 'a'){
			movement = false;
			if (onblock->dx == BLOCK_LEFT_SPEED)  // if block move left i.e. middle platform ALL WORKS
			{
				if (onblock->dx == hero.dx) hero.dx = -1; // and hero not moving
			
				if (hero.dx == 1) movement = true;// and hero moving right					
			}

			if (onblock->dx == BLOCK_RIGHT_SPEED && hero.dx == 1) movement = true;
				
			else if (onblock->dx == BLOCK_RIGHT_SPEED && hero.dx == BLOCK_RIGHT_SPEED) hero.dx = -1;
		}
			
			
		
		if ( switch_stateright != prevStateright ) {
			prevStateright = switch_stateright;
			if (prevStateright == 1)  // right
			{
				movement = false;
				if (onblock->dx == BLOCK_RIGHT_SPEED)  // if block move right
				{
					if (onblock->dx == hero.dx) hero.dx = 1; // and hero not moving 
					
					if (hero.dx == -1) movement = true;
				}

				if (onblock->dx == BLOCK_LEFT_SPEED) // if block move left
				{
					if (onblock->dx == hero.dx) hero.dx = 1; // and hero not moving
					
					else if (hero.dx == -1) movement = true;
					
				}	
			}
		}
		
		else if (c == 'd') {
			movement = false;
			if (onblock->dx == BLOCK_RIGHT_SPEED)  // if block move right
			{
				if (onblock->dx == hero.dx) hero.dx = 1; // and hero not moving 
				
				if (hero.dx == -1) movement = true;
			}

			if (onblock->dx == BLOCK_LEFT_SPEED) // if block move left
			{
				if (onblock->dx == hero.dx) hero.dx = 1; // and hero not moving
				
				else if (hero.dx == -1) movement = true;

			}				
		}
		
		if ( switch_stateup != prevStateup ) {
			prevStateup = switch_stateup;
			if (prevStateup == 1)  // up
			{
				jump();
			}
		}
		
		else if (c == 'w')
		{
			jump();
		}
			
		if ( switch_statedown != prevStatedown ) {
			prevStatedown = switch_statedown;
			if (prevStatedown == 1 && num_food != 0) // down
			{
				summon_food();
			}
		}
		
		else if (c == 's' && get_current_time() > 3)
		{
			summon_food();
		}
		
		sprite_step(&hero);
	}
	
}

double get_current_time(void)
{
	double time = ( overflow_count * 256.0 + TCNT0 ) * PRESCALE  / FREQ;
	return time;
}


void fade_in()
{
	pwm = 0;
	for  (int i = 0; i < 100; i++)
	{
		show_screen();
		set_duty_cycle(ADC_MAX - pwm);
		if (pwm < ADC_MAX) pwm+= 10;	
	}	
	set_duty_cycle(0);
	pwm = 0;
	
	Lcd_Contrast = 0;
	for (int i = 0; i < 100000; i++)
	{
		if (Lcd_Contrast < LCD_DEFAULT_CONTRAST && i % 1000 == 0)
		{	
			Lcd_Contrast++;
			LCD_CMD(lcd_set_function, lcd_instr_extended);
			LCD_CMD(lcd_set_contrast, Lcd_Contrast);
			LCD_CMD(lcd_set_function, lcd_instr_basic);		
		}
		
		else if (Lcd_Contrast == LCD_DEFAULT_CONTRAST) break;
	}
	
}

void fade_out()
{
	
	for (int i = 0; i < 100000; i++)
	{
		if (Lcd_Contrast > 0 && i % 1000 == 0) {
			Lcd_Contrast--;
			LCD_CMD(lcd_set_function, lcd_instr_extended);
			LCD_CMD(lcd_set_contrast, Lcd_Contrast);
			LCD_CMD(lcd_set_function, lcd_instr_basic);		
		}
		else if (Lcd_Contrast == 0) break;
	}
	
	for  (int i = 0; i < 100; i++)
	{
		show_screen();
		set_duty_cycle(pwm);
		if (pwm < ADC_MAX) pwm+= 10;
		
	}	
	
}


void restart_game() // clear state of everything
{
	overflow_count = 0;
	minutes = 0;
	seconds = 0;
	start_life = 10;
	score = 0;
	counter = 0; // for led
	num_zombies = 0;
	num_food = MAX_FOOD;
	treasure.is_visible = true;
	sprite_move_to(&treasure,  -1, LCD_Y - 5);// move to start
	hero.dy = 0;
	hero.dx = 0;
	sprite_move_to(&hero, find_safe_block(), blocks[0].y - HERO_HEIGHT);
	treasure_collide = false; // when restart we can collide again
	state = false; // treasure pause state
	direction = false; // resumes treasure direction
	check = true; // toggle for gravity
	gamePaused = false;
	zombieSpawn = false;
	movement = true;
	checkaddr = &check; 
	end_game = false; // show end game screen
	light = false; // prevent looping for backlight because of show_screen();
	seed = 0;
	fedzombies = 0;
	pwm = 0; // set_duty_cycle argument
	Lcd_Contrast = LCD_DEFAULT_CONTRAST; // for LCD contrast. not backlight.
	
	for (int i = 0; i< MAX_FOOD; i++)
	{
		food[i].x = 200; // goodbye food
		food[i].is_visible = true;
	}
	
	if (seconds == 3 && zombieSpawn == false)
	{
		setup_zombies();
		zombieSpawn = true;
		char s[150];
		sprintf(s, "Zombie spawned! Score: %d\tLife: %d\tTime: %d:%d No. zombies: %d\r\n ",\
		score, start_life, minutes, seconds, MAX_ZOMBIES);
		usb_serial_write((uint8_t *) s, strlen(s));
	}
	
	uint8_t togglemask1 = (1 << 2);
	uint8_t togglemask2 = (1 << 3);
	if (zombieLand() == true) // off LED
	{
		CLEAR_BIT(PORTB, 2);
		CLEAR_BIT(PORTB, 3);
	}
	
	if (zombieLand() == false)
	{
		counter ++; // 4Hz
		if (counter == 30.0 && zombieLand() == false)
		{
			PORTB = PORTB ^ togglemask1;
			PORTB = PORTB ^ togglemask2;	
			counter = 0;
		}
	}
}

void life_over() // when player dies.
{
	clear_screen();
	draw_string(0, 0, "Try not to die.", FG_COLOUR );
	draw_string(0, 15, "Score: ", FG_COLOUR);
	draw_int(30, 15, score);
	draw_string(0, 25, "Time: :", FG_COLOUR);
	draw_int(25, 25, minutes);
	draw_int(35, 25, seconds);
	if (seconds < 10)	// Seconds printer
	{
		draw_string(35, 25, "0", FG_COLOUR);
		draw_int(42, 25, seconds);
	}
	else if (seconds >= 10) draw_int(35, 25,  seconds);
	
	CLEAR_BIT(PORTB, 2);
	CLEAR_BIT(PORTB, 3);
	show_screen();
	
}

void move_hero_test()
{
	hero.dx = 0;
    int hx = round(hero.x);
    int hy = round(hero.y);
	if (gamePaused == false)
	{
	if (BIT_IS_SET(PINB, 1) && hx > 0) // left
	{
		hero.x -= 1; // runs with acceleration
	}
	
    if (BIT_IS_SET(PIND, 0) && hx < 83 - hero.width)  // right
	{
		hero.x += 1; // runs with acceleration
	}
	
	if (BIT_IS_SET(PIND, 1)) hero.y -= 1;
		
    if (BIT_IS_SET(PINB, 7) && hy < LCD_Y - hero.height - 1) hero.y += 1; // to move down for testing
	}
	
	sprite_step(&hero);
}

sprite_id move_zombie(Sprite sid[], int n)
{
	sprite_id moveblock = NULL;
	for ( int i = 0; i < n; i++ )
    {
		int zx = round(sid[i].x);
		
		
        sprite_step(&sid[i]);
		if (sprites_above_any(sid[i], blocks, MAX_BLOCKS))
		{
			sprite_id onblock = sprites_above_any(sid[i], blocks, MAX_BLOCKS);
			moveblock = onblock;
			sid[i].dx = 1;
			double zdx = sid[i].dx;

			if (zx <= onblock->x - 2) zdx = fabs(zdx);
			else if (zx >= onblock->x + onblock->width - sid[i].width -2) zdx = -fabs(zdx);

			if ( zdx != sid[i].dx )
			{
				sid[i].x -= sid[i].dx;
				sid[i].dx = zdx;
			}
			
		}
	}
	
	
	return moveblock;
}

void check_zombie(sprite_id zombieblock)
{
	for (int i = 0; i < MAX_ZOMBIES; i++)
	{
		if (zombieblock != NULL)
		{
			if (zombieblock->x == 0 )
			{
				zombies[i].x = LCD_Y;
				zombies[i].y = zombieblock->y + zombies[i].height + 3;
			}
		}
	}
}


bool move_treasure()
{
	int tx = round(treasure.x);
	double tdx = treasure.dx;

	treasure.x += treasure.dx;
	
	if (tx <= 0) tdx = fabs(tdx);
	else if (tx >= LCD_X - treasure.width) tdx = -fabs(tdx);
	
	if (tdx != treasure.dx)
	{
		treasure.x -= treasure.dx;
		treasure.dx = tdx;
	}
	
	if (tdx > 0) direction = false;  // set false as right
	if (tdx < 0) direction = true;  // set true as left
	return direction;
}

void pause_treasure(bool direction, int keyCode) // pause treasure and save directions
{
	static uint8_t prevStateRB = 0;
	if ( switch_stateRB != prevStateRB ) {
		prevStateRB = switch_stateRB;
		if (prevStateRB == 1 && state == false)
		{
			state = !state;
			treasure.dx = 0;
		}
		else if (prevStateRB == 1 && state == true)
		{
			state = !state;
			if (direction == false)
			{
				treasure.dx = 0.4;
			}
			else 
			{
				treasure.dx = -0.4;
			}
		}
	}
	
	else if (keyCode == 't' && state == false)
	{
		state = !state;
		treasure.dx = 0;
	}
	else if (keyCode == 't' && state == true)
	{
		state = !state;
		if (direction == false)
		{
			treasure.dx = 0.4;
		}
		else 
		{
			treasure.dx = -0.4;
		}
	}
	
}

int spawnTime;

void start_game(int c)
{
	static uint8_t prevStatestart = 0;
	if ( switch_statestart != prevStatestart ) {
		prevStatestart = switch_statestart;	
		if (prevStatestart == 1 && startCheck == false) //SW2 ONLY FOR START GAME
			{
				seed = get_current_time();
				setup_all();
				clear_screen();
				draw_all();
				show_screen();
				startCheck = true;
				
				char  s[150];
				sprintf(s, "Game started. Position(%d,%d)\r\n",\
				(int)hero.x, (int)hero.y);
				usb_serial_write((uint8_t *) s, strlen(s));
			}// end start game
	}
	
	else if (c == 's' && startCheck == false) //SW2 ONLY FOR START GAME
	{
		seed = get_current_time();
		setup_all();
		clear_screen();
		draw_all();
		show_screen();
		startCheck = true;
		
		char  s[150];
		sprintf(s, "Game started. Position(%d,%d)\r\n",\
		(int)hero.x, (int)hero.y);
		usb_serial_write((uint8_t *) s, strlen(s));
	}// end start game
	
	spawnTime = get_current_time();
}


void pause_game(int c)
{
	static uint8_t prevStateMID = 0;
	if ( switch_state != prevStateMID ) {
		prevStateMID = switch_state;
		if (prevStateMID == 1 && gamePaused == false)
		{
			char  s[150];
			sprintf(s, "Paused game. Score: %d Life: %d Time: %d:%d no. of zombies: %d no. of food: %d\r\n",\
			score, start_life, minutes, seconds, numberOfZombies(), num_food);
			usb_serial_write((uint8_t *) s, strlen(s));
				
			hero.is_visible = false;
			for (int i = 0; i < MAX_BLOCKS; i++)
			{
				blocks[i].is_visible = false;
			}
			
			for (int i = 0; i < MAX_FOOD; i++)
			{
				food[i].is_visible = false;
				zombies[i].is_visible = false;
			}
			treasure.is_visible = false;

			gamePaused = !gamePaused;
			clear_screen();
			draw_string(0, 2, "Lives: ", FG_COLOUR);
			draw_int(30, 2, start_life);
			draw_string(0, 11, "Time: ", FG_COLOUR);
			
			draw_int(25, 11, minutes);
			draw_string(30, 11, ":", FG_COLOUR);
		
		
			if (seconds < 10)	// Seconds printer
			{
				draw_string(35, 11, "0", FG_COLOUR);
				draw_int(42, 11, seconds);
			}
			else if (seconds >= 10) draw_int(35, 11,  seconds);
			
			draw_string(0, 20, "Score: ", FG_COLOUR);
			draw_int(30, 20, score);
			draw_string(0, 29, "Food: ", FG_COLOUR);
			draw_int(30, 29, num_food);
			draw_string( 0, 39, "Zombies:", FG_COLOUR);
			draw_int(40, 39, numberOfZombies());
			
		}
		
		else if (prevStateMID == 1 && gamePaused == true)
		{
			hero.is_visible = true;
			for (int i = 0; i < MAX_BLOCKS; i++)
			{
				blocks[i].is_visible = true;
			}
			
			for (int i = 0; i < MAX_FOOD; i++)
			{
				if (food[i].x != 0 && food[i].y != 0) food[i].is_visible = true;
			}
			
			for (int i =0; i < MAX_ZOMBIES; i++)
			{
				zombies[i].is_visible = true;
			}
			
			if (treasure_collide == false) treasure.is_visible = true;
			gamePaused = !gamePaused;
		}	
	}
	
	else if (c == 'p' && gamePaused == false)
	{
		char  s[150];
		sprintf(s, "Paused game. Score: %d Life: %d Time: %d:%d no. of zombies: %d no. of food: %d\r\n",\
		score, start_life, minutes, seconds, numberOfZombies(), num_food);
		usb_serial_write((uint8_t *) s, strlen(s));
			
		hero.is_visible = false;
		for (int i = 0; i < MAX_BLOCKS; i++)
		{
			blocks[i].is_visible = false;
		}
		
		for (int i = 0; i < MAX_FOOD; i++)
		{
			food[i].is_visible = false;
			zombies[i].is_visible = false;
		}
		treasure.is_visible = false;

		gamePaused = !gamePaused;
		clear_screen();
		draw_string(0, 2, "Lives: ", FG_COLOUR);
		draw_int(30, 2, start_life);
		draw_string(0, 11, "Time: ", FG_COLOUR);
		
		draw_int(25, 11, minutes);
		draw_string(30, 11, ":", FG_COLOUR);
		
		
		if (seconds < 10)	// Seconds printer
		{
			draw_string(35, 11, "0", FG_COLOUR);
			draw_int(42, 11, seconds);
		}
		else if (seconds >= 10) draw_int(35, 11,  seconds);
		
		draw_string(0, 20, "Score: ", FG_COLOUR);
		draw_int(30, 20, score);
		draw_string(0, 29, "Food: ", FG_COLOUR);
		draw_int(30, 29, num_food);
	}
	
	else if (c == 'p' && gamePaused == true)
	{
		hero.is_visible = true;
		for (int i = 0; i < MAX_BLOCKS; i++)
		{
			blocks[i].is_visible = true;
		}
		
		for (int i = 0; i < MAX_FOOD; i++)
		{
			if (food[i].x != 0 && food[i].y != 0) food[i].is_visible = true;
		}
		
		for (int i =0; i < MAX_ZOMBIES; i++)
		{
			zombies[i].is_visible = true;
		}
		
		if (treasure_collide == false) treasure.is_visible = true;
		gamePaused = !gamePaused;
	}	
}

void setup( void ) {
	set_clock_speed(CPU_8MHz);
	//	(a) Initialise the LCD display using the default contrast setting
	lcd_init(LCD_DEFAULT_CONTRAST);
	adc_init();
	usb_init();
	// ENABLE ALL INPUT
	CLEAR_BIT(DDRB, 1); // enable left input
	CLEAR_BIT(DDRD, 0); // right input
	CLEAR_BIT(DDRD, 1); // up input
	CLEAR_BIT(DDRB, 7); // down input
	CLEAR_BIT(DDRB, 0); // centre input
	CLEAR_BIT(DDRF, 5); // SW3
	CLEAR_BIT(DDRF, 6); // SW2
	
	// ENABLE ALL OUTPUT
	SET_BIT(DDRB, 2); // LED0
 	SET_BIT(DDRB, 3); // LED1
	
	// ENABLE TIMER 0
	TCCR0A = 0;
	TCCR0B = 4; 
	TIMSK0 = 1; 
		
	// ENABLE TIMER 1
	TCCR1A = 0;
	TCCR1B = 2; // prescale factor 8 overflow interval 0.065536
	TIMSK1 = 1;
	
	TC4H = OVERFLOW_TOP >> 8;
	OCR4C = OVERFLOW_TOP & 0xff;
	// (b)	Use OC4A for PWM. Remember to set DDR for C7.
	TCCR4A = BIT(COM4A1) | BIT(PWM4A);
	SET_BIT(DDRC, 7); // LCD control
	
	// (c)	Set pre-scale to "no pre-scale" 
	TCCR4B = BIT(CS42) | BIT(CS41) | BIT(CS40);
	TCCR4D = 0;
	
	sei();
	char * id = "n9972676";
	clear_screen();
	draw_string(15, 15, id, FG_COLOUR);
	draw_string(15, 25, "Wesley Kok", FG_COLOUR);
}

ISR(TIMER1_OVF_vect) {
	
	uint8_t togglemask1 = (1 << 2);
	uint8_t togglemask2 = (1 << 3);
	
	counter ++; // 4Hz
	if (counter == 4.0 && zombieLand() == false && start_life > 0)
	{
		PORTB = PORTB ^ togglemask1;
		PORTB = PORTB ^ togglemask2;	
		counter = 0;
	}
}


ISR(TIMER0_OVF_vect) {
	overflow_count++;
	uint8_t mask = 0b00001111;
	uint8_t mask2 = 0b00000001;
	bit_counter = (bit_counter << 1);
	bit_counter = bit_counter & mask;
	bit_counter = bit_counter | BIT_VALUE(PINB, 0);
	
	bit_counterleft = (bit_counterleft << 1);
	bit_counterleft = bit_counterleft & mask;
	bit_counterleft = bit_counterleft | BIT_VALUE(PINB, 1);
	
	bit_counterright = (bit_counterright << 1);
	bit_counterright = bit_counterright & mask;
	bit_counterright = bit_counterright | BIT_VALUE(PIND, 0);
	
	bit_counterRB = (bit_counterRB << 1);
	bit_counterRB = bit_counterRB & mask;
	bit_counterRB = bit_counterRB | BIT_VALUE(PINF, 5);
	
	bit_counterLB = (bit_counterLB << 1);
	bit_counterLB = bit_counterLB & mask;
	bit_counterLB = bit_counterLB | BIT_VALUE(PINF, 6);

	bit_counterstart = (bit_counterstart << 1);
	bit_counterstart = bit_counterstart & mask;
	bit_counterstart = bit_counterstart | BIT_VALUE(PINF, 6);
	
	bit_counterdown = (bit_counterdown << 1);
	bit_counterdown = bit_counterdown & mask;
	bit_counterdown = bit_counterdown | BIT_VALUE(PINB, 7);
	
	bit_counterup = (bit_counterup << 1);
	bit_counterup = bit_counterup & mask2;
	bit_counterup = bit_counterup | BIT_VALUE(PIND, 1);


	
	if (bit_counter == mask) switch_state = 1;
	else if (bit_counter == 0) switch_state = 0;
	
	if (bit_counterleft == mask) switch_stateleft = 1;
	else if (bit_counterleft == 0) switch_stateleft = 0;
	
	if (bit_counterright == mask) switch_stateright = 1;
	else if (bit_counterright == 0) switch_stateright = 0;
	
	if (bit_counterdown == mask) switch_statedown = 1;
	else if (bit_counterdown == 0) switch_statedown = 0;
	
	if (bit_counterRB == mask) switch_stateRB = 1;
	else if (bit_counterRB == 0) switch_stateRB = 0;
	
	if (bit_counterLB == mask) switch_stateLB = 1;
	else if (bit_counterLB == 0) switch_stateLB = 0;
	
	if (bit_counterstart == mask) switch_statestart = 1;
	else if (bit_counterstart == 0) switch_statestart = 0;
	
	if (bit_counterup == mask2) switch_stateup = 1;
	else if (bit_counterup == 0) switch_stateup = 0;
	
}

void set_duty_cycle(int duty_cycle) {
	// (a)	Set bits 8 and 9 of Output Compare Register 4A.
	TC4H = duty_cycle >> 8;

	// (b)	Set bits 0..7 of Output Compare Register 4A.
	OCR4A = duty_cycle & 0xff;
}

void timerCount()
{
	double time = get_current_time();
	minutes = trunc(time/60);
	seconds = time - 60*minutes;
}

void process ( void )
{	
	int c = 0;
	if ( usb_serial_available() ) c = usb_serial_getchar();
	start_game(c); //only at the start
	
	
	if (startCheck == true && start_life > 0) // real game process
	{
		if (zombieLand() == true) // off LED
		{
			CLEAR_BIT(PORTB, 2);
			CLEAR_BIT(PORTB, 3);
		}
		if (seconds == 3 && zombieSpawn == false)
		{
			setup_zombies();
			zombieSpawn = true;
			char s[150];
			sprintf(s, "Zombie spawned! Score: %d\tLife: %d\tTime: %d:%d No. zombies: %d\r\n ",\
			score, start_life, minutes, seconds, MAX_ZOMBIES);
			usb_serial_write((uint8_t *) s, strlen(s));
		}
		light = false;
		if (num_food > MAX_FOOD) num_food = MAX_FOOD;
		if (gamePaused == false)
		{
			clear_screen();
			gravity();
			score_tracker(gravity());
			move_treasure();
			check_zombie(move_zombie(zombies, MAX_ZOMBIES));
			timerCount();
			hero_bump();
		}
		pause_game(c);
		
		if (hero.y + 6> LCD_Y -1 ) 
		{
			if (start_life > 1)
			{	
				fade_out();
				fade_in();
			}
			start_life -= 1;
			hero.dx = BLOCK_RIGHT_SPEED;
			hero.dy = 0;
			sprite_move_to(&hero, find_safe_block(), 1);
		}	// checks for fall to death
		check_respawn(); // teleport hero when left or right of screen
		
		// MOVE HERO
		if (check == true && gamePaused == false && startCheck == true) move_hero(c);

		// MOVE PLATFORM
		if (gamePaused == false) move_platform();
		
		
		add_life(&start_life);
		
		pause_treasure(move_treasure(), c);		
		draw_all(); // KEEP INTACT
		if (zombieSpawn == true) draw_sprites(zombies, MAX_ZOMBIES);
	}
	show_screen();
	
	if (start_life <= 0) // game over screen
	{
		static uint8_t prevStateRB = 0;
		static uint8_t prevStateLB = 0;
		if (gamePaused == false)
		{
			char s[150];
			sprintf(s, "Game over! Score: %d\tLife: %d\tTime: %d:%d No. zombies fed: %d\r\n ",\
			score, start_life, minutes, seconds, fedzombies);
			usb_serial_write((uint8_t *) s, strlen(s));
		}
		gamePaused = true;
		if ( switch_stateRB != prevStateRB ) // rb press
		{ 
			prevStateRB = switch_stateRB;
			if (prevStateRB == 1 && end_game == false) restart_game();
		}
		
		else if (c == 'r' && end_game == false) restart_game();
		
		if ( switch_stateLB != prevStateLB )  // lb press to end game
		{
			prevStateLB = switch_stateLB;
			if (prevStateLB == 1)
			{
				clear_screen();
				draw_string(15, 15, "n9972676", FG_COLOUR);
				LCD_CMD(lcd_set_function, lcd_instr_extended);
				LCD_CMD(lcd_set_contrast, LCD_DEFAULT_CONTRAST);
				LCD_CMD(lcd_set_function, lcd_instr_basic);	
				show_screen();
				end_game = true;
			}
		}
		
		else if (c == 'q')
		{
			clear_screen();
			draw_string(15, 15, "n9972676", FG_COLOUR);
			LCD_CMD(lcd_set_function, lcd_instr_extended);
			LCD_CMD(lcd_set_contrast, LCD_DEFAULT_CONTRAST);
			LCD_CMD(lcd_set_function, lcd_instr_basic);	
			show_screen();
			end_game = true;
		}
		if (end_game == false) 
		{
			life_over();
		}
	}
	
}



int main(void) {
	setup();
	for ( ;; ) {
		process();
		_delay_ms(10);
	}

	return 0;
}