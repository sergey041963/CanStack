#include "c4d.h"
#include "canstackgenerator.h"
#include "objecthelpers.h"
#include "c4d_symbols.h"
#include "ostack.h"
#include "main.h"


const Int32 ID_STACK = 1038758;	///< Unique ID obtained from www.plugincafe.com


/// Stack Object class declaration
class StackObject : public ObjectData
{
	INSTANCEOF(StackObject, ObjectData)
	
public:
	virtual Bool Init(GeListNode *node);
	virtual Bool Message(GeListNode *node, Int32 type, void *t_data);
	virtual Bool GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc);
	virtual Bool CopyTo(NodeData *dest, GeListNode *snode, GeListNode *dnode, COPYFLAGS flags, AliasTrans *trn);

	virtual BaseObject* GetVirtualObjects(BaseObject *op, HierarchyHelp *hh);

	static NodeData* Alloc()
	{
		return NewObjClear(StackObject);
	}
	
	
	StackObject() : _lastPathSpline(nullptr)
	{ }
	
private:
	CanStackGenerator	_stackGenerator;	///< The stack generator
	BaseObject*				_lastPathSpline;	///< Pointer to the last used path spline object (used for comparison during dirty detection)
};


// Initialize settings
Bool StackObject::Init(GeListNode *node)
{
	// Good practice: Check for nullptr
	if (!node)
		return false;
	
	// Get object's BaseContainer
	BaseObject *op	= static_cast<BaseObject*>(node);
	BaseContainer *data = op->GetDataInstance();

	// Set default attributes
	data->SetFloat(STACK_BASE_LENGTH, 100.0);
	data->SetInt32(STACK_BASE_COUNT, 3);
	data->SetInt32(STACK_ROWS_COUNT, 3);
	data->SetFloat(STACK_ROWS_HEIGHT, 20.0);
	data->SetBool(STACK_RENDERINSTANCES, true);
	data->SetUInt32(STACK_RANDOM_SEED, 12345);
	data->SetFloat(STACK_RANDOM_ROT, 0.0);
	data->SetFloat(STACK_RANDOM_OFF_X, 0.0);
	data->SetFloat(STACK_RANDOM_OFF_Z, 0.0);

	// Return super
	return SUPER::Init(node);
}


// Catch messages
Bool StackObject::Message(GeListNode *node, Int32 type, void *data)
{
	// Good practice: Check for nullptr
	if (!node)
		return false;
	
	switch (type)
	{
		// Description validation: Make sure STACK_ROWS_COUNT doesn't get higher than STACK_BASE_COUNT
		case MSG_DESCRIPTION_VALIDATE:
		{
			BaseContainer* bc = static_cast<BaseObject*>(node)->GetDataInstance();
			
			// Limit row count to base count
			Int32 baseCount = bc->GetInt32(STACK_BASE_COUNT, 0);
			Int32 maxRows = bc->GetInt32(STACK_ROWS_COUNT, 0);
			bc->SetInt32(STACK_ROWS_COUNT, Min(maxRows, baseCount));
			
			break;
		}
			
		// Command button pressed
		case MSG_DESCRIPTION_COMMAND:
		{
			// Good practice: Check for nullptr when valid pointer required
			if (!data)
				return false;
			
			// Get message data
			DescriptionCommand *dc = (DescriptionCommand*)data;
			
			// Fit STACK_ROWS_HEIGHT to height of child object
			if (dc->id == STACK_CMD_FITHEIGHT)
			{
				// Get child object
				BaseObject *child = static_cast<BaseObject*>(node->GetDown());
				if (child)
				{
					// Get child's bounding box radius
					Vector rad = child->GetRad();
					
					// If radius invalid
					if (rad.IsZero())
					{
						// Get a temporary CSTO clone
						Int32 objectType = 0;
						AutoFree<BaseObject> pointObject;
						pointObject.Set(GetCurrentStateToObject(child, objectType));
						
						// Calculate radius ourselves
						rad = CalculateHierarchyBoundingBox(pointObject).GetRad();
					}

					// If radius is valid
					if (rad.IsNotZero())
					{
						// Get Container
						BaseContainer* bc = static_cast<BaseObject*>(node)->GetDataInstance();
						
						// Set STACK_ROWS_HEIGHT to radius*2
						bc->SetFloat(STACK_ROWS_HEIGHT, rad.y * 2.0);
					}
				}
			}
			break;
		}
			
	}
	
	// Return Super
	return SUPER::Message(node, type, data);
}


