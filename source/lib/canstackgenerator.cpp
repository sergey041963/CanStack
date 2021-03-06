#include "canstackgenerator.h"


Bool CanStackGenerator::InitStack(const StackParameters &params)
{
	// Init random number generator (we have to do this always to ensure reproducible random results!)
	_random.Init(params._randomSeed);

	// If new params are the same as the previous ones, don't do anything else
	if (params == _params)
		return true;
	
	// Default member values
	_params = StackParameters();
	_initialized = false;
	
	// Cancel if nonsense baseCount
	if (params._baseCount < 1)
		return false;
	
	// Store parameters internally
	_params = params;
	
	// Make sure the stack array and its row arrays are of correct size
	if (!ResizeStack(_params._baseCount, _params._rowCount))
		return false;
	
	// Success, we made it!
	_initialized = true;
	return _initialized;
}


Bool CanStackGenerator::GenerateStack()
{
	if (!_initialized)
		return false;
	
	// Some values
	Float distance(0.0);			// Distance between items in a normal row
	Float relDistance(0.0);		// Relative distance between items in a row on a path spline
	Matrix splineMg;
	
	// If spline is used, use length of spline as baseLength
	if (_params._basePath)
	{
		splineMg = _params._basePath->GetMg();
		
		// Allocate SplineLengthData
		if (!_splineLengthData)
		{
			_splineLengthData.Set(SplineLengthData::Alloc());
			if (!_splineLengthData)
				return false;
		}
		
		// Initialize SplineLengthData
		if (!_splineLengthData->Init(_params._basePath))
			return false;
		
		// Calculate relative distance between clones on spline
		relDistance = 1.0 / (Float)(_params._baseCount - 1);
	}
	else
	{
		// Create distance between clones
		distance = _params._baseLength / (Float)_params._baseCount;
	}

	// Iterate stack rows
	Int32 rowIndex = 0;
	for (StackRowArray::Iterator row = _array.Begin(); row != _array.End(); ++row, rowIndex++)
	{
		// Iterate items in row
		// Create positions for current row
		Int32 itemIndex = 0;
		for (StackItemArray::Iterator item = row->Begin(); item != row->End(); ++item, itemIndex++)
		{
			// Compute rotation matrix & set to item
			Matrix rotMatrix = HPBToMatrix(Vector(_random.Get11() * _params._randomRot, 0.0, 0.0), ROTATIONORDER_HPB);
			item->mg = rotMatrix;
			
			// Compute matrix offset
			if (_params._basePath)
			{
				// Calculate item's relative offset on the spline
				Float relOffset = _splineLengthData->UniformToNatural((relDistance * itemIndex) + (relDistance * 0.5 * rowIndex));
				
				// Get values we need to compute position of item
				Vector splinePosition = _params._basePath->GetSplinePoint(relOffset);			// Position of point on spline
				Vector splineTangent = _params._basePath->GetSplineTangent(relOffset);		// Tangent of point on spline (Z axis for item)
				Vector splineCrossTangent = Cross(splineTangent, Vector(0.0, 1.0, 0.0));	// Cross product of tangent and Y axis (X axis for item)

				// Calculate position along spline
				item->mg.off = splinePosition;
				item->mg.off.y += _params._rowHeight * rowIndex;	// Offset to Y direction
				item->mg.off += splineCrossTangent * _random.Get11() * _params._randomOffX;	// Randomly offset to the sides of the spline
				item->mg.off += splineTangent * _random.Get11() * _params._randomOffZ;	// Randomly offset along spline
				
				// Transform into global space
				item->mg = splineMg * item->mg;
			}
			else
			{
				// Calculate item's position
				item->mg.off = Vector(_random.Get11() * _params._randomOffX, _params._rowHeight * rowIndex, distance * itemIndex + distance * rowIndex * 0.5 + _random.Get11() * _params._randomOffZ);
			}
		}
	}
	
	return true;
}


BaseObject *CanStackGenerator::BuildStackGeometry(BaseObject *originalObject, const Matrix &mg, Bool useRenderInstances)
{
	// Create parent object
	AutoAlloc<BaseObject> resultParent(Onull);
	if (!resultParent)
		return nullptr;
	
	BaseObject* objectToClone = nullptr;
	
	// We'll clone either the original child object, or - if child is a render instance - the object that's linked
	if (originalObject->GetType() == Oinstance && originalObject->GetDataInstance()->GetBool(INSTANCEOBJECT_RENDERINSTANCE))
		objectToClone = originalObject->GetDataInstance()->GetObjectLink(INSTANCEOBJECT_LINK, originalObject->GetDocument());
	else
		objectToClone = originalObject;
	
	// Cancel if nothing to clone
	if (!objectToClone)
		return nullptr;
	
	// Store object name (plus an extra space)
	String originalName = objectToClone->GetName() + " ";
	
	// Calculate inversion of 'mg' (needed to transform item matrix from global space to generator's local space if path spline is used)
	Matrix invertedMg = ~mg;
	
	// Counter for total number of created new items
	Int32 newItemCount = 0;
	
	// Store pointer to first created object (if using render instances, all successive instances must link to the first object)
	BaseObject *firstItem = nullptr;

	// Iterate rows in stack
	for (StackRowArray::Iterator row = _array.Begin(); row != _array.End(); ++row)
	{
		// Iterate items in row
		for (StackItemArray::Iterator item = row->Begin(); item != row->End(); ++item)
		{
			BaseObject *newItem = nullptr;
			
			// First object always has to be a clone, even if we use render instances
			if (useRenderInstances && newItemCount > 0)
			{
				// Create render instance of original object
				newItem = BaseObject::Alloc(Oinstance);
				if (!newItem)
					return nullptr;
				
				// Set instance properties
				BaseContainer *newItemData = newItem->GetDataInstance();
				newItemData->SetLink(INSTANCEOBJECT_LINK, firstItem);
				newItemData->SetBool(INSTANCEOBJECT_RENDERINSTANCE, true);
			}
			else
			{
				// Create clone of original object
				newItem = static_cast<BaseObject*>(objectToClone->GetClone(COPYFLAGS_0, nullptr));
				if (!newItem)
					return nullptr;
				
				// Store pointer to clone (needed in case we use render instances)
				firstItem = newItem;
			}
			
			// Increase counter
			newItemCount++;
			
			// Set clone position according to item in stack data
			if (_params._basePath)
				newItem->SetMg(invertedMg * item->mg);	// Transform matrix from global to local generator space
			else
				newItem->SetMl(item->mg);								// Simply set local matrix
			
			// Insert clone as last child under parent Null
			newItem->InsertUnderLast(resultParent);
		}
	}
	
	// Return parent Null and give up ownership
	return resultParent.Release();
}


Bool CanStackGenerator::ResizeStack(Int32 baseCount, Int32 rowCount)
{
	// Resize stack
	if (!_array.Resize(Min(baseCount, rowCount)))
		return false;
	
	// Resize rows in stack
	Int32 rowIndex = 0;
	for (StackRowArray::Iterator row = _array.Begin(); row != _array.End() && rowIndex < rowCount; ++row, rowIndex++)
	{
		// Each row is 1 smaller than its predecessor
		if (!row->Resize(_params._baseCount - rowIndex))
			return false;
	}
	
	return true;
}
