#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <cab202_graphics.h>
#include <cab202_sprites.h>
#include <cab202_timers.h>
#include <time.h>

#define DELAY (10) /* Time in millisecond, delay between game updates */

#define HERO_WIDTH (3)
#define HERO_HEIGHT (3)
#define DISPLAY_HEIGHT (5)
#define MAX_BLOCK (200)
#define MAX_BLOCK_WIDTH (10)
#define MIN_BLOCK_WIDTH (5)
#define COLUMN ((screen_width()/MAX_BLOCK_WIDTH) + 1);

sprite_id hero;

sprite_id treasure;
sprite_id treasureanimation;

sprite_id blocks[MAX_BLOCK];

int num_of_columns(void) // count number of columns for your screen size
{
	return ((screen_width()-1)/(MAX_BLOCK_WIDTH + 2) + 2) ;
}

int num_of_rows(void) // count number of rows for your screen size
{
	return ((screen_height() - DISPLAY_HEIGHT - 2) / 5) - 2;
}

int num_blocks() // number of blocks that needs to be created for your screen size
{
	return num_of_columns() * num_of_rows();
}
timer_id timer;

//int column = (screen_width()/MAX_BLOCK_WIDTH) + 1;

// Game state.
bool game_over = false; /* Set this to true when game is over */
bool update_screen = true; /* Set to false to prevent screen update. */
bool treasure_collide = false; // collision of player and treasure
bool state = false; // treasure pause state
bool direction = false; // resumes treasure direction
bool check = true; // toggle for gravity
bool * checkaddr = &check; 
int start_life = 10; // life tracker
int score = 0; // score tracker
int seconds = 0; // time in seconds
int minutes = 0; // time in minutes

// My cute hero
char *bokuhero =
/**/	" o "
/**/	"/|\\"
/**/	"/ \\" ;

char *herorevive =
/**/	"\\X/"
/**/	" | "
/**/	"/ \\";

char *herojump =
/**/	"\\O/"
/**/	" | "
/**/	"/ \\";

char *heroleft =
/**/	" O "
/**/	"\\|\\"
/**/	"/ \\";

char *heroright =
/**/	" O "
/**/	"/|/"
/**/	"/ \\";

// My moneyy
char *bokutreasure =
/**/	"$$"
/**/	"$$";

char *treasure_image =
/**/	"##"
/**/	"##";

char *goodblocks_image=
/**/	"=========="
/**/	"==========";

char * badblocks_image =
/**/	"XXXXXXXXXX"
/**/	"XXXXXXXXXX";

char * emptyblocks_image =
/**/	"          "
/**/	"          ";



sprite_id setup_hero() 
{
    int x = screen_width() - 8;
    int y = DISPLAY_HEIGHT + HERO_HEIGHT - 1;
    return sprite_create(x, y, HERO_WIDTH, HERO_HEIGHT, bokuhero);
	
}

sprite_id setup_treasure()
{
	int treasurew = 2;
	int treasureh = 2;
    int x = treasurew;
    int y = screen_height() - treasureh - 1;
    treasure = sprite_create(x, y, treasurew, treasureh, bokutreasure);
	sprite_turn_to(treasure, 0.1, 0);
	return treasure;
}

sprite_id setup_treasureanimation()
{
	int treasurew = 2;
	int treasureh = 2;
    int x = treasurew;
    int y = screen_height() - treasureh - 1;
    treasureanimation = sprite_create(x, y, treasurew, treasureh, treasure_image);
	sprite_turn_to(treasureanimation, 0.1, 0);
	return treasureanimation;
}


bool sprites_collide(sprite_id s1, sprite_id s2) // check for collision
{
    int top1 = round(sprite_y(s1));
    int bottom1 = top1 + sprite_height(s1) - 1;
    int left1 = round(sprite_x(s1));
    int right1 = left1 + sprite_width(s1) - 1;

    int top2 = round(sprite_y(s2));
    int bottom2 = top2 + sprite_height(s2) - 1;
    int left2 = round(sprite_x(s2));
    int right2 = left2 + sprite_height(s2) - 1;

    if (top1 > bottom2) return false;
    else if (top2 > bottom1) return false;
    else if (right1 < left2) return false;
    else if (right2 < left1) return false;
    else return true;
}

