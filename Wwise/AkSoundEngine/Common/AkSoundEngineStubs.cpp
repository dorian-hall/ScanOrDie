/*******************************************************************************
The content of this file includes portions of the proprietary AUDIOKINETIC Wwise
Technology released in source code form as part of the game integration package.
The content of this file may not be used without valid licenses to the
AUDIOKINETIC Wwise Technology.
Note that the use of the game engine is subject to the Unity(R) Terms of
Service at https://unity3d.com/legal/terms-of-service
 
License Usage
 
Licensees holding valid licenses to the AUDIOKINETIC Wwise Technology may use
this file in accordance with the end user license agreement provided with the
software or, alternatively, in accordance with the terms contained
in a written agreement between you and Audiokinetic Inc.
Copyright (c) 2024 Audiokinetic Inc.
*******************************************************************************/

#include "stdafx.h"
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkLock.h>
#include <AK/Tools/Common/AkAutoLock.h>

#define GCC_HASCLASSVISIBILITY
#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#if defined (__APPLE__)
	#include <malloc/malloc.h>
	#include <sys/mman.h>
#else
	#if ! defined (__PPU__) && ! defined (__SPU__)
		#include <stdlib.h>
	#else
		#include <ppu/include/stdlib.h>
	#endif // #if ! defined (__PPU__) && ! defined (__SPU__)
#endif // #if defined (__APPLE__)

#include <AK/SoundEngine/Common/AkMemoryMgr.h>
#include <AK/SoundEngine/Common/IAkStreamMgr.h>

#include "AkFilePackageLowLevelIOBlocking.h"
CAkFilePackageLowLevelIOBlocking  g_lowLevelIO;

#ifndef AK_OPTIMIZED
#include <AK/Comm/AkCommunication.h>
#endif // #ifndef AK_OPTIMIZED

#if defined AK_ANDROID 
#include "../Android/jni/AkUnityAndroidSystem.h"
#elif defined AK_HARMONY
#include "../OpenHarmony/AkUnityOpenHarmonySystem.h"
#endif // defined AK_ANDROID

#include "AkCallbackSerializer.h"

#if defined AK_XBOXONE
#include <xaudio2.h>
#elif defined AK_ANDROID || defined AK_LINUX || defined AK_GGP || defined AK_MAC_OS_X || defined AK_EMSCRIPTEN
#include <dlfcn.h>
#elif defined AK_NX
#include <nn/mem.h>
#endif

// Register plugins that are static linked in this DLL.  Others will be loaded dynamically.
#include <AK/Plugin/AkSilenceSourceFactory.h>					// Silence generator
#include <AK/Plugin/AkSineSourceFactory.h>						// Sine wave generator
#include <AK/Plugin/AkToneSourceFactory.h>						// Tone generator
#include <AK/Plugin/AkAudioInputSourceFactory.h>
#if defined(AK_NX)
#include <AK/Plugin/AkSynthOneSourceFactory.h>
#else
#if defined(AK_EMSCRIPTEN)
#include <AK/Plugin/AkSynthOneSourceFactory.h>
#endif
#include <AK/Plugin/Ak3DAudioBedMixerFXFactory.h>				// 3D Audio Bed Mixer
#include <AK/Plugin/AkCompressorFXFactory.h>					// Compressor
#include <AK/Plugin/AkDelayFXFactory.h>							// Delay
#include <AK/Plugin/AkExpanderFXFactory.h>						// Expander
#include <AK/Plugin/AkFlangerFXFactory.h>						// Flanger
#include <AK/Plugin/AkGainFXFactory.h>							// Gain
#include <AK/Plugin/AkGuitarDistortionFXFactory.h>				// Guitar distortion
#include <AK/Plugin/AkHarmonizerFXFactory.h>					// Harmonizer
#include <AK/Plugin/AkMatrixReverbFXFactory.h>					// Matrix reverb
#include <AK/Plugin/AkMeterFXFactory.h>							// Meter
#include <AK/Plugin/AkParametricEQFXFactory.h>					// Parametric equalizer
#include <AK/Plugin/AkPeakLimiterFXFactory.h>					// Peak limiter
#include <AK/Plugin/AkPitchShifterFXFactory.h>					// Pitch Shifter
#include <AK/Plugin/AkRecorderFXFactory.h>						// Recorder
#include <AK/Plugin/AkRoomVerbFXFactory.h>						// RoomVerb
#include <AK/Plugin/AkReflectFXFactory.h>						// Reflect
#include <AK/Plugin/AkStereoDelayFXFactory.h>					// Stereo Delay
#include <AK/Plugin/AkTimeStretchFXFactory.h>					// Time Stretch
#include <AK/Plugin/AkTremoloFXFactory.h>						// Tremolo
#endif

// Required by codecs plug-ins
#include <AK/Plugin/AkVorbisDecoderFactory.h>
#include <AK/Plugin/AkOpusDecoderFactory.h>
#if defined AK_NX
#include <AK/Plugin/AkOpusNXFactory.h>
#endif


#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/MusicEngine/Common/AkMusicEngine.h>
#include <AK/SoundEngine/Common/AkModule.h>
#include <AK/SoundEngine/Common/AkStreamMgrModule.h>
#include <AK/Tools/Common/AkMonitorError.h>
#include <AK/Plugin/AkReflectGameData.h>
#include <AK/SpatialAudio/Common/AkSpatialAudioTypes.h>
#include <AK/SpatialAudio/Common/AkSpatialAudio.h>

#define AKSOUNDENGINESTUBS_CPP
#include "AkSoundEngineStubs.h"
#undef AKSOUNDENGINESTUBS_CPP

#include <AK/AkWwiseSDKVersion.h>

#ifdef AK_GGP
#include <ggp/application.h>
#endif

// Defines.

#if defined (WIN32) || defined (WIN64) || defined (_XBOX_VER)
	#define AK_PLATFORM_PATH_SEPARATOR '\\'
	#define AK_STRING_PATH_SEPERATOR AKTEXT("\\")
#else
	#define AK_PLATFORM_PATH_SEPARATOR '/'
	#define AK_STRING_PATH_SEPERATOR AKTEXT("/")
#endif // #if defined (WIN32) || defined (WIN64) || defined (_XBOX_VER)

#if defined(__ANDROID__) || defined(__HARMONY__) || (defined(AK_LINUX) && !defined(AK_GGP)) || defined(AK_NX) || defined(AK_EMSCRIPTEN)
#define MONOCHAR char
#define CONVERT_MONOCHAR_TO_OSCHAR(__SRC, __DST) CONVERT_CHAR_TO_OSCHAR(__SRC, __DST)
#define CONVERT_MONOCHAR_TO_CHAR(__SRC, __DST) __DST = const_cast<char*>(__SRC)
#else
#define MONOCHAR wchar_t
#define CONVERT_MONOCHAR_TO_OSCHAR(__SRC, __DST) CONVERT_WIDE_TO_OSCHAR(__SRC, __DST)
#define CONVERT_MONOCHAR_TO_CHAR(__SRC, __DST) __DST = (char*)AkAlloca( (1 + wcslen( __SRC )) * sizeof(char)); AKPLATFORM::AkWideCharToChar(__SRC, (AkUInt32)(wcslen( __SRC ) + 1), __DST)
#endif

#ifndef AK_OPTIMIZED
char g_GameName[AK_COMM_SETTINGS_MAX_STRING_SIZE];
#endif

AkOSChar g_basePath[AK_MAX_PATH] = AKTEXT("");
AkOSChar g_decodedBankPath[AK_MAX_PATH] = AKTEXT("");

namespace AkCallbackSerializerHelper
{
	void LocalOutput(AK::Monitor::ErrorCode in_eErrorCode, const AkOSChar* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID, AkGameObjectID in_gameObjID);
}

