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

#if _DEBUG
#define ASSERT_RESULT(res) if(res != S_OK) throw
#else
#define ASSERT_RESULT(res)
#endif

export class AudioWriter
{
	public:
	AudioWriter(const AudioInfo& format) : format(format)
	{
		Setup();
	}

	~AudioWriter()
	{
		renderer->Release();
		client->Release();
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
	static void COMInit()
	{
		HRESULT result;
		result = CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY);
		ASSERT_RESULT(result);
		comInit = true;
	}

	void Setup()
	{
		if (!comInit) COMInit();

		HRESULT result;

		auto device = GetDevice();
		result = device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&client);
		ASSERT_RESULT(result);

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
		ASSERT_RESULT(result);

		constexpr double bufferSecondDuration = 20.0 / 1000.0;
		result = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, bufferSecondDuration * refTimesPerSec, 0, &dataFormat, NULL);
		ASSERT_RESULT(result);

		UINT32 bufSize;
		result = client->GetBufferSize(&bufSize);
		ASSERT_RESULT(result);

		result = client->GetService(IID_IAudioRenderClient, (void**)&renderer);
		ASSERT_RESULT(result);

		device->Release();
	}

	IMMDevice* GetDevice() const
	{
		HRESULT result;

		IMMDeviceEnumerator* devices = nullptr;
		result = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&devices);
		ASSERT_RESULT(result);

		IMMDevice* device = nullptr;
		result = devices->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eMultimedia, &device);
		ASSERT_RESULT(result);
		devices->Release();

		return device;
	}

	void WriteSilence(const WORD blockAlign, const UINT32 silenceFrames = 2) const
	{
		HRESULT result;
		BYTE* stream = nullptr;
		result = renderer->GetBuffer(silenceFrames, &stream);
		ASSERT_RESULT(result);
		result = renderer->ReleaseBuffer(silenceFrames, AUDCLNT_BUFFERFLAGS_SILENT);
		ASSERT_RESULT(result);
	}

	public:
	void Write(const AudioData& data) const
	{
		HRESULT result;

		const auto blockAlign = format.GetBlockAlign();

		UINT32 bufferFrameSize;
		result = client->GetBufferSize(&bufferFrameSize);
		ASSERT_RESULT(result);

		const auto bufferRefSecDuration = refTimesPerSec * bufferFrameSize / format.sampleRate;

		const auto bufferReady = CreateEvent(NULL, FALSE, FALSE, NULL);
		const auto bufferReadyTimeout = bufferRefSecDuration * 4;
		client->SetEventHandle(bufferReady);

		WriteSilence(blockAlign, 8);
		result = client->Start();
		ASSERT_RESULT(result);

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

			const auto waitRes = WaitForSingleObject(bufferReady, bufferReadyTimeout);
			if (waitRes != WAIT_OBJECT_0)
			{
				if (waitRes == WAIT_ABANDONED || waitRes == WAIT_TIMEOUT)
					return;
				throw;
			}

			UINT32 existingFrames;
			result = client->GetCurrentPadding(&existingFrames);
			ASSERT_RESULT(result);

			const UINT32 availableFrames = bufferFrameSize - existingFrames;
			const UINT32 availableBytes = availableFrames * blockAlign;
			UINT32 framesToRequest = availableFrames;
			UINT32 byteCount = availableBytes;

			const auto nextPos = writtenBytes + availableBytes;
			if (nextPos > data.length)
			{
				byteCount = data.length - writtenBytes;
				framesToRequest = byteCount / blockAlign;
			}

			result = renderer->GetBuffer(framesToRequest, &out);
			ASSERT_RESULT(result);

			std::copy_n(data.ptr + writtenBytes, byteCount, out);

			result = renderer->ReleaseBuffer(framesToRequest, 0);
			ASSERT_RESULT(result);

			writtenBytes += byteCount;

			#ifdef _DEBUG
			const auto endTime = timer.now();
			const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
			std::cout << "Audio pass took " << duration << std::endl;
			#endif
	}

		WriteSilence(blockAlign, 8);
		Sleep(bufferRefSecDuration / refTimesPerMs);
		result = client->Stop();
		ASSERT_RESULT(result);
}
};