sprite_id sprites_collide_any(sprite_id s, sprite_id sprites[], int n) // check for collision in array
{
	sprite_id result = NULL;

    for ( int i = 0; i < n; i++ )
    {
        if ( sprites_collide( s, sprites[i] ) )
        {
            result = sprites[i];
            break;
        }
    }

    return result;
}

void hero_bump(void) // if hits corner of block
{
	if ( sprites_collide_any(hero, blocks, num_blocks()) ) // if hit
	{
		
		sprite_id corner = sprites_collide_any(hero, blocks, num_blocks());
		if (corner->bitmap != emptyblocks_image) // if hit
		{
			sprite_back(hero);
			sprite_turn_to(hero, 0 , hero->dy); //rebound
		}
	}
}

sprite_id setup_block(int x, int y,int blockNo)
{
	int block_width, luck;
	char * image = goodblocks_image;
	if (x+10 >= sprite_x(hero) && x <= sprite_x(hero) && y == DISPLAY_HEIGHT + HERO_HEIGHT + 2)
	{
		image = goodblocks_image;
		block_width = 10;
	}
	else
	{
		block_width = rand() % (10 -5 +1) + 5;
		luck = rand() % 30; 
		if  (luck % 2 == 0) image = badblocks_image;
		else if ( luck % 7 == 0) image = emptyblocks_image;
		if (image == emptyblocks_image) block_width = 10;
		if (blockNo != 0) // do not allow 2 bad blocks together for gameplay
		{
			if (blocks[blockNo-1] ->bitmap == badblocks_image) image = goodblocks_image;
		}
	}
	if ( x + 4 > screen_width()-1) x++;
    sprite_id new_block = sprite_create( x, y, block_width, 2, image );
    return new_block;
}



int countRows(int blockNo) // returns row number that the block is in
{
	double rowNum = blockNo/num_of_columns();
	int rowNumber;
	if (blockNo % num_of_columns() == 0)	rowNumber = rowNum ;
	else if (blockNo == 0) rowNumber = 1;
	else rowNumber = (trunc(rowNum) + 1);	
	return rowNumber;
}

double countColumns (int blockNo) // returns column number that the block is in
{
	double columnNumber = (blockNo % num_of_columns());
	return columnNumber;
}

void setup_bunchofblocks()
{
	//int row_num = (screen_height() - DISPLAY_HEIGHT) / 5; // number of rows
	int row1 = DISPLAY_HEIGHT + HERO_HEIGHT + 2 ; // y of row 1
	int by, bx;
	for (int i = 0; i< MAX_BLOCK; i++) // each block
	{	
		if (countRows(i) > num_of_rows())
		{
			break;
		}
		by = ((countRows(i)-1)*7) + row1; // gives y coords corresponding to the row
		if (countRows(i) == 0) by = row1; // solve block[0] issue
		bx =  (countColumns(i)) * (MAX_BLOCK_WIDTH+1) + 2 ;	// gives x coords cooresponding to column
		blocks[i] = setup_block(bx,  by, i); 
		sprite_hide(blocks[0]);
	}
}

void draw_sprites( sprite_id sids[], int n) //draw sprite array
{
	for (int i = 0; i < n; i++)
	{
		sprite_draw(sids[i]);
	}
}

char buffer(int milliseconds) // clears keypresses for milliseconds
{ 
	timer_pause(milliseconds);
    while (get_char() >= 0)
    {
		// Do nothing
    }
    char key = wait_char();
	return key;
}

bool sprites_above(sprite_id above, sprite_id below) // logic for hero standing on platform
{
    int top1 = round(sprite_y(above));
    int bottom1 = top1 + sprite_height(above) - 1;
    int left1 = round(sprite_x(above));
    int right1 = left1 + sprite_width(above) - 1;

    int top2 = round(sprite_y(below));
    int left2 = round(sprite_x(below));
    int right2 = left2 + sprite_width(below) - 1;

    if (bottom1 == top2 - 1 && right1 >= left2 && left1 <= right2)
    {
        return true;
    }
    else 
	{
        return false;
    }
}

sprite_id sprites_above_any( sprite_id s, sprite_id sprites[], int n) // if standing on any blocks[i]
{
	sprite_id result = NULL;

    for ( int i = 0; i < n; i++ )
    {
        if ( sprites_above( s, sprites[i] ) )
        {
            result = sprites[i];
            break;
        }
    }

    return result;
}

