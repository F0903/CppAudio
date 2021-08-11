module;
#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <algorithm>

#ifdef _DEBUG
#include <chrono>
#include <iostream>
#endif
export module AudioWriter;
import AudioInfo;
import AudioData;
import AudioUtils;

export class AudioWriter
{
	public:
	AudioWriter(const AudioInfo& format) noexcept : format(format)
	{
		Setup();
	}

	~AudioWriter() noexcept
	{
		renderer->Release();
		client->Release();

		if (comInit) COMUninit();
	}

	private:
	inline static constexpr REFERENCE_TIME refTimesPerSec = 10000000;
	inline static constexpr REFERENCE_TIME refTimesPerMs = 10000;

	inline static constexpr const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	inline static constexpr const IID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	inline static constexpr const IID IID_IAudioClient = __uuidof(IAudioClient);
	inline static constexpr const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

	inline static bool comInit = false;

	IAudioClient* client;
	IAudioRenderClient* renderer;
	const AudioInfo& format;

	private:
	static void COMInit() noexcept
	{
		HRESULT result;
		result = CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY);
		DebugAssertResult(result);
		comInit = true;
	}

	static void COMUninit() noexcept
	{
		CoUninitialize();
		comInit = false;
	}

	void Setup() const noexcept
	{
		//TEST REMOVE
		DebugAssertResult(S_FALSE);

		if (!comInit) COMInit();

		HRESULT result;

		auto device = GetDevice();
		result = device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&client);
		DebugAssertResult(result);

		const auto blockAlign = format.GetBlockAlign();
		DWORD avgByteRate = blockAlign * format.sampleRate;

		WAVEFORMATEX dataFormat =
		{
			WAVE_FORMAT_PCM,
			format.channels,
			format.sampleRate,
			avgByteRate,
			blockAlign,
			format.bitDepth,
			0
		};

		WAVEFORMATEX* altFormat;
		result = client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &dataFormat, &altFormat);
		DebugAssertResult(result);

		constexpr double bufferSecondDuration = 20.0 / 1000.0;
		result = client->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			static_cast<REFERENCE_TIME>(bufferSecondDuration * refTimesPerSec),
			0,
			&dataFormat,
			NULL);
		DebugAssertResult(result);

		UINT32 bufSize;
		result = client->GetBufferSize(&bufSize);
		DebugAssertResult(result);

		result = client->GetService(IID_IAudioRenderClient, (void**)&renderer);
		DebugAssertResult(result);

		device->Release();
	}

	IMMDevice* GetDevice() const noexcept
	{
		HRESULT result;

		IMMDeviceEnumerator* devices = nullptr;
		result = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&devices);
		DebugAssertResult(result);

		IMMDevice* device = nullptr;
		result = devices->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eMultimedia, &device);
		DebugAssertResult(result);
		devices->Release();

		return device;
	}

	void WriteSilence(const WORD blockAlign, const UINT32 silenceFrames = 2) const noexcept
	{
		HRESULT result;
		BYTE* stream = nullptr;
		result = renderer->GetBuffer(silenceFrames, &stream);
		DebugAssertResult(result);
		result = renderer->ReleaseBuffer(silenceFrames, AUDCLNT_BUFFERFLAGS_SILENT);
		DebugAssertResult(result);
	}

	public:
	bool Write(const AudioData& data) const noexcept
	{
		HRESULT result;

		const auto blockAlign = format.GetBlockAlign();

		UINT32 bufferFrameSize;
		result = client->GetBufferSize(&bufferFrameSize);
		DebugAssertResult(result);

		const auto bufferRefSecDuration = refTimesPerSec * bufferFrameSize / format.sampleRate;

		const auto bufferReady = CreateEvent(NULL, FALSE, FALSE, NULL);
		const auto bufferReadyTimeout = bufferRefSecDuration * 4;
		client->SetEventHandle(bufferReady);

		WriteSilence(blockAlign, 4);
		result = client->Start();
		DebugAssertResult(result);

		#ifdef _DEBUG
		std::chrono::high_resolution_clock timer = std::chrono::high_resolution_clock();
		#endif

		unsigned int writtenBytes = 0;
		BYTE* out;
		while (writtenBytes < data.length)
		{
			#ifdef _DEBUG
			const auto startTime = timer.now();
			#endif

			const auto waitRes = WaitForSingleObject(bufferReady, static_cast<DWORD>(bufferReadyTimeout));
			if (waitRes != WAIT_OBJECT_0)
			{
				if (waitRes == WAIT_ABANDONED || waitRes == WAIT_TIMEOUT)
					return true;
				return false;
			}

			UINT32 existingFrames;
			result = client->GetCurrentPadding(&existingFrames);
			DebugAssertResult(result);

			const UINT32 availableFrames = bufferFrameSize - existingFrames;
			const UINT32 availableBytes = availableFrames * blockAlign;
			UINT32 framesToRequest = availableFrames;
			UINT32 byteCount = availableBytes;

			const auto nextPos = writtenBytes + availableBytes;
			if (nextPos > data.length)
			{
				byteCount = static_cast<UINT32>(data.length - writtenBytes);
				framesToRequest = byteCount / blockAlign;
			}

			result = renderer->GetBuffer(framesToRequest, &out);
			DebugAssertResult(result);

			std::copy_n(data.ptr + writtenBytes, byteCount, out);

			result = renderer->ReleaseBuffer(framesToRequest, 0);
			DebugAssertResult(result);

			writtenBytes += byteCount;

			#ifdef _DEBUG
			const auto endTime = timer.now();
			const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
			std::cout << "Audio pass took " << duration << std::endl;
			#endif
		}

		WriteSilence(blockAlign, 4);
		Sleep(static_cast<DWORD>(bufferRefSecDuration / refTimesPerMs));
		result = client->Stop();
		DebugAssertResult(result);
		return true;
	}
};