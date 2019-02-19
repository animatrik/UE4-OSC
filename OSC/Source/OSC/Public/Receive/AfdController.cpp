// Fill out your copyright notice in the Description page of Project Settings.

#include "OscPrivatePCH.h"
#include "AfdController.h"
#include "OscDispatcher.h"
#include "OscFunctionLibrary.h"

#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "EngineUtils.h"


AAfdController::AAfdController()
	: _listener(this),
	AddressFilter(TEXT("/afd/*"))
{
}

AAfdController::AAfdController(FVTableHelper & helper) :
	_listener(this),
	AddressFilter(TEXT("/afd/*"))
{
	// Does not need to be a valid object.	
}

void AAfdController::BeginPlay() 
{
	auto instance = UOscDispatcher::Get();
	if (instance && !HasAnyFlags(RF_ClassDefaultObject))
	{
		instance->RegisterReceiver(&_listener);

		UE_LOG(LogOSC, Display, TEXT("Registering actor %s"), *GetName());
	}
	ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
	FOnRecordingFinished& OnRecordingFinishedDelegate = SequenceRecorder.OnRecordingFinished();
	OnRecordingFinishedDelegate.AddUObject(this, &AAfdController::OnRecordingFinished);
	FOnRecordingStarted& OnRecordingStartedDelegate = SequenceRecorder.OnRecordingStarted();
	OnRecordingStartedDelegate.AddUObject(this, &AAfdController::OnRecordingStarted);
	UE_LOG(LogOSC, Display, TEXT("OnRecording* Delegates bound"));
}

void AAfdController::BeginDestroy()
{
	FName Address("/ue4/status");
	FName Response("disconnecting");
	SendCmd(Address, Response);
	auto instance = UOscDispatcher::Get();
	if (instance && !HasAnyFlags(RF_ClassDefaultObject))
	{
		instance->UnregisterReceiver(&_listener);

		UE_LOG(LogOSC, Display, TEXT("Unregistering actor %s"), *GetName());
	}

	Super::BeginDestroy();
}

const FString& AAfdController::GetAddressFilter() const
{
	return AddressFilter;
}

void AAfdController::SendEvent(const FName & Address, const TArray<FOscDataElemStruct> & Data, const FString & SenderIp)
{
	UE_LOG(LogOSC, Display, TEXT("AAfdController::SendEvent from %s - %s"), *GetName(), *Address.ToString());

	// break up the address
	FString AddressStr = Address.ToString();
	TArray<FString> Parsed;
	AddressStr.ParseIntoArray(Parsed, TEXT("/"), true);
	if (Parsed.Num() > 1)
	{
		if (Parsed[1] == FString("cmd"))
		{
			HandleCmdEvent(Address, Data, SenderIp);
		}
	}
	OnOscReceived(Address, Data, SenderIp);
}

void AAfdController::HandleCmdEvent(const FName & Address, const TArray<FOscDataElemStruct> & Data, const FString & SenderIp)
{
	UE_LOG(LogOSC, Display, TEXT("AAfdController::HandleCmdEvent from %s - %s"), *GetName(), *Address.ToString());

	if (Data.Num() == 0)
	{
		UE_LOG(LogOSC, Display, TEXT("Missing command parameter for %s"), *Address.ToString());
		return;
	}

	if (Data[0].IsString())
	{
		FString Cmd = Data[0].AsStringValue().ToString().ToLower();
		UE_LOG(LogOSC, Display, TEXT("Command Parameter: %s"), *Cmd);
		ISequenceRecorder& SequenceRecorder = FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
		if (Cmd.Equals(TEXT("connect"), ESearchCase::IgnoreCase))
		{
			FName SendAddress("/ue4/status");
			FName Response("connected");
			SendCmd(SendAddress, Response);
		}
		else if (Cmd.Equals(TEXT("queue_take"), ESearchCase::IgnoreCase))
		{
			FName SendAddress("/ue4/status");
			if (Data.Num() < 2)
			{
				FName sendAddress("/ue4/status");
				TArray<FName> msg;
				msg.Add(FName("error"));
				msg.Add(FName("missing take name"));
				SendCmds(sendAddress, msg);
			}
			else
			{
				_queuedTakeName = Data[1].AsStringValue().ToString();
			}
			FName Response("take_queued");
			SendCmd(SendAddress, Response);
		}
		else if (Cmd.Equals(TEXT("start_recording"), ESearchCase::IgnoreCase))
		{
			
			TArray<AActor*> ActorsToRecord = GetActorsToRecord();
			SequenceRecorder.StartRecording(ActorsToRecord, FString(), _queuedTakeName);
		}
		else if (Cmd.Equals(TEXT("stop_recording"), ESearchCase::IgnoreCase))
		{
			if (!SequenceRecorder.IsRecording())
			{
				UE_LOG(LogOSC, Display, TEXT("Received STOP_RECORD when not recording."));
				FName sendAddress("/ue4/status");
				FName response("error");
				//TArray<const FName&> msg;
				//msg.Add(response);
				//FName error_msg("not recording");
				//msg.Add(error_msg);
				//SendCmds(sendAddress, msg);
			}
			FName sendAddress("/ue4/status");
			FName response("saving");
			SendCmd(sendAddress, response);
			SequenceRecorder.StopRecording();
		}
		else if (Cmd.Equals(TEXT("cancel_recording"), ESearchCase::IgnoreCase))
		{
			SequenceRecorder.StopRecording();
			FName SendAddress("/ue4/status");
			FName Response("recording_cancelled");
			SendCmd(SendAddress, Response);
		}
		
	}

}

void AAfdController::SendCmd(const FName& Address, const FName& Cmd)
{
	TArray<FOscDataElemStruct> Data;
	FOscDataElemStruct elem;
	elem.SetString(Cmd);
	Data.Add(elem);
	UOscFunctionLibrary::SendOsc(Address, Data, -1);
}

void AAfdController::SendCmds(const FName& Address, const TArray<FName>& Cmds)
{
	TArray<FOscDataElemStruct> Data;
	for (const FName& Cmd : Cmds)
	{
		FOscDataElemStruct elem;
		elem.SetString(Cmd);
		Data.Add(elem);
	}
	UOscFunctionLibrary::SendOsc(Address, Data, -1);
}

TArray<AActor*> AAfdController::GetActorsToRecord()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(this->GetWorld(), AStaticMeshActor::StaticClass(), FoundActors);
	return FoundActors;

	/*
	TArray<AActor*> Actors;
	for (TActorIterator<AStaticMeshActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{


		// Same as with the Object Iterator, access the subclass instance with the * or -> operators.
		AStaticMeshActor* ActorPtr= *ActorItr;
		Actors.Add(ActorPtr)
		// ClientMessage(ActorItr->GetName());
		//ClientMessage(ActorItr->GetActorLocation().ToString());
	}
	*/
}

void AAfdController::OnRecordingStarted(UMovieSceneSequence* movieSequence)
{
	UE_LOG(LogOSC, Display, TEXT("Recording Started"));
	FName SendAddress("/ue4/status");
	FName Response("recording_started");
	SendCmd(SendAddress, Response);
}

void AAfdController::OnRecordingFinished(UMovieSceneSequence* movieSequence)
{
	UE_LOG(LogOSC, Display, TEXT("Recording Finished"));
	FName SendAddress("/ue4/status");
	FName Response("save_complete");
	SendCmd(SendAddress, Response);
}