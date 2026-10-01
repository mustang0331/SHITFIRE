#include "ShitfireFixtures.h"
#include "ShitfireGame.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ShitfireFixtures
{

FString FixturesDir()
{
	// -ParityFixtures=<abs path> overrides (the Shipping gate exe may not sit
	// where ProjectDir resolves to the repo - see ShitfireParityGate.h).
	FString Override;
	if (FParse::Value(FCommandLine::Get(), TEXT("ParityFixtures="), Override) && !Override.IsEmpty())
	{
		return FPaths::ConvertRelativePathToFull(Override);
	}
	// ProjectDir = <repo>/port/unreal/ -> fixtures = <repo>/ENGINE_PORT/fixtures
	return FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("../../ENGINE_PORT/fixtures")));
}

TSharedPtr<FJsonObject> LoadJson(const FString& RelPath)
{
	const FString FullPath = FPaths::Combine(FixturesDir(), RelPath);
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FullPath))
	{
		UE_LOG(LogShitfire, Warning, TEXT("fixture load failed: %s"), *FullPath);
		return nullptr;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogShitfire, Warning, TEXT("fixture parse failed: %s"), *FullPath);
		return nullptr;
	}
	return Root;
}

}