void AkUnityAssertHook(
	const char * in_pszExpression,	///< Expression
	const char * in_pszFileName,	///< File Name
	int in_lineNumber				///< Line Number
	)
{
	AkOSChar* os_pszExpression = NULL;
	AkOSChar* os_pszFileName = NULL;
	CONVERT_CHAR_TO_OSCHAR(in_pszExpression, os_pszExpression);
	CONVERT_CHAR_TO_OSCHAR(in_pszFileName, os_pszFileName);
	size_t msgLen = AKPLATFORM::OsStrLen(os_pszExpression) + AKPLATFORM::OsStrLen(os_pszFileName) + 128;
	AkOSChar* msg = (AkOSChar*)AkAlloca(msgLen);
	AK_OSPRINTF(msg, msgLen, AKTEXT("AKASSERT: %s. File: %s, line: %d"), os_pszExpression, os_pszFileName, in_lineNumber);

	AkCallbackSerializerHelper::LocalOutput(AK::Monitor::ErrorCode_NoError, msg, AK::Monitor::ErrorLevel_Error, 0, AK_INVALID_GAME_OBJECT);
}

// Prototype declaration
AKRESULT SaveDecodedBank(AkOSChar * in_OrigBankFile, void * in_pDecodedData, AkUInt32 in_decodedSize, bool in_bIsLanguageSpecific);

AKRESULT CreateDirectoryStructure(const AkOSChar* in_pszDirectory)
{
	// We expect in_pszDirectory to be a path to a directory, not a file
	bool bSkipFilename = false;
	return CAkFileHelpers::ForEachDirectoryLevel(in_pszDirectory,
		[](const AkOSChar* directory) {
			return CAkFileHelpers::CreateEmptyDirectory(directory);
		}
	, bSkipFilename);
}

void AK_STDCALL AkDefaultLogger(const char* message)
{
	const AkUInt32 length = (AkUInt32)(1 + strlen(message));
	char* formattedMessage = (char*)AkAlloca(length * sizeof(char));

	AKPLATFORM::SafeStrCpy(formattedMessage, message, length);
	AKPLATFORM::SafeStrCat(formattedMessage, "\n", length);
	AKPLATFORM::OutputDebugMsg(formattedMessage);
}

AkErrorLogger errorlogger = AkDefaultLogger;

void SetErrorLogger(AkErrorLogger logger)
{
	errorlogger = (logger == NULL) ? AkDefaultLogger : logger;
}

AkGetAudioSamples audioSamplesFunction = NULL;
AkGetAudioFormat audioFormatFunction = NULL;

void GetAudioFormat(AkPlayingID playingID, AkAudioFormat& format)
{
	if (audioFormatFunction)
		audioFormatFunction(playingID, &format);

	format.uTypeID = AK_INT;
	format.uInterleaveID = AK_NONINTERLEAVED;
}

void GetAudioSamples(AkPlayingID playingID, AkAudioBuffer* buffer)
{
	if (!buffer)
		return;

	buffer->eState = AK_NoMoreData;

	const AkUInt16 frameCount = buffer->MaxFrames();
	buffer->uValidFrames = frameCount;

	if (!audioSamplesFunction)
	{
		buffer->ZeroPadToMaxFrames();
		return;
	}

	const AkUInt32 channelCount = buffer->NumChannels();
	for (AkUInt32 i = 0; i < channelCount; ++i)
		if (audioSamplesFunction(playingID, buffer->GetChannel(i), i, frameCount))
			buffer->eState = AK_DataReady;
}

void SetAudioInputCallbacks(AkGetAudioSamples getAudioSamples, AkGetAudioFormat getAudioFormat)
{
	audioSamplesFunction = getAudioSamples;
	audioFormatFunction = getAudioFormat;

	SetAudioInputCallbacks(GetAudioSamples, GetAudioFormat, NULL);
}

AkCommunicationSettings::AkCommunicationSettings()
{
	szAppNetworkName[0] = 0;

#ifndef AK_OPTIMIZED
	AkCommSettings settingsComm;
	AK::Comm::GetDefaultInitSettings(settingsComm);

	uDiscoveryBroadcastPort = settingsComm.ports.uDiscoveryBroadcast;
	uCommandPort = settingsComm.ports.uCommand;

#ifndef AK_NX
	bInitSystemLib = settingsComm.bInitSystemLib;
	commSystem = AkCommSystem_Socket;
#else
	bInitSystemLib = false;
	commSystem = static_cast<AkCommunicationSettings::AkCommSystem>(settingsComm.commSystem);
#ifdef AK_USE_NX_HTCS
	commsThreadProperties = settingsComm.threadProperties;
#endif // AK_USE_NX_HTCS
#endif // AK_NX

#endif // AK_OPTIMIZED
}

AkInitializationSettings::AkInitializationSettings()
{
	AK::StreamMgr::GetDefaultSettings(streamMgrSettings);
	AK::StreamMgr::GetDefaultDeviceSettings(deviceSettings);
	AK::SoundEngine::GetDefaultInitSettings(initSettings);
	AK::SoundEngine::GetDefaultPlatformInitSettings(platformSettings);
	AK::MusicEngine::GetDefaultInitSettings(musicSettings);

	uMemAllocationSizeLimit = 0;
	uMemDebugLevel = 0;
	uMemSpanCount = (AkUInt32)AkSpanCount_Huge;

#ifdef AK_NX
	// This is a temporary fix for WG-39248. Once a similar modification is made within the engine, this can be removed.
	platformSettings.threadLEngine.iIdealCore = 2;
	platformSettings.threadLEngine.affinityMask = 1 << 2;
	platformSettings.threadBankManager.iIdealCore = 2;
	platformSettings.threadBankManager.affinityMask = 1 << 2;
	platformSettings.threadMonitor.iIdealCore = 2;
	platformSettings.threadMonitor.affinityMask = 1 << 2;
	platformSettings.threadOpusDecoder.iIdealCore = 2;
	platformSettings.threadOpusDecoder.affinityMask = 1 << 2;
	platformSettings.threadOutputMgr.iIdealCore = 2;
	platformSettings.threadOutputMgr.affinityMask = 1 << 2;
	deviceSettings.threadProperties.iIdealCore = 2;
	deviceSettings.threadProperties.affinityMask = 1 << 2;
#endif // AK_NX

#ifdef AK_ANDROID
	platformSettings.threadLEngine.uStackSize = 512*1024;	
#endif
}

AkInitializationSettings::~AkInitializationSettings()
{
	delete[] initSettings.szPluginDLLPath;
}

AkSerializedExternalSourceInfo::~AkSerializedExternalSourceInfo()
{
	// ensure proper deletion of the szFile member
	AkSerializedExternalSourceInfo_szFile_set(this, 0);
}

AkSerializedExternalSourceInfo::AkSerializedExternalSourceInfo(AkOSChar* in_pszFileName, AkUInt32 in_iExternalSrcCookie, AkCodecID in_idCodec)
	: AkExternalSourceInfo()
{
	iExternalSrcCookie = in_iExternalSrcCookie;
	idCodec = in_idCodec;
	AkSerializedExternalSourceInfo_szFile_set(this, in_pszFileName);
}

void AkSerializedExternalSourceInfo::Clear()
{
	// this function is called at construction of the array class in C# to wipe the memory as if this specific object were default constructed
	iExternalSrcCookie = 0;
	idCodec = 0;
	szFile = 0;
	pInMemory = 0;
	uiMemorySize = 0;
	idFile = 0;
}

