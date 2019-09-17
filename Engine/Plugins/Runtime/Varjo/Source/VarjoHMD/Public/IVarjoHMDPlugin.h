// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IHeadMountedDisplayModule.h"

namespace vr {
	class IVRSystem;
}

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class IVarjoHMDPlugin : public IHeadMountedDisplayModule
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IVarjoHMDPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IVarjoHMDPlugin >( "VarjoHMD" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "VarjoHMD" );
	}

	/**
	* Get the IVRSystem* that was previously set by the HMD implemenentation.
	*
	* @return The pointer if the HMD has been initialized, otherwise nullptr
	*/
	virtual vr::IVRSystem* GetVRSystem() const = 0;
};