// Grey out unused attributes
Bool StackObject::GetDEnabling(GeListNode *node, const DescID &id, const GeData &t_data, DESCFLAGS_ENABLE flags, const BaseContainer *itemdesc)
{
	// Good practice: Check for nullptr
	if (!node)
		return false;
	
	// Get object's BaseContainer
	BaseObject *op = static_cast<BaseObject*>(node);
	BaseContainer *bc = op->GetDataInstance();
	
	switch (id[0].id)
	{
		// Disable length attribute is a path spline is used
		case STACK_BASE_LENGTH:
			return !bc->GetObjectLink(STACK_BASE_PATH, op->GetDocument());
	}
	
	// Return super
	return SUPER::GetDEnabling(node, id, t_data, flags, itemdesc);
}


// Copy internal data to another StackObject
Bool StackObject::CopyTo(NodeData *dest, GeListNode *snode, GeListNode *dnode, COPYFLAGS flags, AliasTrans *trn)
{
	// Good practice: Check for nullptr
	if (!dest)
		return false;
	
	// Cast destination node to correct class
	StackObject *destStack = static_cast<StackObject*>(dest);
	
	// Copy data
	destStack->_lastPathSpline = _lastPathSpline;
	
	// Return SUPER
	return SUPER::CopyTo(dest, snode, dnode, flags, trn);
}


// Generate stack
BaseObject* StackObject::GetVirtualObjects(BaseObject *op, HierarchyHelp *hh)
{
	// Good practice: Check for nullptr
	if (!op || !hh)
		return nullptr;
	
	// Get container
	BaseContainer *bc = op->GetDataInstance();
	
	// Get document
	BaseDocument *doc = op->GetDocument();
	if (!doc)
		return nullptr;
	
	// Get child object for cloning
	BaseObject *child = op->GetDown();
	if (!child)
		return nullptr;

	// Set dependencies for dirty detection
	op->NewDependenceList();
	BaseObject *pathSpline = bc->GetObjectLink(STACK_BASE_PATH, doc);
	if (pathSpline)
		op->AddDependence(hh, pathSpline);
	
	// Check if we need to recalculate
	Bool dirty = op->CheckCache(hh) || op->IsDirty(DIRTYFLAGS_DATA) || IsDirtyChildren(op, DIRTYFLAGS_DATA|DIRTYFLAGS_CACHE|DIRTYFLAGS_MATRIX) || (pathSpline != _lastPathSpline) || !op->CompareDependenceList();
	
	// Return cache if nothing important has changed
	if (!dirty)
	{
		// Hide child objects, return previously generated cache
		TouchAllChildren(op);
		return op->GetCache(hh);
	}
	
	// Get stack parameters from container
	StackParameters params(*bc, *doc);
	
	// Initialize stack
	if (!_stackGenerator.InitStack(params))
		return nullptr;
	
	// Generate stack item
	if (!_stackGenerator.GenerateStack())
		return nullptr;
	
	// Build geometry
	BaseObject *result = _stackGenerator.BuildStackGeometry(op->GetDown(), op->GetMg(), bc->GetBool(STACK_RENDERINSTANCES));
	if (!result)
		return nullptr;
	
	// Hide all child objects
	TouchAllChildren(op);
	
	// Update internal values for later dirty detection
	_lastPathSpline = pathSpline;
	
	// Name parent result object
	result->SetName(GeLoadString(IDS_STACK));
	
	// Indicate that result has changed
	result->Message(MSG_UPDATE);
	
	// Return result
	return result;
}


//----------------------------------------------------------------------------------------
///	Plugin help support callback. Can be used to display context sensitive help when the
/// user selects "Show Help" for an object or attribute. <B>Only return true for your own
/// object types</B>. All names are always uppercase.
/// @param[in] opType							Object type name, for example "OATOM".
/// @param[in] baseType						Name of base object type that opType is derived from, usually the same as opType.
/// @param[in] group							Name of group in the attribute manager, for example "ID_OBJECTPROPERTIES".
/// @param[in] property						Name of the object property, for example "ATOMOBJECT_SINGLE";
/// @return												True if the plugin can display help for this request.
//----------------------------------------------------------------------------------------
static Bool CanStackHelpDelegate(const String &opType, const String &baseType, const String &group, const String &property)
{
	if (opType == "OSTACK")
	{
		Filename filePath = GeGetPluginPath() + "docs" + "index.html";
		
		GeExecuteFile(filePath.GetString());
		
		return true;
	}
	
	return false;
}


// Register object plugin and help delegate
Bool RegisterStackObject()
{
	if (!RegisterObjectPlugin(ID_STACK, GeLoadString(IDS_STACK), OBJECT_GENERATOR|OBJECT_INPUT, StackObject::Alloc, "Ostack", AutoBitmap("ostack.tif"), 0))
		return false;
	
	return RegisterPluginHelpDelegate(ID_STACK, CanStackHelpDelegate);
}