int find_safe_block(void) //respawn to safety
{
	int respawn;
	for (int i = 1; i < num_of_columns(); i++)
		{
			if (blocks[i]->bitmap == goodblocks_image)
			{
				respawn = sprite_x(blocks[i]);
				break;
			}
		}
	return respawn;
}

void check_respawn(void) // check if player out of screen left and right.
{
	if (sprite_x(hero) <= 0)
	{
		start_life -= 1;		
		sprite_turn_to(hero, 0, 0);
		hero->bitmap = herorevive;
		sprite_move_to(hero, find_safe_block(), DISPLAY_HEIGHT + HERO_HEIGHT -1);
	}
	else if (sprite_x(hero) >= screen_width()-2)
	{
		start_life -= 1;
		sprite_turn_to(hero, 0, 0);
		hero->bitmap = herorevive;
		sprite_move_to(hero, find_safe_block(), DISPLAY_HEIGHT + HERO_HEIGHT -1);
	}
}


void platform_movement(sprite_id sid, int rowNum) // movement of platform altenating
{
	int zx = round( sprite_x( sid ) );
	if (rowNum % 2 == 0 && rowNum != num_of_rows() && rowNum != 0) // for even numbers
		{
			sprite_turn_to(sid, -0.05, 0);
			if (sprites_above(hero, sid)) // if hero is on block
			{
				if (sid->bitmap != emptyblocks_image) // and the block is not empty
				{
					sprite_turn_to(hero, -0.05 , 0);
					sprite_step(hero);
				}
				check_respawn(); //check if out of screen
			}
			if ( zx + 9 == 0 ) sid->x = screen_width();
		}
	
	else if (rowNum % 2 != 0 && rowNum != 1 && rowNum != num_of_rows()) // for odd numbers 
		{
			sprite_turn_to(sid, 0.1, 0);
			if (sprites_above(hero, sid)) // if hero on block
			{
				if (sid->bitmap != emptyblocks_image) // and block not empty
				{
					sprite_turn_to(hero, 0.1 , 0);
					sprite_step(hero);	
				}					
				check_respawn(); //check if out of screen
			}
			if (zx > screen_width()-1) sid->x = 0;
		}
}

void auto_move( sprite_id sid, int keyCode, int rowNum)  // moves row
{
    if ( (keyCode == 'z' || keyCode < 0) )
    {
		platform_movement(sid, rowNum);
        sprite_step( sid );
    }
}

void auto_move_all( sprite_id sprites[], int n, int key ) // moves array of blocks
{
    for ( int i = 0; i < n; i++ )
    {
        auto_move( sprites[i], key, countRows(i) );
    }
}
	

void add_life(int * lives) // add lives if collide and updates display screen
{
	if (sprites_collide(treasure, hero) && treasure_collide == false) 
	{	
		int x = screen_width() - 8;
		int y = DISPLAY_HEIGHT + HERO_HEIGHT - 1;
		(* lives) += 2;
		sprite_hide(treasure);
		sprite_hide(treasureanimation);
		hero->x = x;
		hero->y = y;
		treasure_collide = true;
	}
	draw_int(18, 2,  * lives);
}

bool gravity(void) // free fall at 0.002 units per update
{
	sprite_id onblock = sprites_above_any(hero, blocks, num_blocks());
	if (!sprites_above_any(hero, blocks, num_blocks()) || (onblock->bitmap == emptyblocks_image && sprite_dx(onblock) == 0))
	{
		hero->dy+= 0.002;
		sprite_step(hero);
		check = false;
		return check;
	}
	else hero->dy =0; //reset acceleration
	
	return check;
}

void score_tracker(bool check) // tracks score and life if lands on block
{
	sprite_id block_check = sprites_above_any(hero, blocks, num_blocks());
	if (check == false && block_check)
	{
		if (block_check->bitmap == badblocks_image) // deducts life if lands on bad block
		{
			start_life-= 1;
			*checkaddr = true;
			hero->dx = 0;
			sprite_set_image(hero, bokuhero);
			sprite_move_to(hero, find_safe_block(), DISPLAY_HEIGHT + HERO_HEIGHT -1);
			
		}
		
		else if (block_check->bitmap == goodblocks_image) // adds score if lands on good block
		{
		// step on good blocks
		score+= 1;
		*checkaddr = true;
		hero->dx = 0; // removes dx when jumped once so it wont keep hopping
		sprite_set_image(hero, bokuhero);
		}	
		
		else 
		{
			sprite_move(hero, 0, 0.5);
			sprite_turn_to(hero, 0, 0.001);
			sprite_step(hero);
			check = false;
		}
		
	}
}


