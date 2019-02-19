// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OscReceiverInterface.h"
#include "OscDataElemStruct.h"
#include "Editor/SequenceRecorder/Public/ISequenceRecorder.h"
#include "AfdController.generated.h"

UCLASS(ClassGroup = AFD)
class OSC_API AAfdController : public AActor
{
	GENERATED_BODY()
	
public:	
	
	UPROPERTY(EditAnywhere, Category = OSC)
	FString AddressFilter;

	UFUNCTION(BlueprintImplementableEvent, Category = OSC)
	void OnOscReceived(const FName & Address, const TArray<FOscDataElemStruct> & Data, const FString & SenderIp);

	AAfdController();

	/// Hot reload constructor
	AAfdController(FVTableHelper & helper);

	const FString & GetAddressFilter() const;

	void SendEvent(const FName & Address, const TArray<FOscDataElemStruct> & Data, const FString & SenderIp);

	void HandleCmdEvent(const FName & Address, const TArray<FOscDataElemStruct> & Data, const FString & SenderIp);

private:

	void BeginPlay() override;

	void BeginDestroy() override;

	void SendCmd(const FName& Address, const FName& Cmd);

	void SendCmds(const FName& Address, const TArray<FName>& Cmds);

	void OnRecordingStarted(UMovieSceneSequence* movieSequence);

	void OnRecordingFinished(UMovieSceneSequence* movieSequence);

	TArray<AActor*> GetActorsToRecord();

	BasicOscReceiver<AAfdController> _listener;

	FString _queuedTakeName;

};