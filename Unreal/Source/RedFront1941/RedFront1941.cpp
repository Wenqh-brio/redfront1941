// Copyright RedFront1941. All Rights Reserved.

#include "RedFront1941.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FRedFront1941Module, RedFront1941, "RedFront1941");

void FRedFront1941Module::StartupModule()
{
	// No engine-level hook is required yet: the JSON contracts are loaded lazily by
	// URFDataTableLoader so that -game runs do not pay for parsing unused tables.
	UE_LOG(LogTemp, Log, TEXT("RedFront1941 module started (2D Paper2D/PaperZD stack)."));
}

void FRedFront1941Module::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("RedFront1941 module shut down."));
}

FRedFront1941Module& FRedFront1941Module::Get()
{
	return FModuleManager::LoadModuleChecked<FRedFront1941Module>("RedFront1941");
}