void draw_timer(){
	timer = create_timer(1000);
	if (!timer_expired(timer)) // count up
	{
		seconds += 1;
		timer_reset(timer);
	}
	if (seconds == 5900) // resets seconds
	{
		seconds = 0;
		minutes += 1;
	}

	if (seconds< 1000)	// Seconds printer
	{
		draw_string(33, 2, "0");
		draw_int(34, 2, seconds/100.0);
	}
	else if (seconds >= 1000) draw_int(33, 2,  seconds/100.0);

	if (minutes< 10)	// Minutes printer
	{
		draw_string(30, 2, "0");
		draw_int(31, 2, minutes);
	}
	else if (minutes >= 10) draw_int (30, 2, minutes);
}

void display_screen(){
	
	int * addLife = &start_life; // Life part
	add_life(addLife);
	
	draw_line(0, 0, screen_width()-1, 0, '~'); 	// Drawing borders
	draw_line(0, 5, screen_width()-1, 5, '~');

	draw_string(1, 2, "N9972676"); 	// Writing criteria 1a,b,c,d
	draw_string(10, 2, "Lives: ");
	draw_string(25, 2, "Time: ");
	draw_string(32, 2, ":");
	draw_string(40, 2, "Score: ");
	draw_int(47, 2, score);
}

void draw_all(void) // function to draw everything
{	
	clear_screen();
	display_screen();
	draw_timer();
	sprite_draw(hero);
	draw_sprites( blocks, num_blocks());
	if (seconds % 3 == 0) sprite_draw(treasureanimation);
	else sprite_draw(treasure);
}


void setup(void) // setup game.
{ 
	int seed = time(NULL);
	srand(seed);
	hero = setup_hero();
	treasure = setup_treasure();
	treasureanimation = setup_treasureanimation();
	setup_bunchofblocks();

	draw_all();	// Displays set up
}

bool move_treasure(sprite_id sid, int key_code, int term_width, int h) // movement for treasure, left right and rebound
{
	int tx = round(sprite_x(sid));
	double tdx = sprite_dx(sid);
    if ( key_code < 0)
    {
        sprite_step(sid);
		
        if (tx <= 0) tdx = fabs(tdx);
        else if (tx >= term_width - sprite_width(sid)) tdx = -fabs(tdx);
		
        if (tdx != sprite_dx(sid))
            sprite_back(sid);
        {
            sprite_turn_to(sid, tdx, 0);
        }
    }
	if (tdx > 0) direction = false;  // set false as right
	if (tdx < 0) direction = true;  // set true as left
	return direction;
}

void pause_treasure(int key_code, bool direction, bool direction2) // pause treasure and save directions
{
	if (key_code == 't' && state == false)
	{
		state = !state;
		sprite_turn_to(treasure, 0,0);
		sprite_turn_to(treasureanimation, 0, 0);
	}
	else if (key_code == 't' && state == true)
	{
		state = !state;
		if (direction == false)
		{
			sprite_turn_to(treasure, 0.1, 0);
			sprite_turn_to(treasureanimation, 0.1, 0);
		}
		else 
		{
			sprite_turn_to(treasure, -0.1, 0);
			sprite_turn_to(treasureanimation, -0.1, 0);
		}
	}
}


void jump() // it says right there it's a jump function
{
	if (!sprites_above_any(hero, blocks, num_blocks())) // does not allow double jump
		{
			// do nothing
		}
	else if (sprites_above_any(hero,blocks, num_blocks())->bitmap == emptyblocks_image) // prevent jump on empty block
	{
			// do nothing
	}
	else 
	{
		sprite_id onblock = sprites_above_any(hero, blocks, num_blocks());
		if ( sprite_dx(onblock) == 0)
		{
			sprite_turn_to(hero, 25*hero->dx, hero->dy);
			sprite_move(hero,0, -1);
		}
		else sprite_move(hero,0, -1);
	}
	sprite_set_image(hero, herojump);
}

