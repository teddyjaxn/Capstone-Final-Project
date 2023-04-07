//
//  dataTypes.h
//  Final
//
//  Created by Jean-Yves Hervé on 2019-05-01.
//	Revised for Fall 2020, 2020-12-01

#ifndef DATAS_TYPES_H
#define DATAS_TYPES_H

#include <vector>
#include <string>

/**	Travel Direction data type.
 *	Note that if you define a variable
 *	Direction dir = whatever;
 *		you get the opposite Orientations from dir as (NUM_OrientationS - dir)
 *	you get left turn from dir as (dir + 1) % NUM_OrientationS
 */
typedef enum Direction
{
	NORTH = 0,
	WEST,
	SOUTH,
	EAST,
	//
	NUM_DIRECTIONS
} Direction;


/**	Grid square types for this simulation
 */
typedef enum SquareType
{
	FREE_SQUARE,
	EXIT,
	WALL,
	VERTICAL_PARTITION,
	HORIZONTAL_PARTITION,
	TRAVELER,
	//
	NUM_SQUARE_TYPES

} SquareType;

/**	Data type to store the position of *things* on the grid
 */
typedef struct GridPosition
{
	/**	row index
	 */
	unsigned int row;
	/** column index
	 */
	unsigned int col;

} GridPosition;

/**
 *	Data type to store the position and Direction of a traveler's segment
 */
typedef struct TravelerSegment
{
	/**	row index
	 */
	unsigned int row;
	/** column index
	 */
	unsigned int col;
	/**	One of four possible orientations
	 */
	Direction dir;

} TravelerSegment;


/**
 *	Data type for storing all information about a traveler
 *	Feel free to add anything you need.
 */
typedef struct Traveler
{

	/** The traveler's current number of moves without a new tail segment
	 */
	unsigned int tailCount = 0;

	/** Whether or not the traveler is in the exit
	 *  used for making them disappear on section at a time.
	 */
	bool exiting = false;

	/** Whether or not the traveler is rendered anymore
	 *  used for making them disappear once they finish exiting.
	 */
	bool render = true;

	/**  will be used to determine if the movement should be forced.
	 *   may be replaced in the future with a global indicator for whether a partition has been moved.
	 */ 
	unsigned int moveCount = 0;      

	// This means that is a traveler is blocked by a traveler segment (another traveler or its own tail) for too long, it will randomly move. 
	// This would ideally only happen when it is blocking istelf, or when it is blocked by a dead-end traveler.

	// used to see if a traveler is blocking the path. 
	// Used to stop travelers from exhausting themselves while waiting for the path to open.
	bool wouldStall = false;

	bool stall = false;

	/**	The traveler's index
	 */
	unsigned int index;
	
	/**	The color assigned to the traveler, in rgba format
	 */
	float rgba[4];
	
	/**	The list of segments that form the 'tail' of the traveler
	 */
	std::vector<TravelerSegment> segmentList;
	
} Traveler;

/**
 *	Data type to represent a sliding partition
 */
typedef struct SlidingPartition
{
	/*	vertical vs. horizontal partition
	 */
	bool isVertical;

	bool goingUp = true;
	
	/**	The blocks making up the partition, listed
	 *		top-to-bottom for a vertical list
	 *		left-to-right for a horizontal list
	 */
	std::vector<GridPosition> blockList;

} SlidingPartition;


/**	Ugly little function to return a direction as a string
*	@param dir the direction
*	@return the direction in readable string form
*/
std::string dirStr(const Direction& dir);

/**	Ugly little function to return a square type as a string
*	@param type the suqare type
*	@return the square type in readable string form
*/
std::string typeStr(const SquareType& type);



#endif //	DATAS_TYPES_H
