//
//  main.c
//  Final Project CSC412
//
//  Created by Jean-Yves Hervé on 2020-12-01
//	This is public domain code.  By all means appropriate it and change is to your
//	heart's content.
#include <iostream>
#include <string>
#include <random>
//
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
//
#include <queue>
#include "gl_frontEnd.h"

using namespace std;

//==================================================================================
//	Function prototypes
//==================================================================================
void initializeApplication(void);
GridPosition getNewFreePosition(void);
Direction newDirection(Direction forbiddenDir = NUM_DIRECTIONS);
TravelerSegment newTravelerSegment(const TravelerSegment &currentSeg, bool &canAdd);
void generateWalls(void);
void generatePartitions(void);

void threadFunc(unsigned int travelerIndex);
void manualMove(unsigned int travelerIndex, Direction heading);
//==================================================================================
//	Application-level global variables
//==================================================================================

//	Don't rename any of these variables
//-------------------------------------
//	The state grid and its dimensions (arguments to the program)
SquareType **grid;
unsigned int numRows = 0;	   //	height of the grid
unsigned int numCols = 0;	   //	width
unsigned int numTravelers = 0; //	initial number
//
unsigned int travelerTailTimer = 0; // moves to make a new tail segment
//
unsigned int numTravelersDone = 0;
unsigned int numLiveThreads = 0; //	the number of live traveler threads
vector<Traveler> travelerList;
vector<SlidingPartition> partitionList;
GridPosition exitPos; //	location of the exit

unsigned int maxStall = 35;

//	travelers' sleep time between moves (in microseconds)
const int MIN_SLEEP_TIME = 1000;
int travelerSleepTime = 100000;

int **gridPath;

queue<GridPosition> pathQ;

typedef struct ThreadInfo
{
	pthread_t threadID;		  // currently not used
	unsigned int threadIndex; // currently not used

	unsigned int travelerIndex; // currently not used

} ThreadInfo;

//	An array of C-string where you can store things you want displayed
//	in the state pane to display (for debugging purposes?)
//	Dont change the dimensions as this may break the front end
const int MAX_NUM_MESSAGES = 8;
const int MAX_LENGTH_MESSAGE = 32;
char **message;
time_t launchTime;

bool run = false;
unsigned int botControl = 0;
//	Random generators:  For uniform distributions
const unsigned int MAX_NUM_INITIAL_SEGMENTS = 6;
random_device randDev;
default_random_engine engine(randDev());
uniform_int_distribution<unsigned int> unsignedNumberGenerator(0, numeric_limits<unsigned int>::max());
uniform_int_distribution<unsigned int> segmentNumberGenerator(0, MAX_NUM_INITIAL_SEGMENTS);
uniform_int_distribution<unsigned int> segmentDirectionGenerator(0, NUM_DIRECTIONS - 1);
uniform_int_distribution<unsigned int> headsOrTails(0, 1);
uniform_int_distribution<unsigned int> rowGenerator;
uniform_int_distribution<unsigned int> colGenerator;

//==================================================================================
//	These are the functions that tie the simulation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//==================================================================================

void drawTravelers(void)
{
	//-----------------------------
	//	You may have to sychronize things here
	//-----------------------------
	for (unsigned int k = 0; k < travelerList.size(); k++)
	{
		//	here I would test if the traveler thread is still live, which is to say that it hasn't exited and ended itself
		if (travelerList[k].render)
		{
			drawTraveler(travelerList[k]);
			threadFunc(k);
		}
	}
}

void updateMessages(void)
{
	//	Here I hard-code a few messages that I want to see displayed
	//	in my state pane.  The number of live robot threads will
	//	always get displayed.  No need to pass a message about it.
	unsigned int numMessages = 4;
	sprintf(message[0], "We created %d travelers", numTravelers);
	sprintf(message[1], "%d travelers solved the maze", numTravelersDone);
	sprintf(message[2], "I like cheese");
	sprintf(message[3], "Simulation run time is %ld", time(NULL) - launchTime);

	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//
	//	You *must* synchronize this call.
	//
	//---------------------------------------------------------
	drawMessages(numMessages, message);
}