void AkSerializedExternalSourceInfo::Clone(const AkSerializedExternalSourceInfo& other)
{
	iExternalSrcCookie = other.iExternalSrcCookie;
	idCodec = other.idCodec;
	AkSerializedExternalSourceInfo_szFile_set(this, other.szFile);
	pInMemory = other.pInMemory;
	uiMemorySize = other.uiMemorySize;
	idFile = other.idFile;
}

AkUInt32 AkSerializedExternalSourceInfo_iExternalSrcCookie_get(AkSerializedExternalSourceInfo *info) { return info->iExternalSrcCookie; }
void AkSerializedExternalSourceInfo_iExternalSrcCookie_set(AkSerializedExternalSourceInfo *info, AkUInt32 value) { info->iExternalSrcCookie = value; }

AkCodecID AkSerializedExternalSourceInfo_idCodec_get(AkSerializedExternalSourceInfo *info) { return info->idCodec; }
void AkSerializedExternalSourceInfo_idCodec_set(AkSerializedExternalSourceInfo *info, AkCodecID value) { info->idCodec = value; }

AkOSChar* AkSerializedExternalSourceInfo_szFile_get(AkSerializedExternalSourceInfo *info) { return info->szFile; }
void AkSerializedExternalSourceInfo_szFile_set(AkSerializedExternalSourceInfo *info, AkOSChar* fileName)
{
	delete[] info->szFile;
	info->szFile = 0;

	if (fileName)
	{
		size_t length = AKPLATFORM::OsStrLen(fileName);
		if (length > 0)
		{
			info->szFile = new AkOSChar[length + 1];
			AKPLATFORM::SafeStrCpy(info->szFile, fileName, length + 1);
		}
	}
}

void* AkSerializedExternalSourceInfo_pInMemory_get(AkSerializedExternalSourceInfo *info) { return info->pInMemory; }
void AkSerializedExternalSourceInfo_pInMemory_set(AkSerializedExternalSourceInfo *info, void* value) { info->pInMemory = value; }

AkUInt32 AkSerializedExternalSourceInfo_uiMemorySize_get(AkSerializedExternalSourceInfo *info) { return info->uiMemorySize; }
void AkSerializedExternalSourceInfo_uiMemorySize_set(AkSerializedExternalSourceInfo *info, AkUInt32 value) { info->uiMemorySize = value; }

AkFileID AkSerializedExternalSourceInfo_idFile_get(AkSerializedExternalSourceInfo *info) { return info->idFile; }
void AkSerializedExternalSourceInfo_idFile_set(AkSerializedExternalSourceInfo *info, AkFileID value) { info->idFile = value; }

namespace ResourceMonitoring
{
	CAkLock Lock;
	AkResourceMonitorDataSummary savedDataSummary;

	void ClearDataSummary()
	{
		AkAutoLock<CAkLock> autoLock(Lock);
		savedDataSummary = AkResourceMonitorDataSummary{};
	}

	void GetResourceMonitorDataSummary(AkResourceMonitorDataSummary& resourceMonitorDataSummary)
	{
		AkAutoLock<CAkLock> autoLock(Lock);
		resourceMonitorDataSummary = savedDataSummary;
	}

	void Callback(const AkResourceMonitorDataSummary* in_pdataSummary)
	{
		if (in_pdataSummary)
		{
			AkAutoLock<CAkLock> autoLock(Lock);
			savedDataSummary = *in_pdataSummary;
		}
	}
}

namespace Capture
{
	template<typename Type>
	struct ListBareWithClear : AkListBare<Type>
	{
		auto begin() const { return AkListBare<Type>::Begin(); }
		auto end() const { return AkListBare<Type>::End(); }

		void Clear()
		{
			auto nn = end();
			for (auto it = begin(); it != nn; )
			{
				auto item = *it;
				++it;
				AkDelete(AkMemID_Integration, item);
			}

			AkListBare<Type>::RemoveAll();
		}
	};

	struct AudioData final
	{
		static AkUInt32 GetBufferSampleCount(const AkAudioBuffer& akAudioBuffer)
		{
			return static_cast<AkUInt32>(akAudioBuffer.uValidFrames) * akAudioBuffer.NumChannels();
		}

		const AkUInt32 uNumSamples{};

		AudioData(AkUInt32 uNumSamples)
			: uNumSamples(uNumSamples)
			, pSamples(reinterpret_cast<AkReal32*>(AkAlloc(AkMemID_Integration, uNumSamples * sizeof(*pSamples))))
		{
		}

		~AudioData()
		{
			AkFree(AkMemID_Integration, pSamples);
		}

		AudioData* pNextItem{};

		void CopyFrom(const AkAudioBuffer& akAudioBuffer)
		{
			AKASSERT(GetBufferSampleCount(akAudioBuffer) == uNumSamples);
			AKASSERT(pSamples);
			AKPLATFORM::AkMemCpy(pSamples, const_cast<AkAudioBuffer&>(akAudioBuffer).GetInterleavedData(), uNumSamples * sizeof(*pSamples));
		}

		AkUInt32 CopyTo(AkReal32* out_pSamples, AkUInt32 uCopySize, AkUInt32 uReadOffset)
		{
			AKASSERT(out_pSamples);
			AKASSERT(uNumSamples > uReadOffset);

			const AkUInt32 uRemainder{ uNumSamples - uReadOffset };
			if (uRemainder < uCopySize)
			{
				uCopySize = uRemainder;
			}

			AKPLATFORM::AkMemCpy(out_pSamples, pSamples + uReadOffset, uCopySize * sizeof(*pSamples));
			return uCopySize;
		}

		AudioData(AudioData&&) = default;
		AudioData(const AudioData&) = delete;
		AudioData& operator=(const AudioData) = delete;

	private:
		AkReal32* const pSamples{};
	};

	struct AudioDataList : ListBareWithClear<AudioData>
	{
		void Splice(AudioDataList& other)
		{
			if (m_pFirst == NULL)
			{
				m_pFirst = other.m_pFirst;
			}
			else
			{
				AkListBareNextItem<AudioData>::Get(Last()) = other.m_pFirst;
			}

			SetLast(other.Last());

			for (AudioData* audioData : other)
			{
				IncrementCount(audioData);
			}

			other.RemoveAll();
		}
	};

	struct AudioDataListWithLock final
	{
		void AddAudioData(AudioData* audioData)
		{
			AkAutoLock<CAkLock> autoLock(lock);
			list.AddLast(audioData);
		}

		void TransferTo(AudioDataList& other)
		{
			AkAutoLock<CAkLock> autoLock(lock);
			other.Splice(list);
		}

		AudioData* ExtractAudioData(AkUInt32 numSamples)
		{
			{
				AkAutoLock<CAkLock> autoLock(lock);
				auto end = list.End();
				for (auto it = list.BeginEx(); it != end; ++it)
				{
					auto audioData = *it;
					if (audioData->uNumSamples == numSamples)
					{
						list.Erase(it);
						return audioData;
					}
				}
			}

			return nullptr;
		}

		void Clear()
		{
			AkAutoLock<CAkLock> autoLock(lock);
			list.Clear();
		}

	private:
		CAkLock lock;
		AudioDataList list;
	};

	static AudioDataListWithLock g_reuseList;

	struct DeviceCaptureData final
	{
		DeviceCaptureData(AkOutputDeviceID in_idOutput) : outputDeviceId(in_idOutput) {}

		~DeviceCaptureData()
		{
			if (isRegistered)
				Unregister();
		}

		DeviceCaptureData* pNextItem{};

		void Register()
		{
			if (isRegistered || AK::SoundEngine::RegisterCaptureCallback(&Callback, outputDeviceId, this) != AK_Success)
				return;

			isRegistered = true;
		}

		void Unregister()
		{
			if (!isRegistered)
				return;

			AK::SoundEngine::UnregisterCaptureCallback(&Callback, outputDeviceId, this);
			isRegistered = false;
		}

