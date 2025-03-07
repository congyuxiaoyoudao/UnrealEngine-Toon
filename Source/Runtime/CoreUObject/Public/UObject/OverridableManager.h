// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/UObjectAnnotation.h"
#include "UObject/OverriddenPropertySet.h"
#include "UObject/PropertyBagRepository.h"

/*
 *************************************************************************************
 * Overridable serialization is experimental, not supported and use at your own risk *
 *************************************************************************************
 */

#if WITH_EDITORONLY_DATA

/**
 * Struct holding the shared ptr of the overridden properties
 */
struct FOverriddenPropertyAnnotation
{
	bool IsDefault() const
	{
		return !OverriddenProperties.IsValid();
	}

	TSharedPtr<FOverriddenPropertySet> OverriddenProperties;
};

/**
 * Global container of overridden object annotations
 */
class FOverriddenPropertyAnnotations : public FUObjectAnnotationSparse<FOverriddenPropertyAnnotation, true/*bAutoRemove*/>
{
public:
	bool IsEnabled(const UObject& Object)
	{
		return !GetAnnotation(&Object).IsDefault();
	}

	FOverriddenPropertySet* Find(const UObject& Object)
	{
		const UObjectBase* KeyObject = GetKeyObject(Object);
		FOverriddenPropertyAnnotation Annotation = GetAnnotation(KeyObject);
		return Annotation.OverriddenProperties.Get();
	}

	FOverriddenPropertySet& FindChecked(const UObject& Object)
	{
		const UObjectBase* KeyObject = GetKeyObject(Object);
		FOverriddenPropertyAnnotation Annotation = GetAnnotation(KeyObject);
		checkf(!Annotation.IsDefault(), TEXT("Caller is expecting the object to have overridable serialization enabled"));
		return *Annotation.OverriddenProperties.Get();
	}

	FOverriddenPropertySet& FindOrAdd(UObject& Object)
	{
		UObject* KeyObject = GetKeyObject(Object);
		FOverriddenPropertyAnnotation Annotation = GetAnnotation(KeyObject);
		if (Annotation.IsDefault())
		{
			Annotation.OverriddenProperties = MakeShared<FOverriddenPropertySet>(*KeyObject);
			AddAnnotation(&Object, Annotation);
		}

		return *Annotation.OverriddenProperties;
	}
private:
	static UObject* GetKeyObject(const UObject& Object)
	{
		const UObject* Found = nullptr;
#if WITH_EDITOR
		const UClass* Class = Object.GetClass();
		if (Class && !Class->HasAnyClassFlags(EClassFlags::CLASS_Native))
		{
			Found = UE::FPropertyBagRepository::Get().FindInstanceForDataObject(&Object);
		}
#endif
		return const_cast<UObject*>(Found ? Found : &Object);
	}
};

#endif // WITH_EDITORONLY_DATA

enum class EOverriddenState : uint8
{
	NoOverrides, // no on this object and any of its instanced subobjects
	HasOverrides, // has overrides in the object properties
	AllOverridden, // all properties are overridden for this object and its subobjects
	SubObjectsHasOverrides, // at least one of its subobjects has overrides
	Added, // This object was added
};


/*
 *************************************************************************************
 * Overridable serialization is experimental, not supported and use at your own risk *
 *************************************************************************************
 */
class FOverridableManager
{
public:
	/**
	 * @return the static instance managing the overridability */
	COREUOBJECT_API static FOverridableManager& Get();

	/**
	 * Lookup if the for the specified object has overridable serialization enabled
	 * @param Object to check against
	 * @return true if it uses the overridable serialization */
	 COREUOBJECT_API bool IsEnabled(const UObject& Object);

	/**
	 * Sets on the specified object to use overridable serialization
	 * @param Object to enable */
	COREUOBJECT_API void Enable(UObject& Object);

	/**
	 * Sets on the specified object to use overridable serialization
	 * @param Object to disable */
	COREUOBJECT_API void Disable(UObject& Object);

	/**
	 * Inherit if the specified object should enable overridable serialization. It inherits it from either its default object or its outer.
	 * @param Object to be inherited on
	 * @param DefaultData its default object */
	COREUOBJECT_API void InheritEnabledFrom(UObject& Object, const UObject* DefaultData);

	/**
	 * Return true if this object needs subobject template instantiation
	 * @param Object to be querying about */
	COREUOBJECT_API bool NeedSubObjectTemplateInstantiation(const UObject& Object);