void handleKeyboardEvent(unsigned char c, int x, int y)
{
	int ok = 0;

	switch (c)
	{
	//	'esc' to quit
	case 27:
		exit(0);
		break;

	//	slowdown
	case ',':
		slowdownTravelers();
		ok = 1;
		break;

	//	speedup
	case '.':
		speedupTravelers();
		ok = 1;
		break;

	default:
		ok = 1;
		break;
	}
	if (!ok)
	{
		//	do something?
	}
}

//------------------------------------------------------------------------
//	You shouldn't have to touch this one.  Definitely if you don't
//	add the "producer" threads, and probably not even if you do.
//------------------------------------------------------------------------

void speedupTravelers(void)
{
	//	decrease sleep time by 20%, but don't get too small
	int newSleepTime = (8 * travelerSleepTime) / 10;

	if (newSleepTime > MIN_SLEEP_TIME)
	{
		travelerSleepTime = newSleepTime;
	}
}

void slowdownTravelers(void)
{
	//	increase sleep time by 20%
	travelerSleepTime = (12 * travelerSleepTime) / 10;
}

//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function besides
//	initialization of the various global variables and lists
//------------------------------------------------------------------------
int main(int argc, char **argv)
{
	//	We know that the arguments  of the program  are going
	//	to be the width (number of columns) and height (number of rows) of the
	//	grid, the number of travelers, etc.

	numCols = atoi(argv[1]);
	numRows = atoi(argv[2]);
	numTravelers = atoi(argv[3]);
	travelerTailTimer = atoi(argv[4]);
	numLiveThreads = 0;
	numTravelersDone = 0;

	//	Even though we extracted the relevant information from the argument
	//	list, I still need to pass argc and argv to the front-end init
	//	function because that function passes them to glutInit, the required call
	//	to the initialization of the glut library.
	initializeFrontEnd(argc, argv);

	//	Now we can do application-level initialization
	initializeApplication();

	launchTime = time(NULL);

	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();

	//	Free allocated resource before leaving (not absolutely needed, but
	//	just nicer.  Also, if you crash there, you know something is wrong
	//	in your code.
	for (unsigned int i = 0; i < numRows; i++)
		free(grid[i]);
	free(grid);
	for (int k = 0; k < MAX_NUM_MESSAGES; k++)
		free(message[k]);
	free(message);

	//	This will probably never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}

//==================================================================================
//
//	This is a function that you have to edit and add to.
//
//==================================================================================

//	check to make sure the move is safe, also checks to see if the move will be delayed, so the thread can prioritize an
// 		immediately available move of equal value instead of waiting.
bool safeMove(unsigned int travelerIndex, Direction moveDirection)
{

	// checks if the direction is a valid move by checking the grid, returns true if move is good
	if (travelerList[travelerIndex].exiting)
	{
		return true;
	}
	else
	{

		switch (moveDirection)
		{
		case NORTH:
			if (travelerList[travelerIndex].segmentList[0].row + 1 < numRows)
			{
				if (grid[travelerList[travelerIndex].segmentList[0].row + 1][travelerList[travelerIndex].segmentList[0].col] == EXIT)
				{
					travelerList[travelerIndex].exiting = true;

					return true;
				}
				if (grid[travelerList[travelerIndex].segmentList[0].row + 1][travelerList[travelerIndex].segmentList[0].col] == FREE_SQUARE)
				{
					return true;
				}
				if (grid[travelerList[travelerIndex].segmentList[0].row + 1][travelerList[travelerIndex].segmentList[0].col] == TRAVELER)
				{
					if (travelerList[travelerIndex].moveCount <= maxStall)
					{
						travelerList[travelerIndex].wouldStall = true;
						return true;
					}
					else
					{
						return false;
					}
				}
			}
			break;

		case SOUTH:
			if (travelerList[travelerIndex].segmentList[0].row > 0)
			{
				if (grid[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col] == EXIT)
				{
					travelerList[travelerIndex].exiting = true;

					return true;
				}
				if (grid[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col] == FREE_SQUARE)
				{
					return true;
				}
				if (grid[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col] == TRAVELER)
				{
					if (travelerList[travelerIndex].moveCount <= maxStall)
					{
						travelerList[travelerIndex].wouldStall = true;
						return true;
					}
					else
					{
						return false;
					}
				}
			}
			break;

		case WEST:
			if (travelerList[travelerIndex].segmentList[0].col + 1 < numCols)
			{
				if (grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1] == EXIT)
				{
					travelerList[travelerIndex].exiting = true;

					return true;
				}
				if (grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1] == FREE_SQUARE)
				{
					return true;
				}
				if (grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1] == TRAVELER)
				{
					if (travelerList[travelerIndex].moveCount <= maxStall)
					{
						travelerList[travelerIndex].wouldStall = true;
						return true;
					}
					else
					{
						return false;
					}
				}
			}
			break;

		case EAST:
			if (travelerList[travelerIndex].segmentList[0].col > 0)
			{
				if (grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1] == EXIT)
				{
					travelerList[travelerIndex].exiting = true;

					return true;
				}
				if (grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1] == FREE_SQUARE)
				{
					return true;
				}
				if (grid[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1] == TRAVELER)
				{
					if (travelerList[travelerIndex].moveCount <= maxStall)
					{
						travelerList[travelerIndex].wouldStall = true;
						return true;
					}
					else
					{
						return false;
					}
				}
			}
			break;
		default:
			return false;
		}

		return false;
	}
}