		AkOutputDeviceID GetOutputDeviceID() const { return outputDeviceId; }

		AkUInt32 UpdateSampleCount()
		{
			accumulatingList.TransferTo(outgoingList);

			AkUInt32 uCapturedSampleCount{};
			for (AudioData* audioData : outgoingList)
			{
				uCapturedSampleCount += audioData->uNumSamples;
			}

			AKASSERT(uCapturedSampleCount >= readOffset);
			return uCapturedSampleCount - readOffset;
		}

		AkUInt32 GetSamples(AkReal32* out_pSamples, AkUInt32 in_uBufferSize)
		{
			auto it = outgoingList.BeginEx();

			AkUInt32 uRemainingOutputBufferSize{ in_uBufferSize };
			for (auto end = outgoingList.End(); it != end; )
			{
				if (uRemainingOutputBufferSize == 0)
					break;

				auto audioData = *it;
				it = outgoingList.Erase(it);

				if (readOffset > audioData->uNumSamples)
				{
					AKASSERT(audioData->uNumSamples > readOffset);
					readOffset = 0;
				}

				const AkUInt32 uCopySize = audioData->CopyTo(out_pSamples, uRemainingOutputBufferSize, readOffset);
				out_pSamples += uCopySize;
				readOffset += uCopySize;
				readOffset %= audioData->uNumSamples;
				uRemainingOutputBufferSize -= uCopySize;

				g_reuseList.AddAudioData(audioData);
			}

			return in_uBufferSize - uRemainingOutputBufferSize;
		}

		void Clear()
		{
			accumulatingList.Clear();
			outgoingList.Clear();
			readOffset = 0;
		}

	private:
		AudioDataListWithLock accumulatingList;
		AudioDataList outgoingList;
		const AkOutputDeviceID outputDeviceId{ AK_INVALID_OUTPUT_DEVICE_ID };
		AkUInt32 readOffset{};
		bool isRegistered{ false };

		void FillBuffers(AkAudioBuffer& in_CaptureBuffer)
		{
			auto numSamples = AudioData::GetBufferSampleCount(in_CaptureBuffer);
			if (numSamples == 0)
				return;

			auto audioData = g_reuseList.ExtractAudioData(numSamples);
			if (!audioData)
				audioData = AkNew(AkMemID_Integration, AudioData(numSamples));
			AKASSERT(audioData);

			audioData->CopyFrom(in_CaptureBuffer);
			accumulatingList.AddAudioData(audioData);
		}

		static void Callback(AkAudioBuffer& in_CaptureBuffer, AkOutputDeviceID in_idOutput, void* in_pCookie)
		{
			if (auto deviceCaptureData = reinterpret_cast<DeviceCaptureData*>(in_pCookie))
			{
				deviceCaptureData->FillBuffers(in_CaptureBuffer);
			}
		}
	};

	struct DeviceCaptureDataMap final
	{
		DeviceCaptureData& GetOrCreateDeviceCaptureData(AkOutputDeviceID in_idOutput)
		{
			auto deviceCaptureData = TryGetDeviceCaptureData(in_idOutput);
			if (deviceCaptureData)
				return *deviceCaptureData;

			deviceCaptureData = AkNew(AkMemID_Integration, DeviceCaptureData(in_idOutput));
			AKASSERT(deviceCaptureData);

			{
				AkAutoLock<CAkLock> autoLock(lock);
				deviceIdToCaptureDataList.AddLast(deviceCaptureData);
			}

			return *deviceCaptureData;
		}

		DeviceCaptureData* TryGetDeviceCaptureData(AkOutputDeviceID in_idOutput)
		{
			AkAutoLock<CAkLock> autoLock(lock);
			for (DeviceCaptureData* deviceCaptureData : deviceIdToCaptureDataList)
				if (deviceCaptureData->GetOutputDeviceID() == in_idOutput)
					return deviceCaptureData;

			return nullptr;
		}

		void ClearData()
		{
			AkAutoLock<CAkLock> autoLock(lock);
			for (DeviceCaptureData* deviceCaptureData : deviceIdToCaptureDataList)
				deviceCaptureData->Clear();
		}

		void Terminate()
		{
			AkAutoLock<CAkLock> autoLock(lock);
			deviceIdToCaptureDataList.Clear();
		}

	private:
		CAkLock lock;
		ListBareWithClear<DeviceCaptureData> deviceIdToCaptureDataList;
	};

	static DeviceCaptureDataMap g_perDeviceCaptureData;

	AkForceInline static DeviceCaptureData& GetDeviceCaptureData(AkOutputDeviceID in_idOutput)
	{
		return g_perDeviceCaptureData.GetOrCreateDeviceCaptureData(in_idOutput);
	}

	AkForceInline static DeviceCaptureData* TryGetDeviceCaptureData(AkOutputDeviceID in_idOutput)
	{
		return g_perDeviceCaptureData.TryGetDeviceCaptureData(in_idOutput);
	}

	AkForceInline static void ClearCaptureData()
	{
		g_perDeviceCaptureData.ClearData();
		g_reuseList.Clear();
	}

	AkForceInline static void Terminate()
	{
		g_perDeviceCaptureData.Terminate();
		g_reuseList.Clear();
	}
}

//-----------------------------------------------------------------------------------------
// Sound Engine initialization.
//-----------------------------------------------------------------------------------------

