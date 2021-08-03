module;
#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <fstream>
#include <filesystem>
export module AudioPlayer;

#if _DEBUG
#define ASSERT_RESULT(res) if(res != S_OK) throw
#else
#define TEST_RESULT(res)
#endif

export struct AudioData
{
	const char* ptr;
	const unsigned int length;
};

export class AudioPlayer
{
	public:
	AudioPlayer()
	{
		Setup();
	}

	~AudioPlayer()
	{
		renderer->Release();
		client->Release();
	}

	private:
	struct Format
	{
		WORD channels = 2;
		WORD bits = 16;
		DWORD sampleRate = 48000;

		inline const WORD GetBlockAlign() const noexcept
		{
			return (channels * bits) / 8;
		}
	} format;

	inline static constexpr REFERENCE_TIME refTimesPerSec = 10000000;
	inline static constexpr REFERENCE_TIME refTimesPerMs = 10000;

	inline static constexpr const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	inline static constexpr const IID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	inline static constexpr const IID IID_IAudioClient = __uuidof(IAudioClient);
	inline static constexpr const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

	IAudioClient* client;
	IAudioRenderClient* renderer;

	IMMDevice* GetDevice()
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

	void Setup()
	{
		HRESULT result;

		result = CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY);
		ASSERT_RESULT(result);

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
			format.bits,
			0
		};

		WAVEFORMATEX* altFormat;
		result = client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &dataFormat, &altFormat);
		ASSERT_RESULT(result);

		constexpr double bufferSecondDuration = 10;
		result = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, bufferSecondDuration * refTimesPerSec, 0, &dataFormat, NULL);
		ASSERT_RESULT(result);

		UINT32 bufSize;
		result = client->GetBufferSize(&bufSize);
		ASSERT_RESULT(result);

		result = client->GetService(IID_IAudioRenderClient, (void**)&renderer);
		ASSERT_RESULT(result);

		device->Release();
	}

	void WriteSilence(UINT32 silenceFrames = 2)
	{
		HRESULT result;
		BYTE* stream = nullptr;
		result = renderer->GetBuffer(silenceFrames, &stream);
		ASSERT_RESULT(result);
		result = renderer->ReleaseBuffer(silenceFrames, AUDCLNT_BUFFERFLAGS_SILENT);
		ASSERT_RESULT(result);
	}

	void Write(AudioData data)
	{
		HRESULT result;

		const auto blockAlign = format.GetBlockAlign();

		UINT32 bufferFrameSize;
		result = client->GetBufferSize(&bufferFrameSize);
		ASSERT_RESULT(result);

		result = client->Start();
		ASSERT_RESULT(result);

		const auto bufferRefSecDuration = refTimesPerSec * bufferFrameSize / format.sampleRate;
		const auto dataFrames = data.length / blockAlign;

		WriteSilence(4);
		unsigned int writtenFrames = 0;
		BYTE* out;
		while (writtenFrames < dataFrames)
		{
			UINT32 existingFrames;
			result = client->GetCurrentPadding(&existingFrames);
			ASSERT_RESULT(result);

			const UINT32 availableFrames = bufferFrameSize - existingFrames;

			result = renderer->GetBuffer(availableFrames, &out);
			ASSERT_RESULT(result);

			const auto writtenBytes = writtenFrames * blockAlign;
			const auto availableBytes = availableFrames * blockAlign;
			std::copy_n(data.ptr + writtenBytes, availableBytes, out);
			result = renderer->ReleaseBuffer(availableFrames, 0);
			ASSERT_RESULT(result);

			writtenFrames += availableFrames;
			Sleep(bufferRefSecDuration / refTimesPerMs);
		}

		result = client->Stop();
		ASSERT_RESULT(result);
	}

	public:
	void Play(AudioData data)
	{
		Write(data);
	}

	void Play(const char* file)
	{
		if (!std::filesystem::exists(file))
			throw "File does not exist";

		std::ifstream fs(file, std::ifstream::binary);
		const unsigned int fsLength = fs.seekg(0, fs.end).tellg();
		fs.seekg(0, fs.beg);

		char* data = new char[fsLength];
		fs.read(data, fsLength);
		fs.close();
		Write({ data, fsLength });
		delete[] data;
	}
};