// 		move the traveler in a certain direction.
// 		Assumes that the direction has already been confirmed as safe.
// 		if exiting, it will automatically move, and 'drain' into the exit, and adjust the traveler length accordingly.
void travel(unsigned int travelerIndex, Direction moveDirection)
{

	// need to create a new traveler head segment with correct properties
	TravelerSegment newHead;

	if (!travelerList[travelerIndex].exiting)
	{
		switch (moveDirection)
		{
		case NORTH:
			newHead.col = travelerList[travelerIndex].segmentList[0].col;
			newHead.row = travelerList[travelerIndex].segmentList[0].row + 1;
			grid[newHead.row][newHead.col] = TRAVELER;
			newHead.dir = SOUTH;
			break;

		case SOUTH:
			newHead.col = travelerList[travelerIndex].segmentList[0].col;
			newHead.row = travelerList[travelerIndex].segmentList[0].row - 1;
			grid[newHead.row][newHead.col] = TRAVELER;
			newHead.dir = NORTH;
			break;

		case WEST:
			newHead.col = travelerList[travelerIndex].segmentList[0].col + 1;
			newHead.row = travelerList[travelerIndex].segmentList[0].row;
			grid[newHead.row][newHead.col] = TRAVELER;
			newHead.dir = EAST;
			break;

		case EAST:
			newHead.col = travelerList[travelerIndex].segmentList[0].col - 1;
			newHead.row = travelerList[travelerIndex].segmentList[0].row;
			grid[newHead.row][newHead.col] = TRAVELER;
			newHead.dir = WEST;

			break;
		default:;
		}

		// insert the new tail segement at the front of the list of segments
		travelerList[travelerIndex].segmentList.insert(travelerList[travelerIndex].segmentList.begin(), newHead);

		// is growth is not meant to happen, delete the last tail segment in the list

		if (travelerTailTimer != travelerList[travelerIndex].tailCount)
		{
			int temp = travelerList[travelerIndex].segmentList.size() - 1;

			// delete the last segment
			travelerList[travelerIndex].tailCount++;
			grid[travelerList[travelerIndex].segmentList[temp].row][travelerList[travelerIndex].segmentList[temp].col] = FREE_SQUARE;
			travelerList[travelerIndex].segmentList.pop_back();
		}
		else
		{
			travelerList[travelerIndex].tailCount = 0;
		}
	}
	else
	{

		int temp = travelerList[travelerIndex].segmentList.size() - 1;

		if (temp == 0 && travelerList[travelerIndex].exiting == true && travelerList[travelerIndex].render == true)
		{
			// makes the traveler fully disappear once the last section exits
			grid[travelerList[travelerIndex].segmentList[temp].row][travelerList[travelerIndex].segmentList[temp].col] = FREE_SQUARE;
			travelerList[travelerIndex].render = false;
			numTravelersDone++;
		}
		else
		{

			// delete the last segment
			grid[travelerList[travelerIndex].segmentList[temp].row][travelerList[travelerIndex].segmentList[temp].col] = FREE_SQUARE;
			travelerList[travelerIndex].segmentList.pop_back();
		}
	}
}