AKRESULT Init(AkInitializationSettings* settings)
{
	if (!settings)
	{
		errorlogger("Null pointer to AkInitializationSettings structure.");
		return AK_InvalidParameter;
	}

	AkMemSettings memSettings;
	AK::MemoryMgr::GetDefaultSettings(memSettings);
	
#if defined(AK_XBOX)
	if(settings->platformSettings.uMaxXMAVoices > 0)
	{
		// Perform this as early as possible to ensure that no other allocation calls are made before this!
		HRESULT ApuCreateHeapResult = ApuCreateHeap(settings->unityPlatformSpecificSettings.ApuHeapCachedSize, settings->unityPlatformSpecificSettings.ApuHeapNonCachedSize);
		if (ApuCreateHeapResult == APU_E_HEAP_ALREADY_ALLOCATED)
		{
			errorlogger("APU heap has already been allocated.");
		}
		else if (ApuCreateHeapResult != S_OK)
		{
			errorlogger("APU heap could not be allocated.");
		}
	}
	else
	{
		memSettings.bUseDeviceMemAlways = true;
		memSettings.pfAllocDevice = memSettings.pfAllocVM;
		memSettings.pfFreeDevice = memSettings.pfFreeVM;
	}
#endif // AK_XBOX

	settings->initSettings.pfnAssertHook = AkUnityAssertHook;	

	memSettings.uMemAllocationSizeLimit = settings->uMemAllocationSizeLimit;
	memSettings.uMemoryDebugLevel = settings->uMemDebugLevel;
	memSettings.uVMSpanCount = (AkSpanCount)settings->uMemSpanCount;
	memSettings.uDeviceSpanCount = (AkSpanCount)settings->uMemSpanCount;

	// Create and initialize an instance of our memory manager.
	if (AK::MemoryMgr::Init(&memSettings) != AK_Success)
	{
		errorlogger("Could not create the memory manager.");
		return AK_MemManagerNotInitialized;
	}

	// Create and initialize an instance of the default stream manager.
	if (!AK::StreamMgr::Create(settings->streamMgrSettings))
	{
		errorlogger("Could not create the Stream Manager.");
		return AK_StreamMgrNotInitialized;
	}

	// Create an IO device.
#if defined AK_ANDROID
	settings->platformSettings.pJavaVM = AkUnityAndroidSystem::g_JavaVm;
	if (AkUnityAndroidSystem::InitAndroidSystem(settings->platformSettings.jActivity) != AK_Success)
	{
		errorlogger("Android initialization failure.");
		return AK_Fail;
	}
#endif

#ifdef AK_GGP
	ggp::InitAllocator();
#endif

	if (g_lowLevelIO.Init(settings->deviceSettings, settings->useAsyncOpen) != AK_Success)
	{
		errorlogger("Cannot create streaming I/O device.");
		return AK_Fail;
	}

#if defined AK_HARMONY
	g_lowLevelIO.UseResourceManager(AkUnityOpenHarmonySystem::Instance().GetResourceManager());
#endif

#if defined(AK_IOS) || defined(AK_TVOS) || defined(AK_VISIONOS)
	settings->platformSettings.audioCallbacks.interruptionCallback = AkCallbackSerializer::AudioInterruptionCallbackFunc;
#endif
	settings->initSettings.BGMCallback = AkCallbackSerializer::AudioSourceChangeCallbackFunc;

#ifdef AK_VORBIS_HW_SUPPORTED
	static bool bVorbisHwAcceleratorLoaded = false;
	if (!bVorbisHwAcceleratorLoaded && settings->platformSettings.bVorbisHwAcceleration)
	{
		AKRESULT eResult = AK::LoadVorbisHwAcceleratorLibrary("AkVorbisHwAccelerator", settings->initSettings.szPluginDLLPath);
		switch (eResult)
		{
			case AK_Success:
				bVorbisHwAcceleratorLoaded = true;
				break;
			// A failure here is not critical. We emit the proper error in the log, but we can allow the sound engine to continue to initialize.
			case AK_NotCompatible:
				errorlogger("Vorbis acceleration library failed to load due to platform SDK mismatch.");
				break;
			case AK_InvalidFile:
				errorlogger("Vorbis acceleration library failed to load because the library file could not be found in the game package.");
				break;
			case AK_InsufficientMemory:
				errorlogger("Vorbis acceleration library failed to load due to insufficient resources or memory.");
				break;
			default:
				errorlogger("Vorbis acceleration library failed to load due an unexpected error.");
				break;
		}
	}
#endif

	// Initialize sound engine.
	AKRESULT res = AK::SoundEngine::Init(&settings->initSettings, &settings->platformSettings);
	if ( res != AK_Success )
	{
		errorlogger("Cannot initialize sound engine.");
		return res;
	}

	// Initialize music engine.
	res = AK::MusicEngine::Init(&settings->musicSettings);
	if ( res != AK_Success )
	{
		errorlogger("Cannot initialize music engine.");
		AK::SoundEngine::Term();
		return res;
	}

	return AK_Success;
}

AKRESULT InitSpatialAudio(AkSpatialAudioInitSettings* settings)
{
	if (!settings)
	{
		errorlogger("Null pointer to AkSpatialAudioInitSettings structure.");
		return AK_InvalidParameter;
	}

	// Initialize SpatialAudio.
	if (AK::SpatialAudio::Init(*settings) != AK_Success)
	{
		errorlogger("Cannot initialize spatial audio.");
		return AK_Fail;
	}

	// disable spatial audio until a spatial audio listener is registered
	AK::SpatialAudio::RegisterListener(AK_INVALID_GAME_OBJECT);
	return AK_Success;
}

AKRESULT InitCommunication(AkCommunicationSettings* settings)
{
#ifdef AK_OPTIMIZED
	return AK_NotImplemented;
#else
	if (!settings)
	{
		errorlogger("Null pointer to AkCommunicationSettings structure.");
		return AK_InvalidParameter;
	}

#if defined(AK_XBOXONE)
	try
	{
		// Make sure networkmanifest.xml is loaded by instantiating a Microsoft.Xbox.Networking object.
		auto secureDeviceAssociationTemplate = Windows::Xbox::Networking::SecureDeviceAssociationTemplate::GetTemplateByName( "WwiseDiscovery" );
	}
	catch( Platform::Exception ^e )
	{
		errorlogger("Wwise network sockets not found in manifest file. Profiling will be impossible.");
	}
#endif // AK_XBOXONE

	// Initialize communication.
	AkCommSettings settingsComm;
	AK::Comm::GetDefaultInitSettings(settingsComm);
	settingsComm.ports.uDiscoveryBroadcast = settings->uDiscoveryBroadcastPort;
	settingsComm.ports.uCommand = settings->uCommandPort;
	settingsComm.bInitSystemLib = settings->bInitSystemLib;
	settingsComm.commSystem = static_cast<AkCommSettings::AkCommSystem>(settings->commSystem);

#ifdef AK_USE_NX_HTCS
	settingsComm.threadProperties = settings->commsThreadProperties;
#endif // AK_USE_NX_HTCS

	AKPLATFORM::SafeStrCpy(settingsComm.szAppNetworkName, settings->szAppNetworkName, AK_COMM_SETTINGS_MAX_STRING_SIZE);
	if (AK::Comm::Init(settingsComm) != AK_Success)
	{
		errorlogger("Cannot initialize Wwise communication.");
		return AK_Fail;
	}

	return AK_Success;
#endif // AK_OPTIMIZED
}

//-----------------------------------------------------------------------------------------
// Sound Engine termination.
//-----------------------------------------------------------------------------------------
void Term()
{
	if (!AK::SoundEngine::IsInitialized())
	{
		errorlogger("Term() called before successful initialization.");
		return;
	}

	Capture::Terminate();

	AK::SoundEngine::StopAll();

#ifndef AK_OPTIMIZED
	AkCallbackSerializer::Term();
	AK::Comm::Term();
#endif // AK_OPTIMIZED

	AK::MusicEngine::Term();

	AK::SoundEngine::Term();

	g_lowLevelIO.Term();
	
#if defined AK_ANDROID
	AkUnityAndroidSystem::TermAndroidSystem();
#endif
	

	if ( AK::IAkStreamMgr::Get() )
		AK::IAkStreamMgr::Get()->Destroy();

	AK::MemoryMgr::Term();
}

AKRESULT SetGameName(const MONOCHAR* in_GameName)
{
#ifdef AK_OPTIMIZED
	return AK_NotImplemented;
#else
	AkOSChar* GameNameOsString = NULL;
	char* CharName;
	CONVERT_MONOCHAR_TO_OSCHAR(in_GameName, GameNameOsString);
	CONVERT_OSCHAR_TO_CHAR(GameNameOsString, CharName);
	AKPLATFORM::SafeStrCpy(g_GameName, CharName, AK_COMM_SETTINGS_MAX_STRING_SIZE);
	return AK_Success;
#endif
}

//-----------------------------------------------------------------------------------------
// Access to LowLevelIO's file localization.
//-----------------------------------------------------------------------------------------
// File system interface.
AKRESULT SetBasePath(const MONOCHAR* in_pszBasePath)
{
	AkOSChar* basePathOsString = NULL;
	CONVERT_MONOCHAR_TO_OSCHAR(in_pszBasePath, basePathOsString);
	AKPLATFORM::SafeStrCpy(g_basePath, basePathOsString, AK_MAX_PATH);
	return g_lowLevelIO.SetBasePath(basePathOsString);
}

AKRESULT AddBasePath(const MONOCHAR* in_pszBasePath)
{
	AkOSChar* basePathOsString = NULL;
	CONVERT_MONOCHAR_TO_OSCHAR(in_pszBasePath, basePathOsString);
	return g_lowLevelIO.AddBasePath(basePathOsString);
}

AKRESULT SetDecodedBankPath(const MONOCHAR* in_DecodedPath)
{
	AkOSChar* decodedBankPath = NULL;
	CONVERT_MONOCHAR_TO_OSCHAR(in_DecodedPath, decodedBankPath);

	AKRESULT result = CreateDirectoryStructure(decodedBankPath);
	AKPLATFORM::SafeStrCpy(g_decodedBankPath, (result == AK_Success) ? decodedBankPath : AKTEXT(""), AK_MAX_PATH);

	return result;
}

