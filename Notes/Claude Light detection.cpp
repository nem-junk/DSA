#include "LightDetectionCharacter.h"

#include "Net/UnrealNetwork.h"          // DOREPLIFETIME macros
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"

#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

// -------------------------------------------------------
//  Constructor
// -------------------------------------------------------
ALightDetectionCharacter::ALightDetectionCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Enable full replication for this actor
    bReplicates                = true;
    bAlwaysRelevant            = false; // Set to true only if needed (e.g. for AI-observed characters)
    NetUpdateFrequency         = 10.0f; // How often (per second) the server sends updates to clients
    MinNetUpdateFrequency      = 2.0f;  // Minimum update frequency when nothing is moving

    // Light detection defaults
    LightLevel                 = 0.0f;
    bIsInLight                 = false;
    bWasInLight                = false;
    TimeSinceLastDetection     = 0.0f;

    DetectionUpdateInterval    = 0.1f;
    LightDetectionRadius       = 2000.0f;
    LightThreshold             = 0.25f;
    NumSampleRays              = 4;
    bDrawDebug                 = false;
}

// -------------------------------------------------------
//  GetLifetimeReplicatedProps
//  Tells UE which properties to sync and HOW.
// -------------------------------------------------------
void ALightDetectionCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Light state — replicate to ALL connected clients
    DOREPLIFETIME(ALightDetectionCharacter, LightLevel);
    DOREPLIFETIME(ALightDetectionCharacter, bIsInLight);

    // Config properties — only replicate to the owning client (saves bandwidth)
    DOREPLIFETIME_CONDITION(ALightDetectionCharacter, DetectionUpdateInterval, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ALightDetectionCharacter, LightDetectionRadius,    COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ALightDetectionCharacter, LightThreshold,          COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ALightDetectionCharacter, NumSampleRays,           COND_OwnerOnly);
}

// -------------------------------------------------------
//  BeginPlay
// -------------------------------------------------------
void ALightDetectionCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Only the server runs the detection logic
    if (HasAuthority())
    {
        SampleLightLevel();
    }
}

// -------------------------------------------------------
//  Tick  —  server-only throttled detection
// -------------------------------------------------------
void ALightDetectionCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Light sampling only runs on the server (authoritative)
    // Clients receive replicated results via OnRep callbacks
    if (!HasAuthority()) return;

    TimeSinceLastDetection += DeltaTime;

    if (TimeSinceLastDetection >= DetectionUpdateInterval)
    {
        TimeSinceLastDetection = 0.0f;
        SampleLightLevel();
    }
}

// -------------------------------------------------------
//  SetupPlayerInputComponent
// -------------------------------------------------------
void ALightDetectionCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    // Add your movement / action input bindings here
}

// -------------------------------------------------------
//  SampleLightLevel  —  runs on SERVER only
// -------------------------------------------------------
void ALightDetectionCharacter::SampleLightLevel()
{
    // Guard: never run sampling on a client
    if (!HasAuthority()) return;

    float NewLevel      = CalculateLightLevel();
    bool  bNewIsInLight = (NewLevel >= LightThreshold);

    // Update replicated properties — UE will sync these to clients automatically
    LightLevel = NewLevel;
    bIsInLight = bNewIsInLight;

    // Apply locally on the server too (fire delegates, etc.)
    ApplyLightState(NewLevel, bNewIsInLight);
}

// -------------------------------------------------------
//  ApplyLightState
//  Called on the SERVER after calculating, and on CLIENTS via RepNotify.
//  Fires all Blueprint delegates and handles transition events.
// -------------------------------------------------------
void ALightDetectionCharacter::ApplyLightState(float NewLevel, bool bNewIsInLight)
{
    OnLightLevelChanged.Broadcast(NewLevel);

    if (bNewIsInLight && !bWasInLight)
    {
        OnEnterLight.Broadcast();
    }
    else if (!bNewIsInLight && bWasInLight)
    {
        OnEnterShadow.Broadcast();
    }

    bWasInLight = bNewIsInLight;
}

// -------------------------------------------------------
//  RepNotify: OnRep_LightLevel
//  Fires on CLIENTS when LightLevel changes on the server.
// -------------------------------------------------------
void ALightDetectionCharacter::OnRep_LightLevel()
{
    // Clients update their local light level visuals/audio here
    // bIsInLight may arrive in the same batch; check both together
    ApplyLightState(LightLevel, bIsInLight);
}

// -------------------------------------------------------
//  RepNotify: OnRep_bIsInLight
//  Fires on CLIENTS when the light state boolean flips.
// -------------------------------------------------------
void ALightDetectionCharacter::OnRep_bIsInLight()
{
    // Transition events are already handled via OnRep_LightLevel in most cases,
    // but this covers cases where bIsInLight changes without LightLevel changing.
    if (bIsInLight != bWasInLight)
    {
        if (bIsInLight)
            OnEnterLight.Broadcast();
        else
            OnEnterShadow.Broadcast();

        bWasInLight = bIsInLight;
    }
}

// -------------------------------------------------------
//  Server RPC — Server_RequestLightSample
//  A client can ask the server to force a re-sample.
// -------------------------------------------------------
bool ALightDetectionCharacter::Server_RequestLightSample_Validate()
{
    // Add anti-cheat validation here if needed (e.g., rate limiting)
    return true;
}

void ALightDetectionCharacter::Server_RequestLightSample_Implementation()
{
    // This runs ON THE SERVER after the client calls it
    SampleLightLevel();
}

// -------------------------------------------------------
//  Multicast RPC — Multicast_ForceLightUpdate
//  Server tells ALL clients to update immediately (e.g., scripted event).
// -------------------------------------------------------
void ALightDetectionCharacter::Multicast_ForceLightUpdate_Implementation(float NewLightLevel, bool bNewIsInLight)
{
    // Skip on server — it already updated locally