// goal for this is get the best moves from the public read-only pathfinding before geting the lock
void threadFunc(unsigned int travelerIndex)
{
	usleep(500);

	// stores the best direction
	Direction bestMove;

	// stores the pathfinding value of the best direction
	int bestMoveVal = 99999;

	// this just resets the values, to be safe.
	travelerList[travelerIndex].stall = false;

	// this handles movement, comparing the available moves and checking to see
	// 	not only what the best move is, but also if there is a move that can be made immediatly that is of similar merit.
	if (travelerList[travelerIndex].exiting != true)
	{
		if (travelerList[travelerIndex].segmentList[0].dir != NORTH && (safeMove(travelerIndex, NORTH)) &&
			(gridPath[travelerList[travelerIndex].segmentList[0].row + 1][travelerList[travelerIndex].segmentList[0].col] < gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col]))
		{

			if (gridPath[travelerList[travelerIndex].segmentList[0].row + 1][travelerList[travelerIndex].segmentList[0].col] < bestMoveVal)
			{
				bestMove = NORTH;
				bestMoveVal = gridPath[travelerList[travelerIndex].segmentList[0].row + 1][travelerList[travelerIndex].segmentList[0].col];
				if (travelerList[travelerIndex].wouldStall)
				{
					travelerList[travelerIndex].wouldStall = false;
					travelerList[travelerIndex].stall = true;
				}
				else
				{
					travelerList[travelerIndex].stall = false;
				}
			}
			travelerList[travelerIndex].wouldStall = false;
		}

		if (travelerList[travelerIndex].segmentList[0].dir != WEST && (safeMove(travelerIndex, WEST)) &&
			(gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1] < gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col]))
		{
			if (gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1] < bestMoveVal)
			{
				bestMove = WEST;
				bestMoveVal = gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1];
				if (travelerList[travelerIndex].wouldStall)
				{
					travelerList[travelerIndex].wouldStall = false;
					travelerList[travelerIndex].stall = true;
				}
				else
				{

					travelerList[travelerIndex].stall = false;
				}
			}
			else if (gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1] == bestMoveVal)
			{
				if (!travelerList[travelerIndex].wouldStall && travelerList[travelerIndex].stall)
				{
					travelerList[travelerIndex].stall = false;
					bestMove = WEST;
					bestMoveVal = gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col + 1];
				}
			}
			travelerList[travelerIndex].wouldStall = false;
		}

		if (travelerList[travelerIndex].segmentList[0].dir != EAST && (safeMove(travelerIndex, EAST)) &&
			(gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1] < gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col]))
		{

			if (gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1] < bestMoveVal)
			{
				bestMove = EAST;
				bestMoveVal = gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1];
				if (travelerList[travelerIndex].wouldStall)
				{
					travelerList[travelerIndex].wouldStall = false;
					travelerList[travelerIndex].stall = true;
				}
				else
				{
					travelerList[travelerIndex].stall = false;
				}
			}
			else if (gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1] == bestMoveVal)
			{
				if (!travelerList[travelerIndex].wouldStall && travelerList[travelerIndex].stall)
				{
					travelerList[travelerIndex].stall = false;
					bestMove = EAST;
					bestMoveVal = gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col - 1];
				}
			}
			travelerList[travelerIndex].wouldStall = false;
		}

		if (travelerList[travelerIndex].segmentList[0].dir != SOUTH && (safeMove(travelerIndex, SOUTH)) &&
			(gridPath[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col] < gridPath[travelerList[travelerIndex].segmentList[0].row][travelerList[travelerIndex].segmentList[0].col]))
		{

			if (gridPath[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col] < bestMoveVal)
			{
				bestMove = SOUTH;
				bestMoveVal = gridPath[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col];
				if (travelerList[travelerIndex].wouldStall)
				{
					travelerList[travelerIndex].wouldStall = false;
					travelerList[travelerIndex].stall = true;
				}
				else
				{
					travelerList[travelerIndex].stall = false;
				}
			}
			else if (gridPath[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col] == bestMoveVal)
			{
				if (!travelerList[travelerIndex].wouldStall && travelerList[travelerIndex].stall)
				{
					travelerList[travelerIndex].stall = false;
					bestMove = SOUTH;
					bestMoveVal = gridPath[travelerList[travelerIndex].segmentList[0].row - 1][travelerList[travelerIndex].segmentList[0].col];
				}
			}
			travelerList[travelerIndex].wouldStall = false;
		}

		if (bestMoveVal != 99999)
		{
			if (travelerList[travelerIndex].stall == false)
			{
				travelerList[travelerIndex].moveCount = 0;
				travel(travelerIndex, bestMove);
			}
			else
			{
				travelerList[travelerIndex].moveCount++;
				if (travelerList[travelerIndex].moveCount > maxStall)
				{
					if (safeMove(travelerIndex, travelerList[travelerIndex].segmentList[0].dir))
					{
						travel(travelerIndex, travelerList[travelerIndex].segmentList[0].dir);
					}
					else if (safeMove(travelerIndex, WEST))
					{
						travel(travelerIndex, WEST);
					}
					else if (safeMove(travelerIndex, NORTH))
					{
						travel(travelerIndex, NORTH);
					}
					else if (safeMove(travelerIndex, EAST))
					{
						travel(travelerIndex, EAST);
					}
					else if (safeMove(travelerIndex, SOUTH))
					{
						travel(travelerIndex, SOUTH);
					}
				}
			}
		}
		else
		{
			if (travelerList[travelerIndex].moveCount > maxStall)
			{
				if (safeMove(travelerIndex, travelerList[travelerIndex].segmentList[0].dir))
				{
					travel(travelerIndex, travelerList[travelerIndex].segmentList[0].dir);
				}
				else if (safeMove(travelerIndex, WEST))
				{
					travel(travelerIndex, WEST);
				}
				else if (safeMove(travelerIndex, NORTH))
				{
					travel(travelerIndex, NORTH);
				}
				else if (safeMove(travelerIndex, EAST))
				{
					travel(travelerIndex, EAST);
				}
				else if (safeMove(travelerIndex, SOUTH))
				{
					travel(travelerIndex, SOUTH);
				}
			}
		}
	}
	else
	{
		// this means that the traveler is exiting. in this case, the direction doesnt matter.
		travel(travelerIndex, NORTH);
	}

	return;
}