AKRESULT SetCurrentLanguage(const MONOCHAR*	in_pszLanguageName)
{
	AkOSChar* languageOsString = NULL;
	CONVERT_MONOCHAR_TO_OSCHAR(in_pszLanguageName, languageOsString);

	if (AKPLATFORM::OsStrCmp(g_decodedBankPath, AKTEXT("")) != 0)
	{
		AkOSChar szDecodedPathCopy[AK_MAX_PATH];
		AKPLATFORM::SafeStrCpy(szDecodedPathCopy, g_decodedBankPath, AK_MAX_PATH);
		AKPLATFORM::SafeStrCat(szDecodedPathCopy, &AK_PATH_SEPARATOR[0], AK_MAX_PATH);
		AKPLATFORM::SafeStrCat(szDecodedPathCopy, languageOsString, AK_MAX_PATH);
		CreateDirectoryStructure(szDecodedPathCopy);
	}
	return AK::StreamMgr::SetCurrentLanguage(languageOsString);
}

const AkOSChar* GetCurrentLanguage()
{
	return AK::StreamMgr::GetCurrentLanguage();
}

AKRESULT LoadFilePackage(
	const MONOCHAR* in_pszFilePackageName,	// File package name. Location is resolved using base class' Open().
	AkUInt32 &		out_uPackageID)			// Returned package ID.
{
	AkOSChar* osString = NULL;
	CONVERT_MONOCHAR_TO_OSCHAR( in_pszFilePackageName, osString );
	return g_lowLevelIO.LoadFilePackage(osString, out_uPackageID);
}

// Unload a file package.
// Returns AK_Success if in_uPackageID exists, AK_Fail otherwise.
// WARNING: This method is not thread safe. Ensure there are no I/O occurring on this device
// when unloading a file package.
AKRESULT UnloadFilePackage(AkUInt32 in_uPackageID)
{
	return g_lowLevelIO.UnloadFilePackage(in_uPackageID);
}

// Unload all file packages.
// Returns AK_Success;
// WARNING: This method is not thread safe. Ensure there are no I/O occurring on this device
// when unloading a file package.
AKRESULT UnloadAllFilePackages()
{
	return g_lowLevelIO.UnloadAllFilePackages();
}

AKRESULT RegisterGameObjInternal(AkGameObjectID in_GameObj)
{
	if (AK::SoundEngine::IsInitialized())
		return AK::SoundEngine::RegisterGameObj(in_GameObj);
	return AK_Fail;
}

AKRESULT RegisterGameObjInternal_WithName(AkGameObjectID in_GameObj, const MONOCHAR* in_pszObjName)
{
	if (AK::SoundEngine::IsInitialized())
	{
		char* szName;
		CONVERT_MONOCHAR_TO_CHAR(in_pszObjName, szName);
		return AK::SoundEngine::RegisterGameObj(in_GameObj, szName);
	}
	return AK_Fail;
}

AKRESULT UnregisterGameObjInternal(AkGameObjectID in_GameObj)
{
	if (AK::SoundEngine::IsInitialized())
		return AK::SoundEngine::UnregisterGameObj(in_GameObj);
	return AK_Fail;
}