	/**
	 * Retrieve the overridden properties for the specified object
	 * @param Object to fetch the overridden properties
	 * @return the overridden properties if the object have overridable serialization enabled */
	COREUOBJECT_API FOverriddenPropertySet* GetOverriddenProperties(UObject& Object);

	/**
	 * Retrieve the overridden properties for the specified object
	 * @param Object to fetch the overridden properties
	 * @return the overridden properties if the object have overridable serialization enabled */
	COREUOBJECT_API const FOverriddenPropertySet* GetOverriddenProperties(const UObject& Object);

	/**
	 * Set the override operation on this object and it will enable it if it wasn't already enabled
	 * @param Object to set the override operation on
	 * @param Operation the override operation to set on the object
	 * @param bNeedsSubobjectTemplateInstantiation set to true if it will need a sub object template instantiation during postload, false will not do it.
	 * @return the overridden properties of the object */
	COREUOBJECT_API FOverriddenPropertySet* SetOverriddenProperties(UObject& Object, EOverriddenPropertyOperation Operation, const bool bNeedsSubobjectTemplateInstantiation);

	/**
	 * Retrieve the overridden state for the specified object
	 * @param Object to fetch the overridden properties
	 * @return the overridden state if the object has overridable serialization enabled */
	COREUOBJECT_API EOverriddenState GetOverriddenState(UObject& Object);

	/**
	 * Override the entire object properties and all its instanced subobjects
	 * @param Object to override all it properties */
	COREUOBJECT_API void OverrideObject(UObject& Object);

	/**
	 * Override all the properties of the specified instanced subobject if it is really owned by the referencer
	 * @param Object referencing this subobject
	 * @param InstancedSubObject the sub object to override all its properties */
	COREUOBJECT_API void OverrideInstancedSubObject(UObject& Object, UObject& InstancedSubObject);

	/**
	 * Propagate override to all instanced sub object of the specified object
	 * @param Object to propagate from */
	COREUOBJECT_API void PropagateOverrideToInstancedSubObjects(UObject& Object);

	/**
	 * Clears all the overrides on the specified object
	 * @param Object to clear them on */
	COREUOBJECT_API void ClearOverrides(UObject& Object);

	/**
	 * Clear all overrides of the specified instanced subobject if it is really owned by the referencer
	 * @param Object referencing the subobject
	 * @param InstancedSubObject to clear overrides on */
	COREUOBJECT_API void ClearInstancedSubObjectOverrides(UObject& Object, UObject& InstancedSubObject);

	/**
	 * Propagate the clear overrides to all instanced suboject of the specified object
	 * @param Object*/
	COREUOBJECT_API void PropagateClearOverridesToInstancedSubObjects(UObject& Object);

	/**
	 * Override a specific property of an object (Helper methods to call Pre/PostOverride)
	 * Note: Supports object that does not have overridable serialization enabled
	 * @param Object owning the property
	 * @param PropertyPath leading to the property that is about to be overridden */
	COREUOBJECT_API void OverrideProperty(UObject& Object, const FPropertyVisitorPath& PropertyPath);

	/**
	 * Clears an overridden properties specified by the property chain
	 * @param Object owning the property to clear
	 * @param PropertyPath to the property to clear from the root of the specified object
	 * @return true if the property was successfully cleared. */
	bool ClearOverriddenProperty(UObject& Object, const FPropertyVisitorPath& PropertyPath)
	{
		return ClearOverriddenProperty(Object, PropertyPath.GetRootIterator());
	}

	/**
	 * To be called prior to override a property of the specified object
	 * Note: Supports object that does not have overridable serialization enabled
	 * @param Object owning the property
	 * @param PropertyPath leading to the property about to be overridden */
	COREUOBJECT_API void PreOverrideProperty(UObject& Object, const FPropertyVisitorPath& PropertyPath);

	/**
	 * To be called after the property of the specified object was overridden
	 * Note: Supports object that does not have overridable serialization enabled
	 * @param Object owning the property
	 * @param PropertyPath leading to the property that was overridden
 	 * @param ChangeType of the current operation */
	COREUOBJECT_API void PostOverrideProperty(UObject& Object, const FPropertyVisitorPath& PropertyPath, const EPropertyChangeType::Type ChangeType);