// used to make the pathfinding map. evaluates the a gridsquare based on the neighbor 
// that borders it first, then adds it to the queue.
void evalThenQ(int x, int y, int distance)
{

	if (gridPath[x][y] == 0)
	{
		gridPath[x][y] = distance + 1;
		GridPosition temp;
		temp.row = x;
		temp.col = y;
		pathQ.push(temp);
	}

	return;
}

// gives values to the neighbors of the given tile, then adds them to the queue.
// this limits the queue to the tiles on the border of the pathfinding grid
void addNeighbors(int anX, int anY)
{

	//	make sure the tile isnt out of bounds, nor is it the exit, which will be 1

	if (anX + 1 >= 0 && anY >= 0 && anX + 1 < (int)numCols && anY < (int)numRows)
	{
		evalThenQ(anX + 1, anY, gridPath[anX][anY]);
	}

	if (anX - 1 >= 0 && anY >= 0 && anX - 1 < (int)numCols && anY < (int)numRows)
	{
		evalThenQ(anX - 1, anY, gridPath[anX][anY]);
	}

	if (anX >= 0 && anY + 1 >= 0 && anX < (int)numCols && anY + 1 < (int)numRows)
	{
		evalThenQ(anX, anY + 1, gridPath[anX][anY]);
	}

	if (anX >= 0 && anY - 1 >= 0 && anX < (int)numCols && anY - 1 < (int)numRows)
	{
		evalThenQ(anX, anY - 1, gridPath[anX][anY]);
	}

	return;
}

// makes the pathfinding grid, starts at the exit and works out.
void allRoadsLeadToRoam()
{
	// adds the exit to the queue to jumpstart it
	evalThenQ(exitPos.row, exitPos.col, 0);
	addNeighbors(exitPos.row, exitPos.col);

	// keeps adding until all tiles connected to the exit have a value
	while (!pathQ.empty())
	{
		GridPosition next = pathQ.front();
		addNeighbors(next.row, next.col);
		pathQ.pop();
	}

	return;
}

