// Copyright RedFront1941. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Primary game module for RedFront1941.
 * It owns the runtime data tables built from the shared JSON contracts
 * (see RFDataTableLoader) and the Paper2D flipbook facing conventions.
 */
class FRedFront1941Module : public IModuleInterface
{
public:
	/** IModuleInterface: registers the module's content-free runtime services. */
	virtual void StartupModule() override;

	/** IModuleInterface: releases the module's runtime services. */
	virtual void ShutdownModule() override;

	/**
	 * Accessor for the module singleton, used by RFDataTableLoader.
	 * @return Reference to the loaded RedFront1941 module.
	 */
	static FRedFront1941Module& Get();
};
