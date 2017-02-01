#ifndef STACKGENERATOR_H__
#define STACKGENERATOR_H__


#include "c4d.h"
#include "lib_splinehelp.h"


/*
	Just for the records: This is what stacks look like:
 
        X
       X X          X
      X X X        X X        X
     X X X X      X X X      X X      X
 		X X X X X    X X X X    X X X    X X    X
 
 		    5           4         3       2     1
        =           =         =       =     =
			 15          10         6       3     1
 
 	Notice:
 	- Maximum rowCount is always == baseCount
 	- itemCount per Row is always itemCount of previous row - 1
  - Total number of items is GaussSum(baseCount)
 */


/// MatrixArray is BaseArray of Matrix. It holds a matrix for each item in a row
typedef maxon::BaseArray<Matrix> MatrixArray;


/// StackRowArray is a BaseArray of MatrixArray. It holds multiple rows that form the stack.
typedef maxon::BaseArray<MatrixArray> StackRowArray;


/// Structure that holds the parameters for a stack
struct StackParameters
{
	Int32		_baseCount;					///< How many items the base (lowest) row should have
	Float		_baseLength;				///< The length of the stack (if no path spline used)
	Int32		_rowCount;					///< How many rows to generate maximum
	Float 	_rowHeight;					///< Height of rows / items
	UInt32	_randomSeed;				///< Seed for random number generation
	Float		_randomPos;					///< Random position
	Float		_randomRot;					///< Random rotation
	SplineObject	*_basePath;		///< Pointer to path spline
	
	/// Default constructor
	StackParameters() : _baseCount(0), _baseLength(0.0), _rowCount(0), _rowHeight(0.0), _randomSeed(0), _randomPos(0.0), _randomRot(0.0), _basePath(nullptr)
	{ }
	
	/// Copy constructor
	StackParameters(const StackParameters &src) : _baseCount(src._baseCount), _baseLength(src._baseLength), _rowCount(src._rowCount), _rowHeight(src._rowHeight), _randomSeed(src._randomSeed), _randomPos(src._randomPos), _randomRot(src._randomRot), _basePath(src._basePath)
	{ }
	
	/// Checks if two StackParameters objects are equal.
	/// @param[in] x1									The first StackParameters object
	/// @param[in] x2									The second StackParameters object
	/// @return												True if both are equal, otherwise false.
	friend Bool operator == (const StackParameters& x1, const StackParameters& x2)
	{
		return (x1._baseCount == x2._baseCount) && (x1._baseLength == x2._baseLength) && (x1._rowCount == x2._rowCount) && (x1._rowHeight == x2._rowHeight) && (x1._randomSeed == x2._randomSeed) && (x1._randomPos == x2._randomPos) && (x1._randomRot == x2._randomRot);
	}
};


/// A class that builds stacks
class CanStackGenerator
{
public:
	/// Copies parameters, initializes the stack data arrays and internal structures
	Bool InitStack(const StackParameters &params);

	/// Fills the arrays with data, according to the StackParameters passed in InitStack()
	Bool GenerateStack();
	
	/// Returns
	BaseObject *BuildStackGeometry(BaseObject *originalObject, const Matrix &mg);
	
private:
	/// Resizes the internal stack arrays
	Bool ResizeStack(Int32 baseCount, Int32 rowCount);
	
	/// SplineHelp object required when using a spline as base path
	/// Will be allocated only when needed, and freed automatically
	AutoFree<SplineLengthData> _splineLengthData;
	
	/// This array will hold all the generated stack data
	StackRowArray _array;
	
	/// The parameters for the stack
	StackParameters _params;
	
	/// Random number generator
	Random _random;
	
	/// Set to true after successful initialization
	Bool _initialized;
};


#endif // STACKGENERATOR_H__