void initializeApplication(void)
{
	//	Initialize some random generators
	rowGenerator = uniform_int_distribution<unsigned int>(0, numRows - 1);
	colGenerator = uniform_int_distribution<unsigned int>(0, numCols - 1);

	//	Allocate the grid
	grid = new SquareType *[numRows];
	gridPath = new int *[numRows];

	for (unsigned int i = 0; i < numRows; i++)
	{
		grid[i] = new SquareType[numCols];
		gridPath[i] = new int[numCols];

		for (unsigned int j = 0; j < numCols; j++)
		{
			grid[i][j] = FREE_SQUARE;
			gridPath[i][j] = 0;
		}
	}

	message = new char *[MAX_NUM_MESSAGES];
	for (unsigned int k = 0; k < MAX_NUM_MESSAGES; k++)
		message[k] = new char[MAX_LENGTH_MESSAGE + 1];

	//---------------------------------------------------------------
	//	All the code below to be replaced/removed
	//	I initialize the grid's pixels to have something to look at
	//---------------------------------------------------------------
	//	Yes, I am using the C random generator after ranting in class that the C random
	//	generator was junk.  Here I am not using it to produce "serious" data (as in a
	//	real simulation), only wall/partition location and some color
	std::srand((unsigned int)time(NULL));

	//	generate a random exit
	exitPos = getNewFreePosition();
	grid[exitPos.row][exitPos.col] = EXIT;
	gridPath[exitPos.row][exitPos.col] = 1;
	//	Generate walls and partitions
	generateWalls();
	generatePartitions();

	//	Initialize traveler info structs
	//	You will probably need to replace/complete this as you add thread-related data
	float **travelerColor = createTravelerColors(numTravelers);
	for (unsigned int k = 0; k < numTravelers; k++)
	{
		GridPosition pos = getNewFreePosition();
		//	Note that treating an enum as a sort of integer is increasingly
		//	frowned upon, as C++ versions progress
		Direction dir = static_cast<Direction>(segmentDirectionGenerator(engine));

		TravelerSegment seg = {pos.row, pos.col, dir};
		Traveler traveler;
		traveler.segmentList.push_back(seg);
		grid[pos.row][pos.col] = TRAVELER;

		//	I add 0-n segments to my travelers
		unsigned int numAddSegments = segmentNumberGenerator(engine);
		TravelerSegment currSeg = traveler.segmentList[0];
		bool canAddSegment = true;
		cout << "Traveler " << k << " at (row=" << pos.row << ", col=" << pos.col << "), direction: " << dirStr(dir) << ", with up to " << numAddSegments << " additional segments" << endl;
		cout << "\t";

		for (unsigned int s = 0; s < numAddSegments && canAddSegment; s++)
		{
			TravelerSegment newSeg = newTravelerSegment(currSeg, canAddSegment);
			if (canAddSegment)
			{
				traveler.segmentList.push_back(newSeg);
				currSeg = newSeg;
				cout << dirStr(newSeg.dir) << "  ";
			}
		}
		cout << endl;

		for (unsigned int c = 0; c < 4; c++)
			traveler.rgba[c] = travelerColor[k][c];

		travelerList.push_back(traveler);
	}
	allRoadsLeadToRoam();
	//	free array of colors
	for (unsigned int k = 0; k < numTravelers; k++)
		delete[] travelerColor[k];
	delete[] travelerColor;
}

//------------------------------------------------------
#if 0
#pragma mark -
#pragma mark Generation Helper Functions
#endif
//------------------------------------------------------

GridPosition getNewFreePosition(void)
{
	GridPosition pos;

	bool noGoodPos = true;
	while (noGoodPos)
	{
		unsigned int row = rowGenerator(engine);
		unsigned int col = colGenerator(engine);
		if (grid[row][col] == FREE_SQUARE)
		{
			pos.row = row;
			pos.col = col;
			noGoodPos = false;
		}
	}
	return pos;
}