AKRESULT LoadAndDecodeInternal(void* in_BankData, AkUInt32 in_BankDataSize, bool in_bSaveDecodedBank, AkOSChar* in_szDecodedBankName, bool in_bIsLanguageSpecific, AkBankID& out_bankID)
{
	AKRESULT eResult = AK_Fail;

	// Get the decoded size
	AkUInt32 decodedSize = 0;
	void * pDecodedData = NULL;
	eResult = AK::SoundEngine::DecodeBank(in_BankData, in_BankDataSize, AK_DEFAULT_POOL_ID, pDecodedData, decodedSize); // get the decoded size
	if( eResult == AK_Success )
	{
		pDecodedData = malloc(decodedSize);

		// Actually decode the bank
		if( pDecodedData != NULL )
		{
			eResult = AK::SoundEngine::DecodeBank(in_BankData, in_BankDataSize, AK_DEFAULT_POOL_ID, pDecodedData, decodedSize); 
			if( eResult == AK_Success )
			{
				// TODO: We could use the async load bank to Load and Save at the same time.

				// 3- Load the bank from the decoded memory pointer.
				AkBankType BankType;
				eResult = AK::SoundEngine::LoadBankMemoryCopy(pDecodedData, decodedSize, out_bankID, BankType);
					
				// 4- Save the decoded bank to disk. 
				if (in_bSaveDecodedBank)
				{
					AKRESULT eSaveResult = SaveDecodedBank(in_szDecodedBankName, pDecodedData, decodedSize, in_bIsLanguageSpecific);
					if (eSaveResult != AK_Success)
					{
						eResult = eSaveResult;
						AK::Monitor::PostString("Could not save the decoded bank !", AK::Monitor::ErrorLevel_Error);
					}
				}
			}

			free(pDecodedData);
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}

	return eResult;
}
	
void SanitizeBankNameWithoutExtension(const MONOCHAR* in_BankName, AkOSChar* out_SanitizedString)
{
	AkOSChar* tempStr = NULL;
	CONVERT_MONOCHAR_TO_OSCHAR(in_BankName, tempStr);
	AKPLATFORM::SafeStrCpy(out_SanitizedString, tempStr, AK_MAX_PATH - 1);
	out_SanitizedString[AK_MAX_PATH - 1] = '\0';
}

void SanitizeBankName(const MONOCHAR* in_BankName, AkOSChar* out_SanitizedString)
{
	const AkOSChar* ext = AKTEXT(".bnk");
	SanitizeBankNameWithoutExtension(in_BankName, out_SanitizedString);
	AKPLATFORM::SafeStrCat(out_SanitizedString, ext, AK_MAX_PATH - 1);
}

AKRESULT LoadAndDecodeBankFromMemory(void* in_BankData, AkUInt32 in_BankDataSize, bool in_bSaveDecodedBank, const MONOCHAR* in_DecodedBankName, bool in_bIsLanguageSpecific, AkBankID& out_bankID)
{
	AkOSChar osString[AK_MAX_PATH];
	SanitizeBankName(in_DecodedBankName, osString);
	return LoadAndDecodeInternal(in_BankData, in_BankDataSize, in_bSaveDecodedBank, osString, in_bIsLanguageSpecific, out_bankID);
}

AKRESULT LoadAndDecodeBank(const MONOCHAR* in_pszString, bool in_bSaveDecodedBank, AkBankID& out_bankID)
{
	AKRESULT eResult = AK_Fail;
	AkOSChar osString[AK_MAX_PATH];
	SanitizeBankName(in_pszString, osString);

	if( in_bSaveDecodedBank )
	{
		// 1- Open and read the file ourselves using the StreamMgr
		AkFileSystemFlags flags;
		AK::IAkStdStream *	pStream;
		flags.uCompanyID = AKCOMPANYID_AUDIOKINETIC;
		flags.uCodecID = AKCODECID_BANK;
		flags.uCustomParamSize = 0;
		flags.pCustomParam = NULL;
		flags.bIsLanguageSpecific = true;

		eResult = AK::IAkStreamMgr::Get()->CreateStd(
			osString,
			&flags,
			AK_OpenModeRead,
			pStream,
			true );
			
		if( eResult != AK_Success ) // TODO: Verify in the file location resolver
		{
			flags.bIsLanguageSpecific = false; 
			eResult = AK::IAkStreamMgr::Get()->CreateStd(
				osString,
				&flags,
				AK_OpenModeRead,
				pStream,
				true );
		}

		if( eResult == AK_Success )
		{
			AkStreamInfo info;
			pStream->GetInfo( info );
			AkUInt8 * pFileData = (AkUInt8*)malloc((size_t)info.uSize);
			if( pFileData != NULL )
			{
				AkUInt32 outSize;
				eResult = pStream->Read(pFileData, (AkUInt32)info.uSize, true, AK_DEFAULT_PRIORITY, info.uSize/AK_DEFAULT_BANK_THROUGHPUT, outSize);
				if( eResult == AK_Success )
				{
					pStream->Destroy();
					pStream = NULL;

					eResult = LoadAndDecodeInternal(pFileData, outSize, in_bSaveDecodedBank, osString, flags.bIsLanguageSpecific, out_bankID);
				}

				free(pFileData);
				pFileData = NULL;
			}
			else
			{
				eResult = AK_InsufficientMemory;
			}
		}
	}
	else
	{
		// Use the Prepare Bank mechanism. This will decode on load, but not save anything
		eResult = AK::SoundEngine::PrepareBank(AK::SoundEngine::Preparation_LoadAndDecode, osString);

		AkOSChar osBankName[AK_MAX_PATH];
		SanitizeBankNameWithoutExtension(in_pszString, osBankName);
		out_bankID = AK::SoundEngine::GetIDFromString(osBankName);
	}

	return eResult;
}

AKRESULT SetObjectPosition(AkGameObjectID in_GameObjectID, AkVector Pos, AkVector Front, AkVector Top)
{
	if (!AK::SoundEngine::IsInitialized())
		return AK_Fail;

	AkTransform transform;
	transform.Set(Pos, Front, Top);
	return AK::SoundEngine::SetPosition(in_GameObjectID, transform);
}

AKRESULT GetSourceMultiplePlayPositions(
	AkPlayingID		in_PlayingID,				///< Playing ID returned by AK::SoundEngine::PostEvent()
	AkUniqueID *	out_audioNodeID,			///< Audio Node IDs of sources associated with the specified playing ID. Indexes in this array match indexes in out_msTime.
	AkUniqueID *	out_mediaID,				///< Media ID of playing item. (corresponds to 'ID' attribute of 'File' element in SoundBank metadata file)
	AkTimeMs *		out_msTime,					///< Audio positions of sources associated with the specified playing ID. Indexes in this array match indexes in out_audioNodeID.
	AkUInt32 *		io_pcPositions,				///< Number of entries in out_puPositions. Needs to be set to the size of the array: it is adjusted to the actual number of returned entries
	bool			in_bExtrapolate				///< Position is extrapolated based on time elapsed since last sound engine update
	)
{
	if (*io_pcPositions == 0)
	{
		return AK_Fail;
	}

	AkSourcePosition * sourcePositionInfo = (AkSourcePosition*)AkAlloca((*io_pcPositions) * sizeof(AkSourcePosition));
	if (!sourcePositionInfo)
	{
		return AK_Fail;
	}

	AKRESULT res = AK::SoundEngine::GetSourcePlayPositions(in_PlayingID, sourcePositionInfo, io_pcPositions, in_bExtrapolate);

	for (AkUInt32 i = 0; i < *io_pcPositions; ++i)
	{
		out_audioNodeID[i] = sourcePositionInfo[i].audioNodeID;
		out_mediaID[i] = sourcePositionInfo[i].mediaID;
		out_msTime[i] = sourcePositionInfo[i].msTime;
	}

	return res;
}

AKRESULT SetListeners(AkGameObjectID in_emitterGameObj, AkGameObjectID* in_pListenerGameObjs, AkUInt32 in_uNumListeners)
{
	return AK::SoundEngine::SetListeners(in_emitterGameObj, in_pListenerGameObjs, in_uNumListeners);
}

AKRESULT SetDefaultListeners(AkGameObjectID* in_pListenerObjs, AkUInt32 in_uNumListeners)
{
	return AK::SoundEngine::SetDefaultListeners(in_pListenerObjs, in_uNumListeners);
}
	
AkUInt32 GetNumOutputDevices(AkUniqueID in_AudioDeviceShareSetID)
{
	AkUInt32 NumDevices = 0;
	if (AK::SoundEngine::IsInitialized()) 
	{
		AKRESULT res = AK::SoundEngine::GetDeviceList(in_AudioDeviceShareSetID, NumDevices, nullptr);
	}
	else 
	{
		AKPLATFORM::OutputDebugMsg("Wwise warning in AK::SoundEngine::GetDeviceList(AkUniqueID,AkUInt32 &,AkDeviceDescription *): AkInitializer.cs Awake() was not executed yet. Set the Script Execution Order properly so the current call is executed after.");
	}

	return NumDevices;
}
AKRESULT AddOutput(const AkOutputSettings & in_Settings, AkOutputDeviceID *out_pDeviceID, AkGameObjectID* in_pListenerIDs, AkUInt32 in_uNumListeners)
{
	return AK::SoundEngine::AddOutput(in_Settings, out_pDeviceID, in_pListenerIDs, in_uNumListeners);
}

void GetDefaultStreamSettings(AkStreamMgrSettings & out_settings)
{
	AK::StreamMgr::GetDefaultSettings(out_settings);
}

void GetDefaultDeviceSettings(AkDeviceSettings & out_settings)
{
	AK::StreamMgr::GetDefaultDeviceSettings(out_settings);
}

void GetDefaultMusicSettings(AkMusicSettings &out_settings)
{
	AK::MusicEngine::GetDefaultInitSettings(out_settings);
}

void GetDefaultInitSettings(AkInitSettings & out_settings)
{
	AK::SoundEngine::GetDefaultInitSettings(out_settings);
}

void GetDefaultPlatformInitSettings(AkPlatformInitSettings &out_settings)
{
	AK::SoundEngine::GetDefaultPlatformInitSettings(out_settings);
}

AkUInt32 GetMajorMinorVersion()
{
	return (AK_WWISESDK_VERSION_MAJOR << 16) | AK_WWISESDK_VERSION_MINOR;
}

AkUInt32 GetSubminorBuildVersion()
{
	return (AK_WWISESDK_VERSION_SUBMINOR << 16) | AK_WWISESDK_VERSION_BUILD;
}

void StartResourceMonitoring()
{
	ResourceMonitoring::ClearDataSummary();
	AK::SoundEngine::RegisterResourceMonitorCallback(&ResourceMonitoring::Callback);
}

void GetResourceMonitorDataSummary(AkResourceMonitorDataSummary& resourceMonitorDataSummary)
{
	ResourceMonitoring::GetResourceMonitorDataSummary(resourceMonitorDataSummary);
}

void StopResourceMonitoring()
{
	AK::SoundEngine::UnregisterResourceMonitorCallback(&ResourceMonitoring::Callback);
	ResourceMonitoring::ClearDataSummary();
}

void ClearCaptureData()
{
	Capture::ClearCaptureData();
}

void StartDeviceCapture(AkOutputDeviceID in_idOutput)
{
	Capture::GetDeviceCaptureData(in_idOutput).Register();
}

void StopDeviceCapture(AkOutputDeviceID in_idOutput)
{
	if (auto deviceCaptureData = Capture::TryGetDeviceCaptureData(in_idOutput))
		deviceCaptureData->Unregister();
}

AkUInt32 UpdateCaptureSampleCount(AkOutputDeviceID in_idOutput)
{
	auto deviceCaptureData = Capture::TryGetDeviceCaptureData(in_idOutput);
	return deviceCaptureData ? deviceCaptureData->UpdateSampleCount() : 0;
}

AkUInt32 GetCaptureSamples(AkOutputDeviceID in_idOutput, AkReal32* out_pSamples, AkUInt32 in_uBufferSize)
{
	auto deviceCaptureData = Capture::TryGetDeviceCaptureData(in_idOutput);
	return deviceCaptureData ? deviceCaptureData->GetSamples(out_pSamples, in_uBufferSize) : 0;
}

AKRESULT SetRoomPortal(AkPortalID in_PortalID, AkRoomID FrontRoom, AkRoomID BackRoom, const AkTransform& Transform, const AkExtent& Extent, bool bEnabled, const char* in_pName)
{
	AkPortalParams portalParams;
	portalParams.FrontRoom = FrontRoom;
	portalParams.BackRoom = BackRoom;
	portalParams.Transform = Transform;
	portalParams.Extent = Extent;
	portalParams.bEnabled = bEnabled;

	return AK::SpatialAudio::SetPortal(in_PortalID, portalParams, in_pName);
}

AKRESULT SetRoom(AkRoomID in_RoomID, AkRoomParams& in_roomParams,AkGeometryInstanceID GeometryInstanceID, const char* in_pName)
{
	AkRoomParams roomParams;
	roomParams = in_roomParams;
	roomParams.GeometryInstanceID = GeometryInstanceID;

	return AK::SpatialAudio::SetRoom(in_RoomID, roomParams, in_pName);
}

AKRESULT RegisterSpatialAudioListener(AkGameObjectID in_gameObjectID)
{
	return AK::SpatialAudio::RegisterListener(in_gameObjectID);
}

AKRESULT UnregisterSpatialAudioListener(AkGameObjectID in_gameObjectID)
{
	return AK::SpatialAudio::UnregisterListener(in_gameObjectID);
}

AKRESULT SetGeometry(AkGeometrySetID in_GeomSetID,
	AkTriangle* Triangles,
	AkUInt32 NumTriangles,
	AkVertex* Vertices,
	AkUInt32 NumVertices,
	AkAcousticSurface* Surfaces,
	AkUInt32 NumSurfaces,
	bool EnableDiffraction,
	bool EnableDiffractionOnBoundaryEdges,
	bool EnableTriangles)
{
	AkGeometryParams params;
	params.Triangles = Triangles;
	params.NumTriangles = (AkTriIdx)NumTriangles;
	params.Vertices = Vertices;
	params.NumVertices = (AkVertIdx)NumVertices;
	params.Surfaces = Surfaces;
	params.NumSurfaces = (AkSurfIdx)NumSurfaces;
	params.EnableDiffraction = EnableDiffraction;
	params.EnableDiffractionOnBoundaryEdges = EnableDiffractionOnBoundaryEdges;
	params.EnableTriangles = EnableTriangles;

	return AK::SpatialAudio::SetGeometry(in_GeomSetID, params);
}

AKRESULT SetGeometryInstance(
	AkGeometryInstanceID in_GeomInstanceID,
	const AkTransform& Transform,
	const AkVector& Scale,
	AkGeometrySetID GeometrySetID,
	AkRoomID RoomID)
{
	AkGeometryInstanceParams params;
	params.PositionAndOrientation = Transform;
	params.Scale = Scale;
	params.GeometrySetID = GeometrySetID;
	params.RoomID = RoomID;

	return AK::SpatialAudio::SetGeometryInstance(in_GeomInstanceID, params);
}

AkPlayingID PostEventOnRoom(
	AkUniqueID in_eventID,
	AkRoomID in_roomID,
	AkUInt32 in_uFlags,
	AkCallbackFunc in_pfnCallback,
	void * in_pCookie,
	AkUInt32 in_cExternals,
	AkExternalSourceInfo *in_pExternalSources,
	AkPlayingID	in_PlayingID)
{
	return AK::SoundEngine::PostEvent(in_eventID, in_roomID, in_uFlags, in_pfnCallback, in_pCookie, in_cExternals, in_pExternalSources, in_PlayingID);
}

AkPlayingID PostEventOnRoom(
	const MONOCHAR* in_pszEventName,
	AkRoomID in_roomID,
	AkUInt32 in_uFlags,
	AkCallbackFunc in_pfnCallback,
	void * in_pCookie,
	AkUInt32 in_cExternals,
	AkExternalSourceInfo *in_pExternalSources,
	AkPlayingID	in_PlayingID)
{
	return AK::SoundEngine::PostEvent(in_pszEventName, in_roomID, in_uFlags, in_pfnCallback, in_pCookie, in_cExternals, in_pExternalSources, in_PlayingID);
}

AKRESULT SaveDecodedBank(AkOSChar * in_OrigBankFile, void * in_pDecodedData, AkUInt32 in_decodedSize, bool in_bIsLanguageSpecific)
{
	AkFileSystemFlags flags;
	AK::IAkStdStream *	pStream;
	flags.uCompanyID = AKCOMPANYID_AUDIOKINETIC;
	flags.uCodecID = AKCODECID_BANK;
	flags.uCustomParamSize = 0;
	flags.pCustomParam = NULL;
	flags.bIsLanguageSpecific = in_bIsLanguageSpecific;

	AKRESULT eResult = AK::IAkStreamMgr::Get()->CreateStd(
		in_OrigBankFile,
		&flags,
		AK_OpenModeWrite,
		pStream,
		true);

	if (eResult == AK_Success)
	{
		AkUInt32 outSize = 0;
		eResult = pStream->Write(in_pDecodedData, in_decodedSize, true, AK_DEFAULT_PRIORITY, in_decodedSize / AK_DEFAULT_BANK_THROUGHPUT, outSize);

		pStream->Destroy();
		pStream = NULL;
	}

	return eResult;
}

AKRESULT QueryReflectionPaths(AkGameObjectID in_gameObjectID, AkUInt32 in_positionIndex, AkVector& out_listenerPos, AkVector& out_emitterPos, AkReflectionPathInfo* out_aPaths, AkUInt32& io_uArraySize)
{
	AkVector64 listenerPosDbl, emitterPosDbl;
	listenerPosDbl.Zero();
	emitterPosDbl.Zero();

	AKRESULT result = AK::SpatialAudio::QueryReflectionPaths(in_gameObjectID, in_positionIndex, listenerPosDbl, emitterPosDbl, out_aPaths, io_uArraySize);

	out_listenerPos.X = (float)listenerPosDbl.X;
	out_listenerPos.Y = (float)listenerPosDbl.Y;
	out_listenerPos.Z = (float)listenerPosDbl.Z;

	out_emitterPos.X = (float)emitterPosDbl.X;
	out_emitterPos.Y = (float)emitterPosDbl.Y;
	out_emitterPos.Z = (float)emitterPosDbl.Z;

	return result;
}

AKRESULT QueryDiffractionPaths(AkGameObjectID in_gameObjectID, AkUInt32 in_positionIndex, AkVector& out_listenerPos, AkVector& out_emitterPos, AkDiffractionPathInfo* out_aPaths, AkUInt32& io_uArraySize)
{
	AkVector64 listenerPosDbl, emitterPosDbl;
	listenerPosDbl.Zero();
	emitterPosDbl.Zero();

	AKRESULT result = AK::SpatialAudio::QueryDiffractionPaths(in_gameObjectID, in_positionIndex, listenerPosDbl, emitterPosDbl, out_aPaths, io_uArraySize);

	out_listenerPos.X = (float)listenerPosDbl.X;
	out_listenerPos.Y = (float)listenerPosDbl.Y;
	out_listenerPos.Z = (float)listenerPosDbl.Z;

	out_emitterPos.X = (float)emitterPosDbl.X;
	out_emitterPos.Y = (float)emitterPosDbl.Y;
	out_emitterPos.Z = (float)emitterPosDbl.Z;

	return result;
}

void PerformStreamMgrIO()
{
	AKVERIFY(AK_Success == AK::StreamMgr::PerformIO());
}