void move_hero(int key_code, int term_width, int term_height) // key press for hero
{
    int hx = round(sprite_x(hero));
    int hy = round(sprite_y(hero));
	
	if (key_code == 'a' && hx > 1) 
	{
		sprite_move(hero, -1 + hero->dx, 0); // runs with acceleration
		if (sprite_dx(hero) > 0) hero->dx = 0; // clears acceleration if previously moving right
		if (hero->dx > -0.005) hero->dx -= 0.020; // accelerate to max of 0.6 to the left
		sprite_set_image(hero, heroleft); // animation
	}
	
    if (key_code == 'd' && hx < term_width - sprite_width(hero) - 1) 
	{
		sprite_move(hero, 1 + hero->dx, 0); // runs with acceleration
		if (sprite_dx(hero) < 0) hero->dx = 0; // clears acceleration if previously moving left
		if (hero->dx < 0.005) hero->dx += 0.020;// accelerate to max of 0.6 to the right
		sprite_set_image(hero, heroright); // animation
	}
    if (key_code == 'w' && hy > 1 + DISPLAY_HEIGHT) jump();
	
    //if (key_code == 's' && hy < term_height - sprite_height(hero) - 1) sprite_move(hero, 0, +1); // to move down for testing
}
	

void restart_game() // clear state of everything
{
	minutes = 0;
	seconds = 0;
	start_life = 10;
	score = 0;
	sprite_show(treasure);
	sprite_show(treasureanimation);
	sprite_move_to(treasure, 0, screen_height() -3);// move to start
	sprite_move_to(treasureanimation, 0, screen_height() -3); //move to start
	hero.dx = 0;
	hero.dy = 0;
	sprite_move_to(hero, find_safe_block(), DISPLAY_HEIGHT + HERO_HEIGHT -1);
	treasure_collide = false; // when restart we can collide again
	state = false; // treasure pause state
	direction = false; // resumes treasure direction
	check = true; // toggle for gravity
}

void life_over() // when player dies.
{
	int w = (screen_width()/ 2) - 10;
	int h = screen_height()/ 2;
	clear_screen();
	draw_string(w, h, "U DED. Your score is: " );
	draw_int(w + 22, h, score);
	draw_string(w, h + 1, "Total playtime is 00:00");
	draw_int(w + 19, h + 1, minutes);
	if (seconds/100 < 10) draw_int(w + 22, h + 1, seconds/100);
	
	else if(seconds/100 >=10) draw_int (w+21, h+1, seconds/100);
	
	draw_string(w - 5, h + 2, "Press R to play again, Q to quit.");
	show_screen();
	int key = buffer(500);
	
	if (key == 'r')
	{
		restart_game();
		game_over = false;
	}
	
	else if (key == 'q')
	{		
		game_over = true;
	}
}

// Play one turn of game.
void process(void) {
//  Get a character code from standard input without waiting.
	int keyCode = get_char();
	int w = screen_width();
    int h = screen_height();
	if (start_life == 0) life_over(); // checks for 0 lives
	if (sprite_y(hero) > screen_height()-1)
	{
		start_life -= 1;
		sprite_move_to(hero, find_safe_block(), DISPLAY_HEIGHT + HERO_HEIGHT -1);
	}	// checks for fall to death
	
	gravity(); //constant gravity
	
	score_tracker(gravity());
	move_hero(keyCode, w, h);	
	hero_bump();
	check_respawn();
	pause_treasure(keyCode, move_treasure(treasure, keyCode, screen_width(), screen_height()), move_treasure(treasureanimation, keyCode, screen_width(), screen_height()));
	
	move_treasure(treasure, keyCode, screen_width(), screen_height());
	move_treasure(treasureanimation, keyCode, screen_width(), screen_height());
	
	auto_move_all( blocks, num_blocks(), keyCode);
	
	draw_all();
}

// Clean up game
void cleanup(void) {
	// STATEMENTS
}

// Program entry point.
int main(void) {
	setup_screen();
	setup();
	show_screen();
	if (start_life == 0) life_over();
	while ( !game_over ) {
		process();

		if ( update_screen ) {
			show_screen();
		}

		timer_pause(DELAY);
	}

	cleanup();
	
	return 0;
}