Direction newDirection(Direction forbiddenDir)
{
	bool noDir = true;

	Direction dir = NUM_DIRECTIONS;
	while (noDir)
	{
		dir = static_cast<Direction>(segmentDirectionGenerator(engine));
		noDir = (dir == forbiddenDir);
	}
	return dir;
}

TravelerSegment newTravelerSegment(const TravelerSegment &currentSeg, bool &canAdd)
{
	TravelerSegment newSeg;
	switch (currentSeg.dir)
	{
	case NORTH:
		if (currentSeg.row < numRows - 1 &&
			grid[currentSeg.row + 1][currentSeg.col] == FREE_SQUARE)
		{
			newSeg.row = currentSeg.row + 1;
			newSeg.col = currentSeg.col;
			newSeg.dir = newDirection(SOUTH);
			grid[newSeg.row][newSeg.col] = TRAVELER;
			canAdd = true;
		}
		//	no more segment
		else
			canAdd = false;
		break;

	case SOUTH:
		if (currentSeg.row > 0 &&
			grid[currentSeg.row - 1][currentSeg.col] == FREE_SQUARE)
		{
			newSeg.row = currentSeg.row - 1;
			newSeg.col = currentSeg.col;
			newSeg.dir = newDirection(NORTH);
			grid[newSeg.row][newSeg.col] = TRAVELER;
			canAdd = true;
		}
		//	no more segment
		else
			canAdd = false;
		break;

	case WEST:
		if (currentSeg.col < numCols - 1 &&
			grid[currentSeg.row][currentSeg.col + 1] == FREE_SQUARE)
		{
			newSeg.row = currentSeg.row;
			newSeg.col = currentSeg.col + 1;
			newSeg.dir = newDirection(EAST);
			grid[newSeg.row][newSeg.col] = TRAVELER;
			canAdd = true;
		}
		//	no more segment
		else
			canAdd = false;
		break;

	case EAST:
		if (currentSeg.col > 0 &&
			grid[currentSeg.row][currentSeg.col - 1] == FREE_SQUARE)
		{
			newSeg.row = currentSeg.row;
			newSeg.col = currentSeg.col - 1;
			newSeg.dir = newDirection(WEST);
			grid[newSeg.row][newSeg.col] = TRAVELER;
			canAdd = true;
		}
		//	no more segment
		else
			canAdd = false;
		break;

	default:
		canAdd = false;
	}

	return newSeg;
}

void generateWalls(void)
{
	const unsigned int NUM_WALLS = (numCols + numRows) / 4;

	//	I decide that a wall length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_WALL_LENGTH = 3;
	const unsigned int MAX_HORIZ_WALL_LENGTH = numCols / 3;
	const unsigned int MAX_VERT_WALL_LENGTH = numRows / 3;
	const unsigned int MAX_NUM_TRIES = 20;

	bool goodWall = true;

	//	Generate the vertical walls
	for (unsigned int w = 0; w < NUM_WALLS; w++)
	{
		goodWall = false;

		//	Case of a vertical wall
		if (headsOrTails(engine))
		{
			//	I try a few times before giving up
			for (unsigned int k = 0; k < MAX_NUM_TRIES && !goodWall; k++)
			{
				//	let's be hopeful
				goodWall = true;

				//	select a column index
				unsigned int HSP = numCols / (NUM_WALLS / 2 + 1);
				unsigned int col = (1 + unsignedNumberGenerator(engine) % (NUM_WALLS / 2 - 1)) * HSP;
				unsigned int length = MIN_WALL_LENGTH + unsignedNumberGenerator(engine) % (MAX_VERT_WALL_LENGTH - MIN_WALL_LENGTH + 1);

				//	now a random start row
				unsigned int startRow = unsignedNumberGenerator(engine) % (numRows - length);
				for (unsigned int row = startRow, i = 0; i < length && goodWall; i++, row++)
				{
					if (grid[row][col] != FREE_SQUARE)
						goodWall = false;
				}

				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int row = startRow, i = 0; i < length && goodWall; i++, row++)
					{
						grid[row][col] = WALL;
						gridPath[row][col] = -1;
					}
				}
			}
		}
		// case of a horizontal wall
		else
		{
			goodWall = false;

			//	I try a few times before giving up
			for (unsigned int k = 0; k < MAX_NUM_TRIES && !goodWall; k++)
			{
				//	let's be hopeful
				goodWall = true;

				//	select a column index
				unsigned int VSP = numRows / (NUM_WALLS / 2 + 1);
				unsigned int row = (1 + unsignedNumberGenerator(engine) % (NUM_WALLS / 2 - 1)) * VSP;
				unsigned int length = MIN_WALL_LENGTH + unsignedNumberGenerator(engine) % (MAX_HORIZ_WALL_LENGTH - MIN_WALL_LENGTH + 1);

				//	now a random start row
				unsigned int startCol = unsignedNumberGenerator(engine) % (numCols - length);
				for (unsigned int col = startCol, i = 0; i < length && goodWall; i++, col++)
				{
					if (grid[row][col] != FREE_SQUARE)
						goodWall = false;
				}

				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int col = startCol, i = 0; i < length && goodWall; i++, col++)
					{
						grid[row][col] = WALL;
						gridPath[row][col] = -1;
					}
				}
			}
		}
	}
}