	/**
	 * Retrieve the overridable operation from the specified property path
	 * @param Object owning the property
	 * @param PropertyPath leading to the property the caller is interested in
	 * @param bOutInheritedOperation optional parameter to know if the operation returned was inherited from a parent property
	 * @return the current type of override operation on the property */
	EOverriddenPropertyOperation GetOverriddenPropertyOperation(UObject& Object, const FPropertyVisitorPath& PropertyPath, bool* bOutInheritedOperation = nullptr)
	{
		return GetOverriddenPropertyOperation(Object, PropertyPath.GetRootIterator(), bOutInheritedOperation);
	}

	/**
	 * Serializes the overridden properties of the specified object into the record
	 * @param Object to serialize the overridden property
	 * @param ObjectRecord the record to use for serialization */
	COREUOBJECT_API void SerializeOverriddenProperties(UObject& Object, FStructuredArchive::FRecord ObjectRecord);

	/**
	 * Override a specific property of an object (Helper methods to call Pre/PostOverride)
	 * Note: Supports object that does not have overridable serialization enabled
	 * @param Object owning the property
	 * @param PropertyEvent information about the type of change including any container item index
	 * @param PropertyChain leading to the property that is about to be overridden */
	void OverrideProperty(UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain& PropertyChain)
	{
		OverrideProperty(Object, FPropertyVisitorPath(PropertyEvent, PropertyChain));
	}

	/**
	 * Clears an overridden properties specified by the property chain
	 * @param Object owning the property to clear
	 * @param PropertyEvent only needed to know about the container item index in any
	 * @param PropertyChain to the property to clear from the root of the specified object
	 * @return true if the property was successfully cleared. */
	bool ClearOverriddenProperty(UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain& PropertyChain)
	{
		return ClearOverriddenProperty(Object, FPropertyVisitorPath(PropertyEvent, PropertyChain));
	}

	/**
	 * To be called prior to override a property of the specified object
	 * Note: Supports object that does not have overridable serialization enabled
	 * @param Object owning the property
	 * @param PropertyChain leading to the property about to be overridden */
	void PreOverrideProperty(UObject& Object, const FEditPropertyChain& PropertyChain)
	{
		PreOverrideProperty(Object, FPropertyVisitorPath(FPropertyChangedEvent(nullptr), PropertyChain));
	}

	/**
	 * To be called after the property was overridden of the specified object
	 * Note: Supports object that does not have overridable serialization enabled
	 * @param Object owning the property
	 * @param PropertyEvent information about the type of change including any container item index
	 * @param PropertyChain leading to the property that was overridden */
	void PostOverrideProperty(UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain& PropertyChain)
	{
		PostOverrideProperty(Object, FPropertyVisitorPath(PropertyEvent, PropertyChain), PropertyEvent.ChangeType);
	}

	/**
	 * Retrieve the overridable operation from the specified the edit property chain
	 * @param Object owning the property
	 * @param PropertyEvent only needed to know about the container item index in any
	 * @param PropertyChain leading to the property the caller is interested in
	 * @param bOutInheritedOperation optional parameter to know if the operation returned was inherited from a parent property
	 * @return the current type of override operation on the property */
	FORCEINLINE EOverriddenPropertyOperation GetOverriddenPropertyOperation(UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain& PropertyChain, bool* bOutInheritedOperation = nullptr)
	{
		return GetOverriddenPropertyOperation(Object, FPropertyVisitorPath(PropertyEvent, PropertyChain), bOutInheritedOperation);
	}


	void HandleObjectsReInstantiated(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

protected:
	FOverridableManager();

#if WITH_EDITORONLY_DATA
	FOverriddenPropertyAnnotations OverriddenObjectAnnotations;
#else
	FUObjectAnnotationSparseBool NeedsSubobjectTemplateInstantiation;
#endif // WITH_EDITORONLY_DATA

	friend FOverriddenPropertySet;
	COREUOBJECT_API void NotifyPropertyChange(const EPropertyNotificationType Notification, UObject& Object, FPropertyVisitorPath::Iterator PropertyIterator, const EPropertyChangeType::Type ChangeType);
	COREUOBJECT_API EOverriddenPropertyOperation GetOverriddenPropertyOperation(UObject& Object, FPropertyVisitorPath::Iterator PropertyIterator, bool* bOutInheritedOperation = nullptr);
	COREUOBJECT_API bool ClearOverriddenProperty(UObject& Object, FPropertyVisitorPath::Iterator PropertyIterator);
};