void generatePartitions(void)
{
	const unsigned int NUM_PARTS = (numCols + numRows) / 4;

	//	I decide that a partition length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_PARTITION_LENGTH = 3;
	const unsigned int MAX_HORIZ_PART_LENGTH = numCols / 3;
	const unsigned int MAX_VERT_PART_LENGTH = numRows / 3;
	const unsigned int MAX_NUM_TRIES = 20;

	bool goodPart = true;

	for (unsigned int w = 0; w < NUM_PARTS; w++)
	{
		goodPart = false;

		//	Case of a vertical partition
		if (headsOrTails(engine))
		{
			//	I try a few times before giving up
			for (unsigned int k = 0; k < MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;

				//	select a column index
				unsigned int HSP = numCols / (NUM_PARTS / 2 + 1);
				unsigned int col = (1 + unsignedNumberGenerator(engine) % (NUM_PARTS / 2 - 2)) * HSP + HSP / 2;
				unsigned int length = MIN_PARTITION_LENGTH + unsignedNumberGenerator(engine) % (MAX_VERT_PART_LENGTH - MIN_PARTITION_LENGTH + 1);

				//	now a random start row
				unsigned int startRow = unsignedNumberGenerator(engine) % (numRows - length);
				for (unsigned int row = startRow, i = 0; i < length && goodPart; i++, row++)
				{
					if (grid[row][col] != FREE_SQUARE)
						goodPart = false;
				}

				//	if the partition is possible,
				if (goodPart)
				{
					//	add it to the grid and to the partition list
					SlidingPartition part;
					part.isVertical = true;
					for (unsigned int row = startRow, i = 0; i < length && goodPart; i++, row++)
					{
						grid[row][col] = VERTICAL_PARTITION;
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);

						gridPath[row][col] = -1;
					}
				}
			}
		}
		// case of a horizontal partition
		else
		{
			goodPart = false;

			//	I try a few times before giving up
			for (unsigned int k = 0; k < MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;

				//	select a column index
				unsigned int VSP = numRows / (NUM_PARTS / 2 + 1);
				unsigned int row = (1 + unsignedNumberGenerator(engine) % (NUM_PARTS / 2 - 2)) * VSP + VSP / 2;
				unsigned int length = MIN_PARTITION_LENGTH + unsignedNumberGenerator(engine) % (MAX_HORIZ_PART_LENGTH - MIN_PARTITION_LENGTH + 1);

				//	now a random start row
				unsigned int startCol = unsignedNumberGenerator(engine) % (numCols - length);
				for (unsigned int col = startCol, i = 0; i < length && goodPart; i++, col++)
				{
					if (grid[row][col] != FREE_SQUARE)
						goodPart = false;
				}

				//	if the wall first, add it to the grid and build SlidingPartition object
				if (goodPart)
				{
					SlidingPartition part;
					part.isVertical = false;
					for (unsigned int col = startCol, i = 0; i < length && goodPart; i++, col++)
					{
						grid[row][col] = HORIZONTAL_PARTITION;
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);

						gridPath[row][col] = -1;
					}
				}
			}
		}
	}
}
